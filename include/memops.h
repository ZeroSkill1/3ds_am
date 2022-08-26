#ifndef _AM_MEMOPS_H
#define _AM_MEMOPS_H

#include <stdint.h>
#include <stddef.h>

/*
    credit goes to luigoalma for this code
*/

inline static void _memset32_aligned(void* dest, uint32_t c, size_t size) {
	uint32_t *_dest = (uint32_t*)dest;
	for (; size >= 4; size -= 4) {
		*_dest = c;
		_dest++;
	}
	uint8_t *_dest8 = (uint8_t*)_dest;
	for (; size > 0; size--) {
		*_dest8 = c;
		_dest8++;
	}
}

inline static void _memset(void* dest, uint32_t c, size_t size) {
	if (((uintptr_t)dest & 0x3) == 0) {
		_memset32_aligned(dest, c, size);
		return;
	}
	uint8_t *_dest8 = (uint8_t*)dest;
	for (; size > 0; size--) {
		*_dest8 = c;
		_dest8++;
	}
}

inline static void _memcpy32_aligned(void* dest, const void* src, size_t size) {
	uint32_t *_dest = (uint32_t*)dest;
	const uint32_t *_src = (const uint32_t*)src;
	for (; size >= 4; size -= 4) {
		*_dest = *_src;
		_dest++;
		_src++;
	}
	uint8_t *_dest8 = (uint8_t*)_dest;
	const uint8_t *_src8 = (const uint8_t*)_src;
	for (; size > 0; size--) {
		*_dest8 = *_src8;
		_dest8++;
		_src8++;
	}
}

inline static void _memcpy(void* dest, const void* src, size_t size) {
	if (((uintptr_t)dest & 0x3) == 0 && ((uintptr_t)src & 0x3) == 0) {
		_memcpy32_aligned(dest, src, size);
		return;
	}
	uint8_t *_dest8 = (uint8_t*)dest;
	const uint8_t *_src8 = (const uint8_t*)src;
	for (; size > 0; size--) {
		*_dest8 = *_src8;
		_dest8++;
		_src8++;
	}
}

#endif