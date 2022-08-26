#ifndef _AM_3DS_SVC_H
#define _AM_3DS_SVC_H

#include <3ds/types.h>

typedef enum
{
	ARBITRATION_SIGNAL                                  = 0, ///< Signal #value threads for wake-up.
	ARBITRATION_WAIT_IF_LESS_THAN                       = 1, ///< If the memory at the address is strictly lower than #value, then wait for signal.
	ARBITRATION_DECREMENT_AND_WAIT_IF_LESS_THAN         = 2, ///< If the memory at the address is strictly lower than #value, then decrement it and wait for signal.
	ARBITRATION_WAIT_IF_LESS_THAN_TIMEOUT               = 3, ///< If the memory at the address is strictly lower than #value, then wait for signal or timeout.
	ARBITRATION_DECREMENT_AND_WAIT_IF_LESS_THAN_TIMEOUT = 4, ///< If the memory at the address is strictly lower than #value, then decrement it and wait for signal or timeout.
} ArbitrationType;

typedef enum
{
	RESLIMIT_COMMIT         = 1,        ///< Quantity of allocatable memory
} ResourceLimitType;

typedef enum
{
	MEMPERM_READ     = 1,          ///< Readable
	MEMPERM_WRITE    = 2,          ///< Writable
} MemPerm;

typedef enum
{
	USERBREAK_PANIC         = 0, ///< Panic.
} UserBreakType;

typedef enum
{
	MEMOP_FREE    = 1, ///< Memory un-mapping
	MEMOP_ALLOC   = 3, ///< Memory mapping
} MemOp;

typedef enum {
	RESET_ONESHOT = 0,
	RESET_STICKY  = 1,
	RESET_PULSE   = 2,
} ResetType;

Result svcCreateThread(Handle *thread, void (* entrypoint)(void *), void *arg, void *stack_top, s32 thread_priority, s32 processor_id);
Result svcControlMemory(void **addr_out, void *addr0, void *addr1, u32 size, MemOp op, MemPerm perm);
Result svcGetResourceLimitCurrentValues(s64 *values, Handle resourceLimit, ResourceLimitType *names, s32 nameCount);
Result svcGetResourceLimitLimitValues(s64 *values, Handle resourceLimit, ResourceLimitType *names, s32 nameCount);
Result svcGetResourceLimit(Handle *resourceLimit, Handle process);
Result svcConnectToPort(volatile Handle *out, const char *portName);
Result svcCloseHandle(Handle handle);
Result svcWaitSynchronizationN(s32 *out, const Handle *handles, s32 handles_num, bool wait_all, s64 nanoseconds);
Result svcReplyAndReceive(s32 *index, const Handle *handles, s32 handleCount, Handle replyTarget);
Result svcAcceptSession(Handle *session, Handle port);
Result svcWaitSynchronization(Handle handle, s64 nanoseconds);
Result svcSendSyncRequest(Handle session);
Result svcGetProcessId(u32 *id, Handle process);
void   svcBreak(UserBreakType breakReason);
void   svcSleepThread(u64 nanoseconds);
Result svcCreateAddressArbiter(Handle *arbiter);
Result svcArbitrateAddressNoTimeout(Handle arbiter, u32 addr, ArbitrationType type, s32 value);
Result svcCreateSessionToPort(Handle *client_session, Handle client_port);
Result svcCreatePort(Handle *port_server, Handle *port_client, const char* name, s32 max_sessions);
Result svcCreateMutex(Handle* mutex, bool initially_locked);
Result svcCreateEvent(Handle* event, ResetType reset_type);
Result svcSignalEvent(Handle handle);
#ifndef RELEASE
Result svcOutputDebugString(char *str, s32 length);
#endif

#endif