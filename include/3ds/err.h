#ifndef _AM_3DS_ERR_H
#define _AM_3DS_ERR_H

#include <3ds/result.h>
#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/ipc.h>
#include <memops.h>

extern Handle errf_session;
extern u32 errf_refcount;

typedef struct
{
	u32 r[13]; ///< r0-r12.
	u32 sp;    ///< sp.
	u32 lr;    ///< lr.
	u32 pc;    ///< pc. May need to be adjusted.
	u32 cpsr;  ///< cpsr.
} CpuRegisters;

typedef enum
{
	ERRF_ERRTYPE_GENERIC      = 0, ///< For generic errors. Shows miscellaneous info.
	ERRF_ERRTYPE_MEM_CORRUPT  = 1, ///< Same output as generic, but informs the user that "the System Memory has been damaged".
	ERRF_ERRTYPE_CARD_REMOVED = 2, ///< Displays the "The Game Card was removed." message.
	ERRF_ERRTYPE_EXCEPTION    = 3, ///< For exceptions, or more specifically 'crashes'. union data should be exception_data.
	ERRF_ERRTYPE_FAILURE      = 4, ///< For general failure. Shows a message. union data should have a string set in failure_mesg
	ERRF_ERRTYPE_LOGGED       = 5, ///< Outputs logs to NAND in some cases.
} FatalErrorType;

typedef enum
{
	ERRF_EXCEPTION_PREFETCH_ABORT = 0, ///< Prefetch Abort
	ERRF_EXCEPTION_DATA_ABORT     = 1, ///< Data abort
	ERRF_EXCEPTION_UNDEFINED      = 2, ///< Undefined instruction
	ERRF_EXCEPTION_VFP            = 3, ///< VFP (floating point) exception.
} ExceptionType;

typedef struct
{
	ExceptionType type; ///< Type of the exception. One of the ERRF_EXCEPTION_* values.
	u8  reserved[3];
	u32 fsr;                ///< ifsr (prefetch abort) / dfsr (data abort)
	u32 far;                ///< pc = ifar (prefetch abort) / dfar (data abort)
	u32 fpexc;
	u32 fpinst;
	u32 fpinst2;
} ExceptionInfo;

typedef struct
{
	ExceptionInfo excep;   ///< Exception info struct
	CpuRegisters regs;          ///< CPU register dump.
} ExceptionData;

typedef struct
{
	FatalErrorType type; ///< Type, one of the ERRF_ERRTYPE_* enum
	u8  revHigh;       ///< High revison ID
	u16 revLow;        ///< Low revision ID
	u32 resCode;       ///< Result code
	u32 pcAddr;        ///< PC address at exception
	u32 procId;        ///< Process ID.
	u64 titleId;       ///< Title ID.
	u64 appTitleId;    ///< Application Title ID.
	union
    {
		ExceptionData exception_data; ///< Data for when type is ERRF_ERRTYPE_EXCEPTION
		char failure_mesg[0x60];             ///< String for when type is ERRF_ERRTYPE_FAILURE
	} data;                                ///< The different types of data for errors.
} FatalErrorInfo;

Result errfInit();
void errfExit();

void ERRF_ThrowResultNoRet(Result failure) __attribute__((noreturn, noinline));

#define Err_Throw(failure) ERRF_ThrowResultNoRet(failure);
#define Err_FailedThrow(failure) do {Result __tmp = failure; if (R_FAILED(__tmp)) Err_Throw(__tmp);} while(0);
#define Err_Panic(failure) do { Result __tmp = failure; if (!R_FAILED(__tmp)) {break;} while(1) { svcBreak(USERBREAK_PANIC); } } while(0);

#endif