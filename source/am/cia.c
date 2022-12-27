#include <am/cia.h>

static TMDHeader __attribute__((section(".data.cia_cindex"))) min_tmd;
RecursiveLock GLOBAL_TMDReader_Lock;

static Result CIAReader_ReadEnabledIndices(CIAReader *rd, u16 amount, u16 *indices, u16 *read_indices)
{
	u8 batch[0x400];
	Result res;

	u32 read;
	u16 enabled = 0, processed = 0;

	for (u16 i = 0; i < 0x2000; i += sizeof(batch))
	{
		if (R_FAILED(res = FSFile_Read(&read, rd->cia, 0x20 + i, sizeof(batch), batch)))
			return res;

		if (read != sizeof(batch))
			return AM_GENERAL_IO_FAILURE;

		for (u16 j = 0; j < sizeof(batch); j++)
		{
			for (u8 k = 0; k < 8; k++, processed++)
			{
				if (batch[j] & (0x80 >> k))
				{
					indices[enabled] = processed;
					enabled++;

					if (enabled == amount)
						goto exit;
				}
			}
		}
	}
exit:
	*read_indices = enabled;
	return 0;
}

Result CIAReader_Init(CIAReader *rd, Handle cia, bool init_tmd)
{
	rd->cia = cia;
	(void)init_tmd;

	Result res;
	u32 read;

	if (R_FAILED(res = FSFile_Read(&read, rd->cia, 0, sizeof(CIAHeader), &rd->header)))
		return res;

	return 0;
}

Result CIAReader_ReadMinTMD(CIAReader *rd)
{
	u32 read;

	Result res = FSFile_Read(&read, rd->cia, TMD_START(rd), sizeof(TMDHeader), &min_tmd);

	if (R_FAILED(res))
		return AM_INTERNAL_RESULT(-3); // result failed

	if (read != sizeof(TMDHeader))
		return AM_INTERNAL_RESULT(-4); // size mismatch

	return res;
}

void CIAReader_Close(CIAReader *rd)
{
	svcCloseHandle(rd->cia);
}

Result CIAReader_CalculateTitleSize(CIAReader *rd, MediaType media_type, u64 *size, u16 *indices_count, u32 *align_size, bool skip_tmd_read)
{
	if (media_type != MediaType_SD && media_type != MediaType_NAND)
		return AM_INVALID_ENUM_VALUE;

	u16 indices[256];
	u16 read_indices = 0;
	Result res;

	if (R_FAILED(res = CIAReader_ReadEnabledIndices(rd, 256, indices, &read_indices)))
		return res;

	u64 _size = 0;
	u32 align = media_type == MediaType_SD ? 0x8000 : 0x4000;

	// step 1: contents

	RecursiveLock_Lock(&GLOBAL_TMDReader_Lock);

	if (!skip_tmd_read && R_FAILED(res = CIAReader_ReadMinTMD(rd)))
		return res;

	ContentChunkRecord ccr;
	u16 tmd_content_count = __builtin_bswap16(min_tmd.ContentCount);
	u32 read;

	for (u16 i = 0; i < tmd_content_count; i++)
	{
		if (R_FAILED(res = FSFile_Read(&read, rd->cia, TMD_START(rd) + sizeof(MinimumTMD) + sizeof(ContentChunkRecord) * i, sizeof(ContentChunkRecord), &ccr)))
			return res;

		for (u16 j = 0; j < read_indices; j++)
			if (__builtin_bswap16(ccr.Index) == indices[j])
				_size += ALIGN(__builtin_bswap64(ccr.Size), align);
	}

	_size += (tmd_content_count > 254 ? 2 : 1) * align; // not sure why this is done but /shrug

	// step 2: tmd

	u32 tmd_size = sizeof(MinimumTMD) + (tmd_content_count * sizeof(ContentChunkRecord));
	_size += ALIGN(tmd_size, align);

	// step 3: save data

	u64 tmd_tid = __builtin_bswap64(min_tmd.TitleID);
	u32 save_size;

	if (TitleID_IsTWL(tmd_tid))
		save_size =
			ALIGN(min_tmd.SaveInfo.Size.SRLPublicSaveDataSize, align) +
			ALIGN(min_tmd.SaveInfo.SRLPrivateSaveDataSize, align) +
			(min_tmd.SaveInfo.SRLFlag & 0x2 ? align : 0);
	else
		save_size = ALIGN(min_tmd.SaveInfo.Size.CTRSaveSize, align);

	RecursiveLock_Unlock(&GLOBAL_TMDReader_Lock);

	if (save_size) // extra align block if save data is there
		save_size += align;

	_size += save_size;

	// step 4: THREE extra blocks of alignment???
	_size += 3 * align;

	// done

	*size = _size;

	if (indices_count) *indices_count = read_indices;
	if (align_size) *align_size = align;

	return 0;
}

Result CIAReader_GetTitleInfo(CIAReader *rd, MediaType media_type, TitleInfo *info)
{
	_memset32_aligned(info, 0x00, sizeof(TitleInfo));

	RecursiveLock_Lock(&GLOBAL_TMDReader_Lock);
	Result res = CIAReader_ReadMinTMD(rd);

	if (R_SUCCEEDED(res))
	{
		info->title_id = __builtin_bswap64(min_tmd.TitleID);
		info->type = __builtin_bswap32(min_tmd.TitleType);
		info->version = __builtin_bswap16(min_tmd.TitleVersion);

		res = CIAReader_CalculateTitleSize(rd, media_type, &info->size, NULL, NULL, true);
	}
	
	RecursiveLock_Unlock(&GLOBAL_TMDReader_Lock);
	return res;
}

Result CIAReader_ExtractMetaSMDH(CIAReader *rd, void *smdh)
{
	Result res;
	u32 read;

	if (R_FAILED(res = FSFile_Read(&read, rd->cia, CON_END(rd) + 0x400, 0x36C0, smdh)))
		return res;

	if (read != 0x36C0) // cia has no meta region
		return AM_META_REGION_NOT_FOUND;

	return 0;
}

Result CIAReader_ExtractDependencyList(CIAReader *rd, void *list)
{
	Result res;
	u32 read;

	if (R_FAILED(res = FSFile_Read(&read, rd->cia, CON_END(rd), 0x300, list)))
		return res;

	if (read != 0x300) // cia has no meta region
		return AM_META_REGION_NOT_FOUND;

	return 0;
}

Result CIAReader_ExtractMetaCoreVersion(CIAReader *rd, u32 *version)
{
	Result res;
	u32 read;

	if (R_FAILED(res = FSFile_Read(&read, rd->cia, CON_END(rd) + 0x300, sizeof(u32), version)))
		return res;

	if (read != sizeof(u32)) // cia has no meta region
		return AM_META_REGION_NOT_FOUND;

	return 0;
}

Result CIAReader_CalculateRequiredSize(CIAReader *rd, MediaType media_type, u64 *size)
{
	u16 indices_count;
	u32 align;
	Result res;

	if (R_FAILED(res = CIAReader_CalculateTitleSize(rd, media_type, size, &indices_count, &align, false)))
		return res;

	// step 1: add another block of align size

	*size += align;

	// step 2: ???

	*size += ALIGN((indices_count * 1024) + 0x200, align);

	return 0;
}

Result CIAReader_ExtractMeta(CIAReader *rd, u32 size, void *meta, u32 *read)
{
	Result res;

	if (rd->header.MetaSize > size)
		return AM_CIA_META_READ_SIZE_TOO_SMALL;

	if (R_FAILED(res = FSFile_Read(read, rd->cia, CON_END(rd), size, meta)))
		return res;

	return 0;
}