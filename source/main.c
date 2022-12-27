#include <3ds/synchronization.h>
#include <am/globals.h>
#include <3ds/result.h>
#include <am/demodb.h>
#include <allocator.h>
#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/srv.h>
#include <am/pipe.h>
#include <3ds/am9.h>
#include <am/ipc.h>
#include <am/cia.h>
#include <3ds/fs.h>
#include <memops.h>
#include <stdint.h>

#define countof(arr) (sizeof(arr) / sizeof(arr[0]))

// service constants

#define AM_SERVICE_COUNT            4
#define AM_MAX_SESSIONS_PER_SERVICE 5
#define AM_MAX_TOTAL_SESSIONS       5

// names

#ifdef REPLACE_AM
#define SERVICE_NAME "am"
#else
// can't register `am` if we're not replacing am
#define SERVICE_NAME "amf"
#endif

static const struct
{
	const char *name;
	u32 len;
} AM_ServiceNames[AM_SERVICE_COUNT] =
{
	{ .name = SERVICE_NAME ":net", .len = sizeof(SERVICE_NAME ":net" ) - 1 },
	{ .name = SERVICE_NAME ":sys", .len = sizeof(SERVICE_NAME ":sys" ) - 1 },
	{ .name = SERVICE_NAME ":u"  , .len = sizeof(SERVICE_NAME ":u"   ) - 1 },
	{ .name = SERVICE_NAME ":app", .len = sizeof(SERVICE_NAME ":app" ) - 1 }
};

const u32 heap_size = 0x8000; // stock am allocates 0xC000, stacks in this case are in .data so we can use less

__attribute__((section(".data.heap")))
void *heap = NULL;

__attribute__((section(".data.notif_id")))
u32 notification_id = 0;

__attribute__((section(".data.sys_update_mutex")))
Handle GLOBAL_SystemUpdaterMutex;

// thread stacks for non-pipe threads (5), and one pipe thread (1)
__attribute__((section(".data.stacks"),aligned(8)))
static u8 AM_SessionThreadStacks[AM_MAX_TOTAL_SESSIONS + 1][0x1000];

// session data + thread for non-pipes
__attribute__((section(".data.sessioninfo")))
static AM_SessionData AM_SessionsData[AM_MAX_TOTAL_SESSIONS + 1] =
{
	{ .thread = 0, .session = 0, .handle_ipc = NULL, .importing_title = false, .cia_deplist_buf = { 0 }, .media = 0 },
	{ .thread = 0, .session = 0, .handle_ipc = NULL, .importing_title = false, .cia_deplist_buf = { 0 }, .media = 0 },
	{ .thread = 0, .session = 0, .handle_ipc = NULL, .importing_title = false, .cia_deplist_buf = { 0 }, .media = 0 },
	{ .thread = 0, .session = 0, .handle_ipc = NULL, .importing_title = false, .cia_deplist_buf = { 0 }, .media = 0 },
	{ .thread = 0, .session = 0, .handle_ipc = NULL, .importing_title = false, .cia_deplist_buf = { 0 }, .media = 0 },
};

// session storage/thread management

void _thread_start();

Result startThread(Handle *thread, void (* function)(void *), void *arg, void *stack_top, s32 priority, s32 processor_id)
{
	if ((u32)(stack_top) & (0x8 - 1))
		return OS_MISALIGNED_ADDRESS;
	//_thread_start will pop these out
	((u32 *)stack_top)[-1] = (u32)function;
	((u32 *)stack_top)[-2] = (u32)arg;

    return svcCreateThread(thread, _thread_start, function, stack_top, priority, processor_id);
}

static inline void freeThread(Handle *thread)
{
	if (thread && *thread)
	{
		Err_FailedThrow(svcWaitSynchronization(*thread, -1))
		Err_FailedThrow(svcCloseHandle(*thread));
		*thread = 0;
	}
}

extern void (* AM_IPCHandlers[4])(AM_SessionData *);

static inline AM_SessionData *getNewSessionData(Handle session, u8 *index, s32 service_index)
{
	for (u8 i = 0; i < AM_MAX_TOTAL_SESSIONS + 1; i++)
		if (!AM_SessionsData[i].session)
		{
			freeThread(&AM_SessionsData[i].thread);
			AM_SessionsData[i].session = session;
			AM_SessionsData[i].handle_ipc = AM_IPCHandlers[service_index];
			AM_SessionsData[i].importing_title = false;
			*index = i;
			return &AM_SessionsData[i];
		}

	return NULL;
}

// regular service thread entrypoint

