#ifndef _AM_SYNCHRONIZATION_H
#define _AM_SYNCHRONIZATION_H

#include <3ds/types.h>
#include <3ds/ipc.h>
#include <3ds/svc.h>

typedef s32 LightLock;

typedef struct __attribute__((aligned(4))) RecursiveLock
{
	LightLock lock;
 	ThreadLocalStorage *thread_tag;
 	u32 counter;
} RecursiveLock;

typedef struct __attribute__((aligned(4))) LightEvent
{
	s32 state;      ///< State of the event: -2=cleared sticky, -1=cleared oneshot, 0=signaled oneshot, 1=signaled sticky
	LightLock lock; ///< Lock used for sticky timer operation
} LightEvent;

enum
{
	CLEARED_STICKY = -2,
	CLEARED_ONESHOT = -1,
	SIGNALED_ONESHOT = 0,
	SIGNALED_STICKY = 1
};

Result syncInit();
void syncExit();

static inline void __dmb(void)
{
	__asm__ __volatile__("mcr p15, 0, %[val], c7, c10, 5" :: [val] "r" (0) : "memory");
}


static inline void __clrex(void)
{
	__asm__ __volatile__("clrex" ::: "memory");
}


static inline s32 __ldrex(s32 *addr)
{
	s32 val;
	__asm__ __volatile__("ldrex %[val], %[addr]" : [val] "=r" (val) : [addr] "Q" (*addr));
	return val;
}


static inline bool __strex(s32 *addr, s32 val)
{
	bool res;
	__asm__ __volatile__("strex %[res], %[val], %[addr]" : [res] "=&r" (res) : [val] "r" (val), [addr] "Q" (*addr));
	return res;
}

static inline u8 __ldrexb(u8 *addr)
{
	u8 val;
	__asm__ __volatile__("ldrexb %[val], %[addr]" : [val] "=r" (val) : [addr] "Q" (*addr));
	return val;
}

static inline bool __strexb(u8 *addr, u8 val)
{
	bool res;
	__asm__ __volatile__("strexb %[res], %[val], %[addr]" : [res] "=&r" (res) : [val] "r" (val), [addr] "Q" (*addr));
	return res;
}

void LightLock_Init(LightLock *lock);
void LightLock_Lock(LightLock *lock);
void LightLock_Unlock(LightLock *lock);

void RecursiveLock_Init(RecursiveLock *lock);
void RecursiveLock_Lock(RecursiveLock *lock);
void RecursiveLock_Unlock(RecursiveLock *lock);

void LightEvent_Signal(LightEvent* event);
void LightEvent_Wait(LightEvent* event);

#endif