#include <3ds/synchronization.h>

Handle GLOBAL_AddressArbiter;

inline Result syncInit()
{
	return svcCreateAddressArbiter(&GLOBAL_AddressArbiter);
}

inline void syncExit()
{
	svcCloseHandle(GLOBAL_AddressArbiter);
}

Result syncArbitrateAddress(s32 *addr, ArbitrationType type, s32 value)
{
	return svcArbitrateAddressNoTimeout(GLOBAL_AddressArbiter, (u32)addr, type, value);
}

void LightLock_Init(LightLock *lock)
{
	do
		__ldrex(lock);
	while (__strex(lock, 1));
}

void LightLock_Lock(LightLock *lock)
{
	s32 val;
	bool bAlreadyLocked;

	// Try to lock, or if that's not possible, increment the number of waiting threads
	do
	{
		// Read the current lock state
		val = __ldrex(lock);
		if (val == 0) val = 1; // 0 is an invalid state - treat it as 1 (unlocked)
		bAlreadyLocked = val < 0;

		// Calculate the desired next state of the lock
		if (!bAlreadyLocked)
			val = -val; // transition into locked state
		else
			--val; // increment the number of waiting threads (which has the sign reversed during locked state)
	} while (__strex(lock, val));

	// While the lock is held by a different thread:
	while (bAlreadyLocked)
	{
		// Wait for the lock holder thread to wake us up
		syncArbitrateAddress(lock, ARBITRATION_WAIT_IF_LESS_THAN, 0);

		// Try to lock again
		do
		{
			// Read the current lock state
			val = __ldrex(lock);
			bAlreadyLocked = val < 0;

			// Calculate the desired next state of the lock
			if (!bAlreadyLocked)
				val = -(val-1); // decrement the number of waiting threads *and *transition into locked state
			else
			{
				// Since the lock is still held, we need to cancel the atomic update and wait again
				__clrex();
				break;
			}
		} while (__strex(lock, val));
	}

	__dmb();
}

void LightLock_Unlock(LightLock *lock)
{
	__dmb();

	s32 val;
	do
		val = -__ldrex(lock);
	while (__strex(lock, val));

	if (val > 1)
		// Wake up exactly one thread
		syncArbitrateAddress(lock, ARBITRATION_SIGNAL, 1);
}

void RecursiveLock_Init(RecursiveLock *lock)
{
	LightLock_Init(&lock->lock);
	lock->thread_tag = 0;
	lock->counter = 0;
}

void RecursiveLock_Lock(RecursiveLock *lock)
{
	ThreadLocalStorage *tag = getThreadLocalStorage();
	if (lock->thread_tag != tag)
	{
		LightLock_Lock(&lock->lock);
		lock->thread_tag = tag;
	}
	lock->counter ++;
}

void RecursiveLock_Unlock(RecursiveLock *lock)
{
	if (!--lock->counter)
	{
		lock->thread_tag = 0;
		LightLock_Unlock(&lock->lock);
	}
}

static inline int LightEvent_TryReset(LightEvent* event)
{
	__dmb();
	do
	{
		if (__ldrex(&event->state))
		{
			__clrex();
			return 0;
		}
	} while (__strex(&event->state, CLEARED_ONESHOT));
	__dmb();
	return 1;
}

void LightEvent_Wait(LightEvent* event)
{
	while (true)
	{
		if (event->state == CLEARED_STICKY)
		{
			syncArbitrateAddress(&event->state, ARBITRATION_WAIT_IF_LESS_THAN, SIGNALED_ONESHOT);
			return;
		}

		if (event->state != CLEARED_ONESHOT)
		{
			if (event->state == SIGNALED_STICKY)
				return;
			if (event->state == SIGNALED_ONESHOT && LightEvent_TryReset(event))
				return;
		}
		syncArbitrateAddress(&event->state, ARBITRATION_WAIT_IF_LESS_THAN, SIGNALED_ONESHOT);
	}
}

static inline void LightEvent_SetState(LightEvent* event, int state)
{
	do
		__ldrex(&event->state);
	while (__strex(&event->state, state));
}

void LightEvent_Signal(LightEvent* event)
{
	if (event->state == CLEARED_ONESHOT)
	{
		__dmb();
		LightEvent_SetState(event, SIGNALED_ONESHOT);
		syncArbitrateAddress(&event->state, ARBITRATION_SIGNAL, 1);
	}
	else if (event->state == CLEARED_STICKY)
	{
		LightLock_Lock(&event->lock);
		LightEvent_SetState(event, SIGNALED_STICKY);
		syncArbitrateAddress(&event->state, ARBITRATION_SIGNAL, -1);
		LightLock_Unlock(&event->lock);
	}
}