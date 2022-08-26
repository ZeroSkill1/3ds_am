#include <3ds/srv.h>

u32 srv_refcount;
Handle srv_session;

Result srvInit()
{
    if (srv_refcount++)
        return 0;

    Result res;

    if (!(R_SUCCEEDED(res = svcConnectToPort(&srv_session, "srv:")) && 
          R_SUCCEEDED(res = SRV_RegisterClient())))
        srvExit();

    return res;
}

void srvExit()
{
    if (--srv_refcount)
        return;
    
    if (srv_session != 0)
    {
        Err_FailedThrow(svcCloseHandle(srv_session));
        srv_session = 0;
    }
}

Result SRV_RegisterClient()
{
    u32 *ipc_command = getThreadLocalStorage()->ipc_command;

    ipc_command[0] = IPC_MakeHeader(ID_SRV_RegisterClient, 0, 2);
    ipc_command[1] = IPC_Desc_CurProcessId();

    BASIC_RET(srv_session)
}

Result SRV_EnableNotification(Handle *sempahore)
{
    u32 *ipc_command = getThreadLocalStorage()->ipc_command;

    ipc_command[0] = IPC_MakeHeader(ID_SRV_EnableNotification, 0, 0);
    
    CHECK_RET(srv_session)

    *sempahore = ipc_command[3];
    return res;
}

Result SRV_RegisterService(Handle *service, const char *service_name, u32 service_name_len, u32 max_sessions)
{
    u32 *ipc_command = getThreadLocalStorage()->ipc_command;

    ipc_command[0] = IPC_MakeHeader(ID_SRV_RegisterService, 4, 0);
    ipc_command[1] = *((u32 *)service_name);
    ipc_command[2] = *((u32 *)(service_name + 4));
    ipc_command[3] = service_name_len;
    ipc_command[4] = max_sessions;

    CHECK_RET(srv_session)

    *service = ipc_command[3];
    return res;
}

Result SRV_UnregisterService(const char *service_name, u32 service_name_length)
{
    u32 *ipc_command = getThreadLocalStorage()->ipc_command;

    ipc_command[0] = IPC_MakeHeader(ID_SRV_UnregisterService, 3, 0);
    ipc_command[1] = *((u32 *)service_name);
    ipc_command[2] = *((u32 *)(service_name + 4));
    ipc_command[3] = service_name_length;

    BASIC_RET(srv_session)
}

Result SRV_GetServiceHandle(Handle *service, const char *service_name, u32 service_name_length, u32 flags)
{
    u32 *ipc_command = getThreadLocalStorage()->ipc_command;

    ipc_command[0] = IPC_MakeHeader(ID_SRV_GetServiceHandle, 4, 0);
    ipc_command[1] = *((u32 *)service_name);
    ipc_command[2] = *((u32 *)(service_name + 4));
    ipc_command[3] = service_name_length;
    ipc_command[4] = flags;

    CHECK_RET(srv_session)

    *service = ipc_command[3];
    return res;
}

Result SRV_RegisterPort(const char *port_name, u32 port_name_length, Handle client_port)
{
    u32 *ipc_command = getThreadLocalStorage()->ipc_command;

    ipc_command[0] = IPC_MakeHeader(ID_SRV_RegisterPort, 3, 2);
    ipc_command[1] = *((u32 *)port_name);
    ipc_command[2] = *((u32 *)(port_name + 4));
    ipc_command[3] = port_name_length;
    ipc_command[4] = IPC_Desc_SharedHandles(1);
    ipc_command[5] = client_port;

    BASIC_RET(srv_session)
}

Result SRV_UnregisterPort(const char *port_name, u32 port_name_length)
{
    u32 *ipc_command = getThreadLocalStorage()->ipc_command;

    ipc_command[0] = IPC_MakeHeader(ID_SRV_UnregisterPort, 3, 0);
    ipc_command[1] = *((u32 *)port_name);
    ipc_command[2] = *((u32 *)(port_name + 4));
    ipc_command[3] = port_name_length;

    BASIC_RET(srv_session)
}

Result SRV_ReceiveNotification(u32 *notification_id)
{
    u32 *ipc_command = getThreadLocalStorage()->ipc_command;

    ipc_command[0] = IPC_MakeHeader(ID_SRV_ReceiveNotification, 0, 0);

    CHECK_RET(srv_session)

    *notification_id = ipc_command[2];
    return res;
}