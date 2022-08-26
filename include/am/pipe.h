#ifndef _AM_PIPE_H
#define _AM_PIPE_H

#include <3ds/synchronization.h>
#include <am/asserts.h>
#include <allocator.h>
#include <3ds/types.h>
#include <am/format.h>
#include <3ds/am9.h>
#include <3ds/svc.h>
#include <am/util.h>
#include <3ds/fs.h>
#include <errors.h>

typedef Result (* AM_PipeWriteImpl)(void *, u64 offset, u32 size, u32 flags, void *data, u32 *written);

typedef struct AM_Pipe
{
	Handle port_client;
	Handle current_session;
	Handle thread;
	Handle thread_close_event;
	AM_PipeWriteImpl write;
	void *data;
	RecursiveLock lock;
	LightEvent event;
	u8 init;
} AM_Pipe;

enum
{
	InstallState_Header           = 0x0,
	InstallState_ContentIndex     = 0x1,
	InstallState_CertChainCopy    = 0x2,
	InstallState_CertChainInstall = 0x3,
	InstallState_TicketHeader     = 0x4,
	InstallState_Ticket           = 0x5,
	InstallState_TMDHeader        = 0x6,
	InstallState_TMD              = 0x7,
	InstallState_Content          = 0x8,
	InstallState_Finalize         = 0x9
};

typedef struct CIAInstallData
{
    CIAHeader header;
    u64 ticket_tid;
    u64 tmd_tid;
    MediaType media;
    u8 db_type;
    u8 state;
    u8 num_contents_imported_batch; // dlc only
    u8 batch_size; // dlc only
    u8 misalign_bufsize;
    u8 misalign_buf[16];
    void *buf;
    u32 offset;
    u32 cur_content_start_offs;
    u32 cur_content_end_offs;
    u16 num_contents_imported;
    u16 cindex;
    bool is_dlc: 1;
    bool importing_title: 1;
    bool importing_content: 1;
    bool importing_tmd: 1;
    bool importing_tik: 1;
    bool tik_header_imported: 1;
    bool tmd_header_imported: 1;
    bool invalidated: 1;
    bool overwrite: 1;
    bool system: 1;
} CIAInstallData;

bool atomicUpdateState(u8 *src, u8 val, u8 wanted);

Result AM_Pipe_CreateImportHandle(AM_Pipe *pipe, AM_PipeWriteImpl impl, void *data, Handle *import);
Result AM_Pipe_CreateCIAImportHandle(AM_Pipe *pipe, MediaType media_type, u8 title_db_type, bool overwrite, bool is_system, Handle *import);
Result AM_Pipe_CreateTicketImportHandle(AM_Pipe *pipe, Handle *import);
Result AM_Pipe_CreateTMDImportHandle(AM_Pipe *pipe, Handle *import);
Result AM_Pipe_CreateContentImportHandle(AM_Pipe *pipe, Handle *import);

void AM_Pipe_CloseImportHandle(AM_Pipe *pipe, Handle import);
void AM_Pipe_EnsureThreadExit(AM_Pipe *pipe);
void AM_Pipe_HandleIPC();

#endif