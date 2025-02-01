#ifndef _AM_GLOBALS_H
#define _AM_GLOBALS_H

#include <3ds/synchronization.h>
#include <am/demodb.h>
#include <am/pipe.h>

extern RecursiveLock g_TMDReader_Lock;
extern Database g_DemoDatabase;
extern AM_Pipe g_PipeManager;
extern Handle g_AddressArbiter;
extern Handle g_SystemUpdaterMutex;

#endif