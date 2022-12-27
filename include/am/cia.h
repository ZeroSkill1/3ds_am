#ifndef _AM_CIA_H
#define _AM_CIA_H

#include <3ds/synchronization.h>
#include <3ds/types.h>
#include <am/format.h>
#include <3ds/am9.h>
#include <am/util.h>
#include <3ds/fs.h>

typedef struct CIAReader
{
	CIAHeader header;
	Handle cia;
} CIAReader;

Result CIAReader_Init(CIAReader *rd, Handle cia, bool init_tmd);

Result CIAReader_ReadMinTMD(CIAReader *rd);
Result CIAReader_CalculateTitleSize(CIAReader *rd, MediaType media_type, u64 *size, u16 *indices_count, u32 *align_size, bool skip_tmd_read);
Result CIAReader_CalculateRequiredSize(CIAReader *rd, MediaType media_type, u64 *size);
Result CIAReader_ExtractMetaSMDH(CIAReader *rd, void *smdh);
Result CIAReader_ExtractDependencyList(CIAReader *rd, void *list);
Result CIAReader_ExtractMetaCoreVersion(CIAReader *rd, u32 *version);
Result CIAReader_ExtractMeta(CIAReader *rd, u32 size, void *meta, u32 *read);
Result CIAReader_GetTitleInfo(CIAReader *rd, MediaType media_type, TitleInfo *info);

void CIAReader_Close(CIAReader *rd);

#endif