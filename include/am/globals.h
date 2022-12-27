#ifndef _AM_GLOBALS_H
#define _AM_GLOBALS_H

#include <3ds/synchronization.h>
#include <am/demodb.h>
#include <am/pipe.h>

extern RecursiveLock GLOBAL_TMDReader_Lock;
extern Database GLOBAL_DemoDatabase;
extern AM_Pipe GLOBAL_PipeManager;
extern Handle GLOBAL_AddressArbiter;
extern Handle GLOBAL_SystemUpdaterMutex;

#endif