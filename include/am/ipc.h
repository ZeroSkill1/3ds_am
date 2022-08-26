#ifndef _AM_IPC_H
#define _AM_IPC_H

#include <am/twlexport.h>
#include <am/globals.h>
#include <am/asserts.h>
#include <allocator.h>
#include <am/demodb.h>
#include <3ds/types.h>
#include <3ds/ipc.h>
#include <3ds/am9.h>
#include <am/util.h>
#include <am/pipe.h>
#include <am/cia.h>

typedef struct AM_SessionData
{
	Handle thread;
	Handle session; // needs to be freed in thread itself!
	void (* handle_ipc)(struct AM_SessionData *data);
	bool importing_title;
	u64 cia_deplist_buf[0x60];
	MediaType media;
} AM_SessionData;

void AMNET_HandleIPC(AM_SessionData *session);
void AMU_HandleIPC(AM_SessionData *session);
void AMSYS_HandleIPC(AM_SessionData *session);
void AMAPP_HandleIPC(AM_SessionData *session);

extern void (* AM_IPCHandlers[4])(AM_SessionData *);

#endif