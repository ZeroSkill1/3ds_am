#ifndef _AM_3DS_IPC_H
#define _AM_3DS_IPC_H

#include <3ds/types.h>
#include <errors.h>

#define CHECK_RET(x) \
	Result res = svcSendSyncRequest(x); \
	if (R_SUCCEEDED(res)) \
		res = (Result)ipc_command[1]; \
	if (R_FAILED(res)) \
		return res;

#define BASIC_RET(x) \
	CHECK_RET(x) \
	return res;

#define RET_OS_INVALID_IPCARG \
	{ \
		ipc_command[0] = IPC_MakeHeader(0, 1, 0); \
		ipc_command[1] = OS_INVALID_IPC_ARGUMENT; \
		return; \
	}

#define CHECK_WRONGARG(x) \
	if ((x)) \
		RET_OS_INVALID_IPCARG

#define CHECK_HEADER(...) \
	if (cmd_header != IPC_MakeHeader(__VA_ARGS__)) \
	{ \
		ipc_command[0] = IPC_MakeHeader(0, 1, 0); \
		ipc_command[1] = OS_INVALID_IPC_HEADER; \
		break; \
	}

typedef enum
{
	IPC_BUFFER_R  = BIT(1),                     ///< Readable
	IPC_BUFFER_W  = BIT(2),                     ///< Writable
	IPC_BUFFER_RW = IPC_BUFFER_R | IPC_BUFFER_W ///< Readable and Writable
} IPC_BufferRights;

static inline ThreadLocalStorage *getThreadLocalStorage(void)
{
	ThreadLocalStorage *tls;
	__asm__ ("mrc p15, 0, %[data], c13, c0, 3" : [data] "=r" (tls));
	return tls;
}

static inline u32 IPC_MakeHeader(u16 command_id, unsigned normal_params, unsigned translate_params)
{
	return ((u32) command_id << 16) | (((u32) normal_params & 0x3F) << 6) | (((u32) translate_params & 0x3F) << 0);
}

static inline u32 IPC_Desc_SharedHandles(unsigned number)
{
	return ((u32)(number - 1) << 26);
}

static inline u32 IPC_Desc_MoveHandles(unsigned number)
{
	return ((u32)(number - 1) << 26) | 0x10;
}

static inline bool IPC_VerifyMoveHandles(u32 desc, unsigned number)
{
	return desc == IPC_Desc_MoveHandles(number);
}

static inline bool IPC_VerifySharedHandles(u32 desc, unsigned number)
{
	return desc == IPC_Desc_SharedHandles(number);
}

static inline u32 IPC_Desc_CurProcessId(void)
{
	return 0x20;
}

static inline bool IPC_CompareHeader(u32 header, u16 command_id, unsigned normal_params, unsigned translate_params)
{
	return header == IPC_MakeHeader(command_id, normal_params, translate_params);
}

// static buffers

static inline u32 IPC_Desc_StaticBuffer(size_t size, unsigned buffer_id)
{
	return (size << 14) | ((buffer_id & 0xF) << 10) | 0x2;
}

static inline bool IPC_VerifyStaticBuffer(u32 desc, unsigned buffer_id)
{
	return (desc & (0xf << 10 | 0xf)) == (((buffer_id & 0xF) << 10) | 0x2);
}

static inline size_t IPC_GetStaticBufferSize(u32 desc)
{
	return (size_t)((desc >> 14) & 0x3FFFF);
}

// normal non-pxi buffers

static inline u32 IPC_Desc_Buffer(size_t size, IPC_BufferRights rights)
{
	return (size << 4) | 0x8 | rights;
}

static inline bool IPC_VerifyBuffer(u32 desc, IPC_BufferRights rights)
{
	return (desc & 0xF) == (0x8 | rights);
}

static inline size_t IPC_GetBufferSize(u32 desc)
{
	return (size_t)(desc >> 4);
}

// pxi buffers

static inline u32 IPC_Desc_PXIBuffer(size_t size, unsigned buffer_id, bool is_read_only)
{
	u8 type = 0x4;
	if(is_read_only)type = 0x6;
	return (size << 8) | ((buffer_id & 0xF) << 4) | type;
}

#endif