#ifndef AM_UTIL_H
#define AM_UTIL_H

#include <3ds/types.h>

static inline u16 TitleID_Category(u64 title_id)
{
	return (u16)((title_id >> 32) & 0xFFFF);
}

static inline u16 TitleID_ConsoleType(u64 title_id)
{
	return (u16)((title_id >> 48) & 0xFFFF);
}

static inline bool TitleID_IsTWL(u64 title_id)
{
	return (TitleID_Category(title_id) & 0x8000) == 0x8000;
}

static inline bool TitleID_IsSystemCTR(u64 title_id)
{
	return (TitleID_Category(title_id) & 0xC010) == 0x0010;
}

static inline bool TitleID_IsSystemTWL(u64 title_id)
{
	return (TitleID_Category(title_id) & 0xC001) == 0x8001;
}

static inline bool TitleID_IsAnySystem(u64 title_id)
{
	return TitleID_IsSystemCTR(title_id) || TitleID_IsSystemTWL(title_id);
}

static inline bool TitleID_IsDLPChild(u64 title_id)
{
	return TitleID_ConsoleType(title_id) == 0x0004 && TitleID_Category(title_id) == 0x0001;
}

static inline bool TitleID_IsPatch(u64 title_id)
{
	return TitleID_ConsoleType(title_id) == 0x0004 && TitleID_Category(title_id) == 0x000E;
}

static inline bool TitleID_IsDLC(u64 title_id)
{
	return TitleID_ConsoleType(title_id) == 0x0004 && TitleID_Category(title_id) == 0x008C;
}

static inline bool TitleID_IsLicense(u64 title_id)
{
	return TitleID_ConsoleType(title_id) == 0x0004 && TitleID_Category(title_id) == 0x000D;
}

static inline bool TitleID_IsDLCOrLicense(u64 title_id)
{
	return TitleID_IsDLC(title_id) || TitleID_IsLicense(title_id);
}

// i am aware this is quality:tm: code but yes

#ifdef DEBUG_PRINTS
	void int_convert_to_hex(char* out, uint32_t n, bool newline);
	#define DEBUG_PRINT(str) svcOutputDebugString(str, sizeof(str) - 1)
	#define DEBUG_PRINTF(str, num) \
		{ \
			char __ibuf[12]; \
			\
			svcOutputDebugString(str, sizeof(str) - 1); \
			int_convert_to_hex(__ibuf, num, true); \
			svcOutputDebugString(__ibuf, 11); \
		}
	#define DEBUG_PRINTF2(str, num, str2, num2) \
		{ \
			char __ibuf[12]; \
			\
			svcOutputDebugString(str, sizeof(str) - 1); \
			int_convert_to_hex(__ibuf, num, false); \
			svcOutputDebugString(__ibuf, 10); \
			\
			svcOutputDebugString(str2, sizeof(str2) - 1); \
			int_convert_to_hex(__ibuf, num2, true); \
			svcOutputDebugString(__ibuf, 11); \
		}
	#define DEBUG_PRINTF3(str, num, str2, num2, str3, num3) \
		{ \
			char __ibuf[12]; \
			\
			svcOutputDebugString(str, sizeof(str) - 1); \
			int_convert_to_hex(__ibuf, num, false); \
			svcOutputDebugString(__ibuf, 10); \
			\
			svcOutputDebugString(str2, sizeof(str2) - 1); \
			int_convert_to_hex(__ibuf, num2, false); \
			svcOutputDebugString(__ibuf, 10); \
			\
			svcOutputDebugString(str3, sizeof(str3) - 1); \
			int_convert_to_hex(__ibuf, num3, true); \
			svcOutputDebugString(__ibuf, 11); \
		}

#else
	#define DEBUG_PRINT(str) {}
	#define DEBUG_PRINTF(str, num) {}
	#define DEBUG_PRINTF2(str, num, str2, num2) {}
	#define DEBUG_PRINTF3(str, num, str2, num2, str3, num3) {}
#endif

#endif