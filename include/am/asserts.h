#ifndef _AM_ASSERTS_H
#define _AM_ASSERTS_H

#include <3ds/result.h>
#include <3ds/types.h>
#include <3ds/err.h>
#include <3ds/fs.h>

static inline void assertNotAmOrFsWithMedia(Result res, MediaType media_type)
{
	if (R_FAILED(res))
	{
		u16 mod = R_MODULE(res);
		if ((mod == RM_FS && media_type == MediaType_NAND) || (mod != RM_FS && mod != RM_AM))
			Err_Throw(res)
	}
}

static inline void assertNotAmOrFs(Result res)
{
	if (R_FAILED(res))
	{
		u16 mod = R_MODULE(res);
		if (mod != RM_FS && mod != RM_AM)
			Err_Throw(res)
	}
}

static inline void assertNotAm(Result res)
{
	if (R_FAILED(res) && R_MODULE(res) != RM_AM)
		Err_Throw(res)
}

#endif
