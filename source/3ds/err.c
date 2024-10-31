#include <3ds/err.h>

Handle errf_session;
u32 errf_refcount;

Result errfInit()
{
	Result res;

	if (errf_refcount++) return 0;
	if (R_FAILED(res = svcConnectToPort(&errf_session, "err:f"))) 
        errfExit();
	return res;
}

void errfExit()
{
	if (--errf_refcount)
		return;
	if (errf_session)
    {
        svcCloseHandle(errf_session);
        errf_session = 0;
    }
}

void ERRF_ThrowResultNoRet(Result failure)
{
	while (R_FAILED(errfInit()))
		svcSleepThread(100000000LLU);

	// manually inlined ERRF_Throw and adjusted to make smaller code output
	uint32_t *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(1, 32, 0);
	_memset32_aligned(&ipc_command[1], 0, sizeof(FatalErrorInfo));

	FatalErrorInfo *error = (FatalErrorInfo *)&ipc_command[1];

	error->type = ERRF_ERRTYPE_GENERIC;
	error->pcAddr = (u32)__builtin_extract_return_addr(__builtin_return_address(0));
	error->resCode = failure;

	svcGetProcessId(&error->procId, CUR_PROCESS_HANDLE);

	svcSendSyncRequest(errf_session);
	errfExit();

	while (true)
		svcSleepThread(10000000000LLU); // lighter loop
}
