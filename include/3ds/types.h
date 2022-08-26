#ifndef _AM_3DS_TYPES_H
#define _AM_3DS_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define U64_MAX	UINT64_MAX
#define U32_MAX UINT32_MAX
#define U16_MAX UINT16_MAX
#define U8_MAX UINT8_MAX

typedef uint8_t u8;   ///<  8-bit unsigned integer
typedef uint16_t u16; ///< 16-bit unsigned integer
typedef uint32_t u32; ///< 32-bit unsigned integer
typedef uint64_t u64; ///< 64-bit unsigned integer

typedef int8_t s8;   ///<  8-bit signed integer
typedef int16_t s16; ///< 16-bit signed integer
typedef int32_t s32; ///< 32-bit signed integer
typedef int64_t s64; ///< 64-bit signed integer

#define LODWORD(x) ((u32)(x & 0xFFFFFFFF))
#define HIDWORD(x) ((u32)((x >> 32) & 0xFFFFFFFF))
#define BIT(n) (1 << (n))
#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)
#define ALIGN(x,y) ((x + y - 1) & ~(y - 1))

typedef s32 Handle;
typedef s32 Result;

// syscalls

#define OS_HEAP_AREA_BEGIN ((void *)0x08000000) ///< Start of the heap area in the virtual address space
#define OS_HEAP_AREA_END   ((void *)0x0E000000) ///< End of the heap area in the virtual address space
#define OS_MAP_AREA_BEGIN  ((void *)0x10000000) ///< Start of the mappable area in the virtual address space
#define OS_MAP_AREA_END    ((void *)0x14000000) ///< End of the mappable area in the virtual address space

#define CFG_FIRM_VERSIONREVISION ((u8 *) 0x1FF80061)
#define CFG_FIRM_VERSIONMINOR    ((u8 *) 0x1FF80062)
#define CFG_FIRM_SYSCOREVER      ((u32 *)0x1FF80064)

#define CUR_PROCESS_HANDLE 0xFFFF8001 // Handle to current process

// IPC

typedef struct __attribute__((aligned(4))) ThreadLocalStorage
{
  u8 any_purpose[128];
  u32 ipc_command[64];
  u32 ipc_static_buffers[32];
} ThreadLocalStorage;

#endif