void tmain(void *arg)
{
	AM_SessionData *data = (AM_SessionData *)arg;
	Result res;
	s32 index;

	getThreadLocalStorage()->ipc_command[0] = 0xFFFF0000;

	while (true)
	{
		res = svcReplyAndReceive(&index, &data->session, 1, data->session);

		if (R_FAILED(res))
		{
			if (res != OS_REMOTE_SESSION_CLOSED)
				Err_Panic(res)

			break;
		}
		else if (index != 0)
			Err_Panic(OS_EXCEEDED_HANDLES_INDEX)

		data->handle_ipc(data);
	}

	Err_FailedThrow(svcCloseHandle(data->session))
	data->session = 0;
}

// pipe thread entry point

void pipe_tmain()
{
	DEBUG_PRINT("[pipe thread] hello, pipe thread has started\n");
	DEBUG_PRINT("[pipe thread] signaling event\n");

	LightEvent_Signal(&GLOBAL_PipeManager.event);

	DEBUG_PRINT("[pipe thread] event signaled\n");

	// zero-out static buffers so they can't be used

	u32 *static_bufs = getThreadLocalStorage()->ipc_static_buffers;

	static_bufs[0] = IPC_Desc_StaticBuffer(0, 0);
	static_bufs[1] = (u32)NULL;
	static_bufs[2] = IPC_Desc_StaticBuffer(0, 2);
	static_bufs[3] = (u32)NULL;
	static_bufs[4] = IPC_Desc_StaticBuffer(0, 4);
	static_bufs[5] = (u32)NULL;

	Handle handles[2] = { GLOBAL_PipeManager.current_session, GLOBAL_PipeManager.thread_close_event };
	Handle target = 0;

	Result res;
	s32 index;

	getThreadLocalStorage()->ipc_command[0] = 0xFFFF0000;

	while (true)
	{
		res = svcReplyAndReceive(&index, handles, 2, target);

		if (R_FAILED(res))
		{
			if (res != OS_REMOTE_SESSION_CLOSED)
				Err_Panic(res)

			target = 0;
			break;
		}

		if (index == 0)
		{
#ifdef DEBUG_PRINTS
			u32 *ipc_command = getThreadLocalStorage()->ipc_command;

			u32 header_og = ipc_command[0];

			AM_Pipe_HandleIPC();

			u32 header_ret = ipc_command[0];
			Result r = ipc_command[1];

			DEBUG_PRINTF3("[am:pipe] src (", header_og, ") -> (", header_ret, ") replying with result ", r);
#else
			AM_Pipe_HandleIPC();
#endif
		}
		else if (index == 1)
			break;
		else
			Err_Panic(OS_EXCEEDED_HANDLES_INDEX)

		target = handles[index];
	}

	Err_FailedThrow(svcCloseHandle(GLOBAL_PipeManager.current_session))
	GLOBAL_PipeManager.current_session = 0;

	if (GLOBAL_PipeManager.write)
	{
		if (GLOBAL_PipeManager.data) free(GLOBAL_PipeManager.data);
		GLOBAL_PipeManager.data = NULL;
		GLOBAL_PipeManager.write = NULL;
	}

	DEBUG_PRINT("[pipe thread] goodbye\n");
}

// BSS and heap

static inline void initializeBSS()
{
	extern void *__bss_start__;
	extern void *__bss_end__;

	_memset32_aligned(__bss_start__, 0, (size_t)__bss_end__ - (size_t)__bss_end__);
}

void initializeHeap(void)
{
	Handle reslimit;

	Result res = svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE);

	if (R_FAILED(res))
		svcBreak(USERBREAK_PANIC);

	s64 maxCommit = 0, currentCommit = 0;

	ResourceLimitType reslimitType = RESLIMIT_COMMIT;

	svcGetResourceLimitLimitValues(&maxCommit, reslimit, &reslimitType, 1); // for APPLICATION this is equal to APPMEMALLOC at all times
	svcGetResourceLimitCurrentValues(&currentCommit, reslimit, &reslimitType, 1);

	Err_FailedThrow(svcCloseHandle(reslimit));

	u32 remaining = (u32)(maxCommit - currentCommit) &~ 0xFFF;

	if (heap_size > remaining)
		svcBreak(USERBREAK_PANIC);

	res = svcControlMemory(&heap, OS_HEAP_AREA_BEGIN, 0x0, heap_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);

	if (R_FAILED(res))
		svcBreak(USERBREAK_PANIC);

	meminit(heap, heap_size);
}

void deinitializeHeap()
{
	void *tmp;

	if (R_FAILED(svcControlMemory(&tmp, heap, 0x0, heap_size, MEMOP_FREE, 0x0)))
		svcBreak(USERBREAK_PANIC);
}


// better looking index checking

#define SEMAPHORE_REPLY(idx) (idx == 0) // handles[0]
#define SERVICE_REPLY(idx) (idx > 0 && idx < AM_SERVICE_COUNT + 1) // handles[1] until handles[4]
#define PIPE_REPLY(idx) (idx == AM_SERVICE_COUNT + 1) // handles[5]

void AM_Main()
{
	initializeBSS();
	initializeHeap();

    Err_FailedThrow(srvInit())
    Err_FailedThrow(am9Init())
    Err_FailedThrow(fsUserInit())
    Err_FailedThrow(syncInit())

    /*
		handles[0] = semaphore
		handles[1] = am:net  service handle
		handles[2] = am:sys  service handle
		handles[3] = am:u    service handle
		handles[4] = am:app  service handle
		handles[5] = am:pipe pipe port server handle
    */
	Handle handles[AM_SERVICE_COUNT + 2];

	// handles[0] - semaphore
	Err_FailedThrow(SRV_EnableNotification(&handles[0]));
	
	// handles[1] through handles[4] - services
	for (u8 i = 0, j = 1; i < AM_SERVICE_COUNT; i++, j++)
		Err_FailedThrow(SRV_RegisterService(&handles[j], AM_ServiceNames[i].name, AM_ServiceNames[i].len, AM_MAX_SESSIONS_PER_SERVICE))

	// handles[5] - pipe port server
	Err_FailedThrow(svcCreatePort(&handles[5], &GLOBAL_PipeManager.port_client, NULL, 1))

	// locks pipe manager to one thread at a time
	RecursiveLock_Init(&GLOBAL_PipeManager.lock);

	// locks tmd reading for cia ipcs to one thread at a time
	RecursiveLock_Init(&GLOBAL_TMDReader_Lock);

	// used to do ??? in home menu and nim(?)
	Err_FailedThrow(svcCreateMutex(&GLOBAL_SystemUpdaterMutex, 0))

	// used to tell pipe thread to exit when pausing or cancelling imports
	Err_FailedThrow(svcCreateEvent(&GLOBAL_PipeManager.thread_close_event, RESET_ONESHOT))

	// initialize demo db
	AM_DemoDatabase_Initialize(&GLOBAL_DemoDatabase);

	while (true)
	{
		s32 index;

		Result res = svcWaitSynchronizationN(&index, handles, countof(handles), false, -1);

		if (R_FAILED(res))
			Err_Throw(res);

		if (SEMAPHORE_REPLY(index)) // SRV semaphore fired for notification
		{
			Err_FailedThrow(SRV_ReceiveNotification(&notification_id))
			if (notification_id == 0x100) // terminate
				break;
		}
		else if (SERVICE_REPLY(index)) // service handle received request to create session
		{
			Handle session, thread;
			u8 stack_index;

			Err_FailedThrow(svcAcceptSession(&session, handles[index]));

			AM_SessionData *data = getNewSessionData(session, &stack_index, index - 1);

			Err_FailedThrow(startThread(&thread, &tmain, data, AM_SessionThreadStacks[stack_index] + 0x1000, 61, -2));

			data->thread = thread;
		}
		else if (PIPE_REPLY(index)) // pipe server session received request to open pipe session
		{
			Err_FailedThrow(svcAcceptSession(&GLOBAL_PipeManager.current_session, handles[5]))

			freeThread(&GLOBAL_PipeManager.thread);

			Err_FailedThrow(startThread(&GLOBAL_PipeManager.thread, &pipe_tmain, NULL, AM_SessionThreadStacks[5] + 0x1000, 61, -2))
		}
		else // invalid index
			Err_Throw(AM_INTERNAL_RANGE)
	}

	// save, commit and close demodb
	AM_DemoDatabase_Close(&GLOBAL_DemoDatabase);

	// wait and close thread handles
	for (u8 i = 0; i < AM_MAX_TOTAL_SESSIONS + 1; i++)
		freeThread(&AM_SessionsData[i].thread);

	// official services to do this, so why not
	Err_FailedThrow(svcCloseHandle(handles[0]))

	// unregister services
	for (u8 i = 0, j = 1; i < AM_SERVICE_COUNT; i++, j++)
	{
		Err_FailedThrow(SRV_UnregisterService(AM_ServiceNames[i].name, AM_ServiceNames[i].len))
		Err_FailedThrow(svcCloseHandle(handles[j]));
	}

	// pipe stuff
	Err_FailedThrow(svcCloseHandle(handles[5]));
	Err_FailedThrow(svcCloseHandle(GLOBAL_PipeManager.thread_close_event))

	fsUserExit();
	am9Exit();
    srvExit();
    syncExit();
	deinitializeHeap();
}