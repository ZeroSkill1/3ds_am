#include <am/ipc.h>

static bool firmNewerThanRunning(TitleInfo *firm)
{
	return
		((u8)firm->title_id >  *CFG_FIRM_SYSCOREVER) ||
		((u8)firm->title_id == *CFG_FIRM_SYSCOREVER && ((firm->version >> 4) & 0x3F) > *CFG_FIRM_VERSIONMINOR) ||
		((u8)firm->title_id == *CFG_FIRM_SYSCOREVER && ((firm->version >> 4) & 0x3F) > *CFG_FIRM_VERSIONMINOR && (firm->version & 0xF) > *CFG_FIRM_VERSIONREVISION);
}

static u64 findNewestFirmTitleId(u64 *title_ids, u32 count)
{
	u64 curr = 0, ret = 0;

	for (u32 i = 0; i < count; i++)
	{
		if (!TitleID_IsSystemCTR(title_ids[i]) || !(title_ids[i] & 0x1))
		{
			curr = title_ids[i];

			if (((LODWORD(curr) >> 8) & 0xFFFFF) == 0 && (u8)curr > (u8)ret)
				ret = curr;
		}
	}

	return ret;
}

static Result findNewestInstalledFirmTitleId(u64 *firm_title_id)
{
	Result res;
	u32 count = 0;

	if (R_FAILED(res = AM9_GetTitleCount(&count, MediaType_NAND)))
		return res;

	u64 *tids = (u64 *)malloc(sizeof(u64) * count);

	if (!tids)
		return AM_OUT_OF_MEMORY;

	if (R_FAILED(res = AM9_GetTitleList(&count, count, MediaType_NAND, tids)))
	{
		free(tids);
		return res;
	}

	*firm_title_id = findNewestFirmTitleId(tids, count);

	free(tids);

	return res;
}

static Result findInstalledFirmTitleIdNewerThanRunning(u64 *firm_title_id)
{
	u64 tmp;

	Result res = findNewestInstalledFirmTitleId(&tmp);

	if (R_FAILED(res))
		return res;
	else if (!tmp)
		return AM_NOT_FOUND;

	TitleInfo firm_info;

	if (R_FAILED(res = AM9_GetTitleInfos(MediaType_NAND, 1, &tmp, &firm_info)))
		return res;

	if (!firmNewerThanRunning(&firm_info))
		return AM_NOT_FOUND;

	*firm_title_id = tmp;

	return res;
}

static Result findFirmTitleIdToInstall(u64 *title_ids, u32 count, u64 *out_tid)
{
	u64 hver_tid_input = findNewestFirmTitleId(title_ids, count);
	u64 hver_tid_installed = 0;

	Result res = findInstalledFirmTitleIdNewerThanRunning(&hver_tid_installed);
	
	if (R_FAILED(res))
	{
		if (res != AM_NOT_FOUND)
			return res;
		else if (!hver_tid_input)
			return AM_NOT_FOUND;

		TitleInfo info;

		if (R_FAILED(res = AM9_GetPendingTitleInfo(1, MediaType_NAND, &hver_tid_input, &info)))
			return res;

		if (firmNewerThanRunning(&info))
		{
			*out_tid = hver_tid_input;
			return 0;
		}

		return AM_NOT_FOUND;
	}

	if (!hver_tid_input || (u8)hver_tid_input < (u8)hver_tid_installed)
		*out_tid = hver_tid_installed;
	else
		*out_tid = hver_tid_input;

	return 0;
}

static Result AM_InstallTitlesCommit_OptionalFirmUpgrade(MediaType media_type, u32 count, u8 db_type, u64 *title_ids)
{
	u64 firm_tid;
	Result res = findFirmTitleIdToInstall(title_ids, count, &firm_tid);

	return R_FAILED(res) ?
		res != AM_NOT_FOUND ?
			res : AM9_InstallTitlesCommit(media_type, count, db_type, title_ids) :
			AM9_InstallTitlesFinishAndInstallFIRM(media_type, count, firm_tid, db_type, title_ids);
}

static Result AMNet_CalculateContextRequiredSize(MediaType media_type, u64 title_id, u16 *indices, u32 indices_count, u64 *size)
{
	if (media_type != MediaType_NAND && media_type != MediaType_SD)
		return AM_INVALID_ENUM_VALUE;

	u64 _size = 0;

	u32 align = media_type == MediaType_SD ? 0x10000 : 0x4000;
	u32 remain = indices_count;
	u32 processed = 0;

	ContentInfo infos[64];

	Result res = 0;

	// calculate sizes using given indices

	while (processed != indices_count)
	{
		u32 batch = MIN(remain, 64);

		if (R_FAILED(res = AM9_FindContentInfos(media_type, title_id, batch, &indices[processed], infos)))
			return res;

		for (u32 i = 0; i < batch; i++)
		{
			if (!(infos[i].type & ContentTypeFlag_Optional)) // only allow optional contents
				return AM_GENERAL_INVALIDARG;

			_size += ALIGN(infos[i].size, align);
		}

		processed += batch;
	}

	u32 num_infos = 0, read_infos = 0;

	// get last content info

	if (R_FAILED(res = AM9_GetContentInfoCount(&num_infos, media_type, title_id)) ||
		R_FAILED(res = AM9_ListContentInfos(&read_infos, 1, media_type, title_id, num_infos - 1, &infos[0])))
		return res;

	// ????????? (this deserves more ???'s)

	_size += 
		ALIGN((indices_count * 0x400) + 0x200, align) +
		ALIGN((((0x10 * infos[0].index) + 0x10) + ((4 * infos[0].index) + 4) + 0x20 + (4 * num_infos)), align);

	*size = _size;
	return 0;
}

static void AM_HandleIPC_Range0x1_0x2D()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;
	u32 cmd_header = ipc_command[0];

	switch ((cmd_header >> 16) & 0xFFFF)
	{
	case 0x0001: // get title count
		{
			CHECK_HEADER(0x0001, 1, 0)

			MediaType media_type = (MediaType)ipc_command[1];

			u32 count = 0;

			Result res = AM9_GetTitleCount(&count, media_type);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0001, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x0002: // get title (id) list
		{
			CHECK_HEADER(0x0002, 2, 2)

			u32 amount = ipc_command[1];
			u32 objcount = amount * sizeof(u64);
			MediaType media_type = (MediaType)ipc_command[2];

			u32 count = 0;

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[3]) != objcount
			)

			u64 *tids_buffer = (u64 *)ipc_command[4];

			Result res = AM9_GetTitleList(&count, amount, media_type, tids_buffer);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0002, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(objcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)tids_buffer;
		}
		break;
	case 0x0003: // get title infos
		{
			CHECK_HEADER(0x0003, 2, 4)

			MediaType media_type = ipc_command[1];
			u32 count = ipc_command[2];
			u32 tobjcount = count * sizeof(u64);
			u32 iobjcount = count * sizeof(TitleInfo);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_R) || 
				IPC_GetBufferSize(ipc_command[3]) != tobjcount ||
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount
			)

			u64 *tids_buffer = (u64 *)ipc_command[4];
			TitleInfo *infos_buffer = (TitleInfo *)ipc_command[6];

			Result res = AM9_GetTitleInfos(media_type, count, tids_buffer, infos_buffer);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0003, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)tids_buffer;
			ipc_command[4] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)infos_buffer;
		}
		break;
	case 0x0004: // delete title (user)
		{
			CHECK_HEADER(0x0004, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			Result res = 0;

			if (TitleID_IsAnySystem(title_id))
				res = AM_TRYING_TO_UNINSTALL_SYSAPP;
			else
			{
				res = AM9_DeleteTitle(media_type, title_id);
				assertNotAmOrFsWithMedia(res, media_type);
			}

			ipc_command[0] = IPC_MakeHeader(0x0004, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0005: // get title product code
		{
			CHECK_HEADER(0x0005, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			char product_code[16];

			Result res = AM9_GetTitleProductCode(product_code, media_type, title_id);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0005, 5, 0);
			ipc_command[1] = res;
			_memcpy32_aligned(&ipc_command[2], product_code, sizeof(product_code));
		}
		break;
	case 0x0006: // get title extdata id
		{
			CHECK_HEADER(0x0006, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			u64 extdata_id = 0;

			Result res = AM9_GetTitleExtDataID(&extdata_id, media_type, title_id);

			ipc_command[0] = IPC_MakeHeader(0x0006, 3, 0);
			ipc_command[1] = res;
			ipc_command[2] = LODWORD(extdata_id);
			ipc_command[3] = HIDWORD(extdata_id);
		}
		break;
	case 0x0007: // delete ticket
		{
			CHECK_HEADER(0x0007, 2, 0)

			u64 title_id = *((u64 *)(&ipc_command[1]));

			Result res = AM9_DeleteTicket(title_id);

			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0007, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0008: // get ticket count
		{
			CHECK_HEADER(0x0008, 0, 0)

			u32 count = 0;

			Result res = AM9_GetTicketCount(&count);

			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0008, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x0009: // get ticket (title id) list
		{
			CHECK_HEADER(0x0009, 2, 2)

			u32 amount = ipc_command[1];
			u32 objcount = amount * sizeof(u64);
			u32 offset = ipc_command[2];

			u32 count = 0;

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[3]) != objcount
			)

			u64 *tids_buffer = (u64 *)ipc_command[4];

			Result res = AM9_GetTicketList(&count, amount, offset, tids_buffer);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0009, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(objcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)tids_buffer;
		}
		break;
	case 0x000A: // get device id
		{
			CHECK_HEADER(0x000A, 0, 0)

			u64 id = 0;

			Result res = AM9_GetDeviceID(&id);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x000A, 3, 0);
			ipc_command[1] = res;
			ipc_command[2] = LODWORD(id);
			ipc_command[3] = HIDWORD(id);
		}
		break;
	case 0x000B: // get import title context count
		{
			CHECK_HEADER(0x000B, 1, 0)

			MediaType media_type = (MediaType)ipc_command[1];

			u32 count = 0;

			Result res = AM9_GetPendingTitleCount(&count, media_type, 0xFF); // everything
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x000B, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x000C: // get import title context list
		{
			CHECK_HEADER(0x000C, 2, 2)

			u32 amount = ipc_command[1];
			u32 objcount = amount * sizeof(u64);
			MediaType media_type = (MediaType)ipc_command[2];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[3]) != objcount
			)

			u64 *tids_buffer = (u64 *)ipc_command[4];

			u32 count = 0;

			Result res = AM9_GetPendingTitleList(&count, amount, media_type, 0xFF, tids_buffer); // everything
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x000C, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(objcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)tids_buffer;
		}
		break;
	case 0x000D: // get import title contexts
		{
			CHECK_HEADER(0x000D, 2, 4)

			u32 count = ipc_command[1];
			u32 tobjcount = count * sizeof(u64);
			u32 iobjcount = count * sizeof(TitleInfo);
			MediaType media_type = (MediaType)ipc_command[2];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[3]) != tobjcount ||
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount
			)

			u64 *tids_buffer = (u64 *)ipc_command[4];
			TitleInfo *infos_buffer = (TitleInfo *)ipc_command[6];

			Result res = AM9_GetPendingTitleInfo(count, media_type, tids_buffer, infos_buffer);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x000D, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)tids_buffer;
			ipc_command[4] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)infos_buffer;
		}
		break;
	case 0x000E: // delete import title context
		{
			CHECK_HEADER(0x000E, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			Result res = AM9_DeletePendingTitle(media_type, title_id);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x000E, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x000F: // get import content context count
		{
			CHECK_HEADER(0x000F, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			u32 count = 0;

			Result res = AM9_GetImportContentContextsCount(&count, media_type, title_id); // everything
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x000F, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x0010: // get import content context list
		{
			CHECK_HEADER(0x0010, 4, 2)

			u32 amount = ipc_command[1];
			u32 objcount = amount * sizeof(u16);
			MediaType media_type = (MediaType)ipc_command[2];
			u64 title_id = *(u64 *)(&ipc_command[3]);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != objcount
			)

			u16 *indices_buf = (u16 *)ipc_command[6];

			u32 count = 0;

			Result res = AM9_GetImportContentContextsList(&count, amount, media_type, title_id, indices_buf);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0010, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(objcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)indices_buf;
		}
		break;
	case 0x0011: // get import content contexts
		{
			CHECK_HEADER(0x0011, 4, 4)

			u32 count = ipc_command[1];
			u32 iobjcount = count * sizeof(u16);
			u32 cobjcount = count * sizeof(ImportContentContext);
			MediaType media_type = (MediaType)ipc_command[2];
			u64 title_id = *((u64 *)(&ipc_command[3]));

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount ||
				!IPC_VerifyBuffer(ipc_command[7], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[7]) != cobjcount
			)

			u16 *indices_buf = (u16 *)ipc_command[6];
			ImportContentContext *contexts_buf = (ImportContentContext *)ipc_command[8];

			Result res = AM9_GetImportContentContexts(count, media_type, title_id, indices_buf, contexts_buf);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0011, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)indices_buf;
			ipc_command[4] = IPC_Desc_Buffer(cobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)contexts_buf;
		}
		break;
	case 0x0012:
		{
			CHECK_HEADER(0x0012, 4, 2)

			u32 count = ipc_command[1];
			u32 iobjcount = count * sizeof(u16);
			MediaType media_type = (MediaType)ipc_command[2];
			u64 title_id = *((u64 *)(&ipc_command[3]));

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount
			)

			u16 *indices_buf = (u16 *)ipc_command[6];

			Result res = AM9_DeleteImportContentContexts(count, media_type, title_id, indices_buf);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0012, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)indices_buf;
		}
		break;
	case 0x0013: // needs cleanup (import database initialized) -> checks if there are any titles pending finalization in import.db
		{
			CHECK_HEADER(0x0013, 1, 0)

			MediaType media_type = (MediaType)ipc_command[1];

			u8 initialized = 0;

			Result res = AM9_ImportDatabaseInitialized(&initialized, media_type);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0013, 1, 0);
			ipc_command[1] = res;
			ipc_command[2] = (u32)initialized;
		}
		break;
	case 0x0014: // do cleanup -> calls installtitlefinish() on every title id inside import.db where applicable
		{
			CHECK_HEADER(0x0014, 1, 0)

			MediaType media_type = (MediaType)ipc_command[1];

			Result res = AM9_Cleanup(media_type);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0014, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0015: // delete all import title contexts
		{
			CHECK_HEADER(0x0015, 1, 0)

			MediaType media_type = (MediaType)ipc_command[1];

			Result res = AM9_DeletePendingTitles(media_type, 0xFF); // everything
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0015, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0016: // delete all temporary titles
		{
			CHECK_HEADER(0x0016, 0, 0)

			Result res = AM9_DeleteTemporaryTitles();
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0016, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0017: // stubbed: import twl backup
		{
			CHECK_HEADER(0x0017, 4, 2)

			u32 bufsize = ipc_command[1];

			CHECK_WRONGARG
			(
				!IPC_VerifyMoveHandles(ipc_command[2], 1) ||
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[4]) != bufsize
			)

			Handle file_handle = ipc_command[3];
			void *buffer = (void *)ipc_command[5];

			(void)file_handle;

			ipc_command[0] = IPC_MakeHeader(0x0017, 1, 2);
			ipc_command[1] = 0;
			ipc_command[2] = IPC_Desc_Buffer(bufsize, IPC_BUFFER_W);
			ipc_command[3] = (u32)buffer;
		}
		break;
	case 0x0018: // initialize title database
		{
			CHECK_HEADER(0x0018, 2, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			bool overwrite = (bool)ipc_command[2];

			Result res = AM9_InitializeTitleDatabase(media_type, overwrite);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0018, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0019: // query title database availability
		{
			CHECK_HEADER(0x0019, 1, 0)

			MediaType media_type = (MediaType)ipc_command[1];

			u8 available = 0;

			Result res = AM9_ReloadTitleDatabase(&available, media_type);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0019, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = (u32)available;
		}
		break;
	case 0x001A: // calculate TWL backup size
		{
			CHECK_HEADER(0x001A, 3, 0)

			u64 twl_title_id = *((u64 *)(&ipc_command[1]));
			u8 export_type = (u8)ipc_command[3];

			u32 size = 0;

			Result res = AM9_DSiWareExportGetSize(&size, twl_title_id, export_type);

			ipc_command[0] = IPC_MakeHeader(0x001A, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = size;
		}
		break;
	case 0x001B: // export TWL backup
		{
			CHECK_HEADER(0x001B, 5, 4)

			u64 twl_title_id = *((u64 *)(&ipc_command[1]));
			u32 path_size = ipc_command[3];
			u32 working_buffer_size = ipc_command[4];
			u8 export_type = (u8)ipc_command[5];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[6], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[6]) != path_size ||
				!IPC_VerifyBuffer(ipc_command[8], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[8]) != working_buffer_size
			)

			void *path = (void *)ipc_command[7];
			void *working_buffer = (void *)ipc_command[9];

			Result res = AM9_DSiWareExportCreate(twl_title_id, path_size, working_buffer_size, export_type, path, working_buffer);

			ipc_command[0] = IPC_MakeHeader(0x001B, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(path_size, IPC_BUFFER_R);
			ipc_command[3] = (u32)path;
			ipc_command[4] = IPC_Desc_Buffer(working_buffer_size, IPC_BUFFER_W);
			ipc_command[5] = (u32)working_buffer;
		}
		break;
	case 0x001C: // import TWL backup
		{
			CHECK_HEADER(0x001C, 2, 4)

			u32 working_buffer_size = ipc_command[1];
			AM_TWLExportType export_type = (AM_TWLExportType)ipc_command[2];

			CHECK_WRONGARG
			(
				!IPC_VerifyMoveHandles(ipc_command[3], 1) ||
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != working_buffer_size
			)

			Handle export = ipc_command[4];
			void *working_buffer = (void *)ipc_command[6];

			Result res = AM_ImportTWLBackup(working_buffer_size, export_type, export, working_buffer);

			ipc_command[0] = IPC_MakeHeader(0x001C, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(working_buffer_size, IPC_BUFFER_W);
			ipc_command[3] = (u32)working_buffer;
		}
		break;
	case 0x001D: // delete all user TWL titles
		{
			CHECK_HEADER(0x001D, 0, 0)

			Result res = AM9_DeleteUserDSiWareTitles();
			assertNotAmOrFsWithMedia(res, MediaType_NAND);

			ipc_command[0] = IPC_MakeHeader(0x001D, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x001E: // read TWL export info (11 content sections)
		{
			CHECK_HEADER(0x001E, 3, 8)

			u32 output_info_size = ipc_command[1];
			u32 banner_size = ipc_command[2];
			u32 buffer_size = ipc_command[3];

			CHECK_WRONGARG
			(
				!IPC_VerifyMoveHandles(ipc_command[4], 1) ||
				!IPC_VerifyBuffer(ipc_command[6], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[6]) != output_info_size ||
				!IPC_VerifyBuffer(ipc_command[8], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[8]) != banner_size ||
				!IPC_VerifyBuffer(ipc_command[10], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[10]) != buffer_size
			)

			Handle export = ipc_command[5];
			void *output_info = (void *)ipc_command[7];
			void *banner = (void *)ipc_command[9];
			void *buffer = (void *)ipc_command[11];

			Result res = AM_ReadTWLBackupInfo(buffer_size, output_info_size, banner_size, V2_11ContentSectionsDefault0, export, buffer, output_info, banner);

			ipc_command[0] = IPC_MakeHeader(0x001E, 1, 6);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(output_info_size, IPC_BUFFER_W);
			ipc_command[3] = (u32)output_info;
			ipc_command[4] = IPC_Desc_Buffer(banner_size, IPC_BUFFER_W);
			ipc_command[5] = (u32)banner;
			ipc_command[6] = IPC_Desc_Buffer(buffer_size, IPC_BUFFER_W);
			ipc_command[7] = (u32)buffer;
		}
		break;
	case 0x001F: // delete all expired user titles
		{
			CHECK_HEADER(0x001F, 1, 0)

			MediaType media_type = (MediaType)ipc_command[1];

			Result res = AM9_DeleteExpiredUserTitles(media_type);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x001F, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0020: // get TWL archive resource info
		{
			CHECK_HEADER(0x0020, 0, 0)

			TWLArchiveResourceInfo info;

			Result res = AM9_GetTWLArchiveResourceInfo(&info);
			assertNotAmOrFsWithMedia(res, MediaType_NAND);

			ipc_command[0] = IPC_MakeHeader(0x0020, 9, 0);
			ipc_command[1] = res;
			_memcpy32_aligned(&ipc_command[2], &info, sizeof(TWLArchiveResourceInfo));
		}
		break;
	case 0x0021: // get personalized ticket info list
		{
			CHECK_HEADER(0x0021, 1, 2)

			u32 amount = ipc_command[1];
			u32 objcount = amount * sizeof(TicketInfo);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[2]) != objcount
			)

			TicketInfo *infos = (TicketInfo *)ipc_command[3];

			u32 count = 0;

			Result res = AM9_GetPersonalizedTicketInfos(&count, amount, infos);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0021, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(objcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)infos;
		}
		break;
	case 0x0022: // delete all import title contexts (pending titles) (filtered)
		{
			CHECK_HEADER(0x0022, 2, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u8 filter = (u8)ipc_command[2];

			Result res = AM9_DeletePendingTitles(media_type, filter);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0022, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0023: // get import title context count (pending titles) (filtered)
		{
			CHECK_HEADER(0x0023, 2, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u8 filter = (u8)ipc_command[2];

			u32 count = 0;

			Result res = AM9_GetPendingTitleCount(&count, media_type, filter);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0023, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x0024: // get import title context list (pending titles) (filtered)
		{
			CHECK_HEADER(0x0024, 3, 2)

			u32 amount = ipc_command[1];
			u32 objcount = amount * sizeof(u64);
			MediaType media_type = (MediaType)ipc_command[2];
			u8 filter = (u8)ipc_command[3];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[4]) != objcount
			)

			u64 *title_ids = (u64 *)ipc_command[5];

			u32 count = 0;

			Result res = AM9_GetPendingTitleList(&count, amount, media_type, filter, title_ids);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0024, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(objcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)title_ids;
		}
		break;
	case 0x0025: // check content rights
		{
			CHECK_HEADER(0x0025, 3, 0)

			u64 title_id = *((u64 *)(&ipc_command[1]));
			u16 content_index = (u16)ipc_command[3];

			u8 has_rights = 0;

			Result res = AM9_CheckContentRight(&has_rights, title_id, content_index);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0025, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = (u32)has_rights;
		}
		break;
	case 0x0026: // get ticket limit infos
		{
			CHECK_HEADER(0x0026, 1, 4)

			u32 count = ipc_command[1];
			u32 tobjcount = count * sizeof(u64);
			u32 lobjcount = count * sizeof(TicketLimitInfo);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[2]) != tobjcount ||
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[4]) != lobjcount
			)

			u64 *ticket_ids = (u64 *)ipc_command[3];
			TicketLimitInfo *infos = (TicketLimitInfo *)ipc_command[5];

			Result res = AM9_GetTicketLimitInfos(count, ticket_ids, infos);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0026, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)ticket_ids;
			ipc_command[4] = IPC_Desc_Buffer(lobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)infos;
		}
		break;
	case 0x0027: // get demo launch infos
		{
			CHECK_HEADER(0x0027, 1, 4)

			u32 count = ipc_command[1];
			u32 tcount = count * sizeof(u64);
			u32 objcount = count * sizeof(DemoLaunchInfo);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[2]) != tcount ||
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[4]) != objcount
			)

			u64 *title_ids = (u64 *)ipc_command[3];
			DemoLaunchInfo *infos = (DemoLaunchInfo *)ipc_command[5];

			AM_DemoDatabase_GetLaunchInfos(&GLOBAL_DemoDatabase, title_ids, count, infos);

			ipc_command[0] = IPC_MakeHeader(0x0027, 1, 4);
			ipc_command[1] = 0; // we don't really have possible errors unless nand is fucked
			ipc_command[2] = IPC_Desc_Buffer(tcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)title_ids;
			ipc_command[4] = IPC_Desc_Buffer(objcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)infos;
		}
		break;
	case 0x0028: // read TWL export info (12 content sections)
		{
			CHECK_HEADER(0x0028, 4, 8)

			u32 output_info_size = ipc_command[1];
			u32 banner_size = ipc_command[2];
			u32 buffer_size = ipc_command[3];
			// u32 unk = ipc_command[4] ? unused ?

			CHECK_WRONGARG
			(
				!IPC_VerifyMoveHandles(ipc_command[5], 1) ||
				!IPC_VerifyBuffer(ipc_command[7], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[7]) != output_info_size ||
				!IPC_VerifyBuffer(ipc_command[9], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[9]) != banner_size ||
				!IPC_VerifyBuffer(ipc_command[11], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[11]) != buffer_size
			)

			Handle export = ipc_command[6];
			void *output_info = (void *)ipc_command[8];
			void *banner = (void *)ipc_command[10];
			void *buffer = (void *)ipc_command[12];

			Result res = AM_ReadTWLBackupInfo(buffer_size, output_info_size, banner_size, V2_12ContentSections7, export, buffer, output_info, banner);

			ipc_command[0] = IPC_MakeHeader(0x0028, 1, 6);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(output_info_size, IPC_BUFFER_W);
			ipc_command[3] = (u32)output_info;
			ipc_command[4] = IPC_Desc_Buffer(banner_size, IPC_BUFFER_W);
			ipc_command[5] = (u32)banner;
			ipc_command[6] = IPC_Desc_Buffer(buffer_size, IPC_BUFFER_W);
			ipc_command[7] = (u32)buffer;
		}
		break;
	case 0x0029: // delete user programs (atomically?)
		{
			CHECK_HEADER(0x0029, 2, 2)

			MediaType media_type = (MediaType)ipc_command[1];
			u32 count = ipc_command[2];
			u32 tcount = sizeof(u64) * count;

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[3]) != tcount
			)

			u64 *title_ids = (u64 *)ipc_command[4];

			Result res = 0;

			for (u32 i = 0; i < count; i++)
				if (TitleID_IsAnySystem(title_ids[i]))
				{
					res = AM_TRYING_TO_UNINSTALL_SYSAPP;
					break;
				}

			if (R_SUCCEEDED(res))
				res = AM9_DeleteTitles(media_type, count, title_ids);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0029, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)title_ids;
		}
		break;
	case 0x002A: // get current existing content infos count
		{
			CHECK_HEADER(0x002A, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			u32 count = 0;

			Result res = AM9_GetInstalledContentInfoCount(&count, media_type, title_id);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x002A, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x002B: // get current existing content infos list
		{
			CHECK_HEADER(0x002B, 5, 2)

			u32 amount = ipc_command[1];
			u32 objcount = sizeof(ContentInfo) * amount;
			MediaType media_type = (MediaType)ipc_command[2];
			u64 title_id = *((u64 *)(&ipc_command[3]));
			u32 offset = ipc_command[5];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[6], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[6]) != objcount
			)

			ContentInfo *infos = (ContentInfo *)ipc_command[7];

			u32 count = 0;

			Result res = AM9_ListInstalledContentInfos(&count, amount, media_type, title_id, offset, infos);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x002B, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(objcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)infos;
		}
		break;
	case 0x002C: // get title infos (ignore platform ctr/ktr)
		{
			CHECK_HEADER(0x002C, 2, 4)

			MediaType media_type = (MediaType)ipc_command[1];
			u32 count = ipc_command[2];
			u32 tobjcount = sizeof(u64) * count;
			u32 iobjcount = sizeof(TitleInfo) * count;

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[3]) != tobjcount ||
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount
			)

			u64 *title_ids = (u64 *)ipc_command[4];
			TitleInfo *infos = (TitleInfo *)ipc_command[6];

			Result res = 0;

			for (u32 i = 0; i < count; i++)
			{
				if (TitleID_IsTWL(title_ids[i]))
					res = AM9_GetTitleInfos(media_type, 1, &title_ids[i], &infos[i]);
				else
				{
					// set n3ds systitle bit
					title_ids[i] |= 0x20000000;
					// if fail, should check without n3ds bit
					if (R_FAILED(res = AM9_GetTitleInfos(media_type, 1, &title_ids[i], &infos[i])))
					{
						title_ids[i] &= ~(0x20000000);
						res = AM9_GetTitleInfos(media_type, 1, &title_ids[i], &infos[i]);
					}
				}
				assertNotAmOrFsWithMedia(res, media_type);
			}

			ipc_command[0] = IPC_MakeHeader(0x002C, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)title_ids;
			ipc_command[4] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)infos;
		}
		break;
	case 0x002D: // get content rights (ignore platform ctr/ktr)
		{
			CHECK_HEADER(0x002D, 3, 0)

			u64 title_id = *((u64 *)(&ipc_command[1]));
			u16 content_index = (u16)ipc_command[3];

			Result res = 0;
			u8 has_right = 0;

			if (TitleID_IsTWL(title_id))
				res = AM9_CheckContentRight(&has_right, title_id, content_index);
			else
			{
				// set n3ds systitle bit
				title_id |= 0x20000000;
				// if fail or no right, should check without n3ds bit
				if (R_FAILED(AM9_CheckContentRight(&has_right, title_id, content_index)) || !has_right)
					res = AM9_CheckContentRight(&has_right, title_id &= ~(0x20000000), content_index);
			}
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x002D, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = (u32)has_right;
		}
		break;
	}
}

static void AM_HandleIPC_Range0x401_0x419(AM_SessionData *session)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;
	u32 cmd_header = ipc_command[0];

	switch ((cmd_header >> 16) & 0xFFFF)
	{
	case 0x0401: // install FIRM
		{
			CHECK_HEADER(0x0401, 2, 0)

			u64 title_id = *((u64 *)(&ipc_command[1]));

			Result res = AM9_InstallFIRM(title_id);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0401, 1, 0);
			ipc_command[1] = res;
		}	
		break;
	case 0x0402: // begin user CIA install
		{
			CHECK_HEADER(0x0402, 1, 0)

			MediaType media_type = (MediaType)ipc_command[1];

			Handle import = 0;

			Result res = AM_Pipe_CreateCIAImportHandle(&GLOBAL_PipeManager, media_type, TitleDB, false, false, &import);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0402, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_MoveHandles(1);
			ipc_command[3] = import;
		}
		break;
	case 0x0403: // begin user CIA install in tempdb
		{
			CHECK_HEADER(0x0403, 0, 0)

			Handle import = 0;

			Result res = AM_Pipe_CreateCIAImportHandle(&GLOBAL_PipeManager, MediaType_NAND, TempDB, false, false, &import);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0403, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_MoveHandles(1);
			ipc_command[3] = import;
		}
		break;
	case 0x0404: // cancel CIA install
		{
			CHECK_HEADER(0x0404, 0, 2)

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[1], 1))

			Handle import = ipc_command[2];

			Result res = 0;

			if (!GLOBAL_PipeManager.write)
				res = AM_INVALID_CONTENT_IMPORT_STATE;

			if (R_SUCCEEDED(res))
			{
				CIAInstallData *c = (CIAInstallData *)GLOBAL_PipeManager.data;

				if (c->buf) { free(c->buf); c->buf = NULL; }
				if (c->importing_tik)     AM9_InstallTicketCancel();
				if (c->importing_tmd)     AM9_InstallTMDCancel();
				if (c->importing_content) AM9_InstallContentCancel();
				if (c->importing_title)   AM9_InstallTitleCancel();

				AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);
			}

			ipc_command[0] = IPC_MakeHeader(0x0404, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0405: // finish CIA install
		{
			CHECK_HEADER(0x0405, 0, 2)

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[1], 1))

			Handle import = ipc_command[2];

			Result res = 0;

			if (!GLOBAL_PipeManager.write)
				res = AM_INVALID_CONTENT_IMPORT_STATE;

			if (R_SUCCEEDED(res))
			{
				AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);

				CIAInstallData *c = (CIAInstallData *)GLOBAL_PipeManager.data;

				res = AM9_InstallTitleFinish();
				assertNotAmOrFsWithMedia(res, c->media);

				if (R_SUCCEEDED(res))
				{
					res = AM9_InstallTitlesCommit(c->media, 1, c->db_type, &c->tmd_tid);
					assertNotAmOrFsWithMedia(res, c->media);
				}
			}

			ipc_command[0] = IPC_MakeHeader(0x405, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0406: // finish CIA install without commit
		{
			CHECK_HEADER(0x0406, 0, 2)

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[1], 1))

			Handle import = ipc_command[2];

			AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);

			MediaType media_type = ((CIAInstallData *)GLOBAL_PipeManager.data)->media;

			Result res = AM9_InstallTitleFinish();
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0406, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0407: // install titles commit
		{
			CHECK_HEADER(0x0407, 3, 2)

			MediaType media_type = (MediaType)ipc_command[1];
			u32 count = ipc_command[2];
			u32 tobjcount = count * sizeof(u64);
			u8 db_type = ipc_command[3];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[4]) != tobjcount
			)

			u64 *title_ids = (u64 *)ipc_command[5];

			Result res = AM9_InstallTitlesCommit(media_type, count, db_type, title_ids);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0407, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)title_ids;
		}
		break;
	case 0x0408: // get title info from cia
		{
			CHECK_HEADER(0x0408, 1, 2)

			MediaType media_type = (MediaType)ipc_command[1];

			CHECK_WRONGARG(!IPC_VerifySharedHandles(ipc_command[2], 1))

			Handle cia = ipc_command[3];

			TitleInfo info;
			CIAReader reader;

			Result res = CIAReader_Init(&reader, cia, true);
			assertNotAmOrFs(res);

			if (R_SUCCEEDED(res))
			{
				res = CIAReader_GetTitleInfo(&reader, media_type, &info);
				assertNotAmOrFs(res);
			}

			CIAReader_Close(&reader);

			ipc_command[0] = IPC_MakeHeader(0x0408, 7, 0);
			ipc_command[1] = res;
			_memcpy32_aligned(&ipc_command[2], &info, sizeof(TitleInfo));
		}
		break;
	case 0x0409: // extract SMDH from cia meta region
		{
			CHECK_HEADER(0x0409, 0, 4)

			CHECK_WRONGARG
			(
				!IPC_VerifySharedHandles(ipc_command[1], 1) ||
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[3]) != 0x36C0 /* smdh size */
			)

			Handle cia = ipc_command[2];
			void *smdh = (void *)ipc_command[4];

			CIAReader reader;

			Result res = CIAReader_Init(&reader, cia, false);
			assertNotAmOrFs(res);

			if (R_SUCCEEDED(res))
			{
				res = CIAReader_ExtractMetaSMDH(&reader, smdh);
				assertNotAmOrFs(res);
			}

			CIAReader_Close(&reader);

			ipc_command[0] = IPC_MakeHeader(0x0409, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(0x36C0, IPC_BUFFER_W);
			ipc_command[3] = (u32)smdh;
		}
		break;
	case 0x040A: // extract title id dependency list from cia meta region
		{
			CHECK_HEADER(0x040A, 0, 2)

			CHECK_WRONGARG(!IPC_VerifySharedHandles(ipc_command[1], 1))

			Handle cia = ipc_command[2];

			CIAReader reader;

			Result res = CIAReader_Init(&reader, cia, false);
			assertNotAmOrFs(res);

			if (R_SUCCEEDED(res))
			{
				res = CIAReader_ExtractDependencyList(&reader, session->cia_deplist_buf);
				assertNotAmOrFs(res);
			}

			CIAReader_Close(&reader);

			ipc_command[0] = IPC_MakeHeader(0x040A, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_StaticBuffer(0x300, 0);
			ipc_command[3] = (u32)session->cia_deplist_buf;
		}
		break;
	case 0x040B: // get cia meta start offset / get cia transfer size
		{
			CHECK_HEADER(0x040B, 0, 2)

			CHECK_WRONGARG(!IPC_VerifySharedHandles(ipc_command[1], 1))

			Handle cia = ipc_command[2];

			union
			{
				u64 meta_start_offset;
				u64 transfer_size;
				u64 x;
			} a = { .x = 0 };
			CIAReader reader;

			Result res = CIAReader_Init(&reader, cia, false);
			assertNotAmOrFs(res);

			if (R_SUCCEEDED(res))
				a.x = CON_END(&reader);

			CIAReader_Close(&reader);

			ipc_command[0] = IPC_MakeHeader(0x040B, 3, 0);
			ipc_command[1] = res;
			ipc_command[2] = LODWORD(a.x);
			ipc_command[3] = HIDWORD(a.x);
		}
		break;
	case 0x040C: // get cia meta core version
		{
			CHECK_HEADER(0x040C, 0, 2)

			CHECK_WRONGARG(!IPC_VerifySharedHandles(ipc_command[1], 1))

			Handle cia = ipc_command[2];

			CIAReader reader;
			u32 core_version = 0;

			Result res = CIAReader_Init(&reader, cia, false);
			assertNotAmOrFs(res);

			if (R_SUCCEEDED(res))
			{
				res = CIAReader_ExtractMetaCoreVersion(&reader, &core_version);
				assertNotAmOrFs(res);
			}

			CIAReader_Close(&reader);

			ipc_command[0] = IPC_MakeHeader(0x040C, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = core_version;
		}
		break;
	case 0x040D: // get cia required installation space
		{
			CHECK_HEADER(0x040D, 1, 2)

			MediaType media_type = (MediaType)ipc_command[1];

			CHECK_WRONGARG(!IPC_VerifySharedHandles(ipc_command[2], 1))

			Handle cia = ipc_command[3];

			CIAReader reader;
			u64 required_size = 0;

			Result res = CIAReader_Init(&reader, cia, true);
			assertNotAmOrFs(res);

			if (R_SUCCEEDED(res))
			{
				res = CIAReader_CalculateRequiredSize(&reader, media_type, &required_size);
				assertNotAmOrFs(res);
			}

			CIAReader_Close(&reader);

			ipc_command[0] = IPC_MakeHeader(0x040D, 3, 0);
			ipc_command[1] = res;
			ipc_command[2] = LODWORD(required_size);
			ipc_command[3] = HIDWORD(required_size);
		}
		break;
	case 0x040E: // install titles commit (with optional firm upgrade)
		{
			CHECK_HEADER(0x040E, 3, 2)

			MediaType media_type = (MediaType)ipc_command[1];
			u32 count = ipc_command[2];
			u32 tobjcount = count * sizeof(u64);
			u8 db_type = (u8)ipc_command[3];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[4]) != tobjcount
			)

			u64 *title_ids = (u64 *)ipc_command[5];

			Result res = AM_InstallTitlesCommit_OptionalFirmUpgrade(media_type, count, db_type, title_ids);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x040E, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)title_ids;
		}
		break;
	case 0x040F: // update firmware (auto)
		{
			CHECK_HEADER(0x040F, 0, 0)

			u64 firm_tid = 0;

			Result res = findInstalledFirmTitleIdNewerThanRunning(&firm_tid);

			if (R_SUCCEEDED(res))
			{
				res = AM9_InstallFIRM(firm_tid);
				assertNotAm(res);
			}

			ipc_command[0] = IPC_MakeHeader(0x040F, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0410: // delete title
		{
			CHECK_HEADER(0x0410, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			Result res = AM9_DeleteTitle(media_type, title_id);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0410, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0411: // get TWL title/contents list for reboot
		{
			CHECK_HEADER(0x0411, 1, 4)

			u32 amount = ipc_command[1];
			u32 tobjcount = amount * sizeof(u64);
			u32 cobjcount = amount * sizeof(u32);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[2]) != tobjcount ||
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[4]) != cobjcount
			)

			u64 *title_ids = (u64 *)ipc_command[3];
			u32 *content_ids = (u32 *)ipc_command[5];

			u32 count = 0;

			Result res = AM9_GetTWLTitleListForReboot(&count, amount, title_ids, content_ids);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0411, 2, 4);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)title_ids;
			ipc_command[5] = IPC_Desc_Buffer(cobjcount, IPC_BUFFER_W);
			ipc_command[6] = (u32)content_ids;
		}
		break;
	case 0x0412: // get system updater mutex
		{
			CHECK_HEADER(0x0412, 0, 0)

			ipc_command[0] = IPC_MakeHeader(0x0412, 1, 2);
			ipc_command[1] = 0;
			ipc_command[2] = IPC_Desc_SharedHandles(1);
			ipc_command[3] = GLOBAL_SystemUpdaterMutex;
		}
		break;
	case 0x0413: // get cia meta size
		{
			CHECK_HEADER(0x0413, 0, 2)

			CHECK_WRONGARG(!IPC_VerifySharedHandles(ipc_command[1], 1))

			Handle cia = ipc_command[2];

			CIAReader reader;
			u32 meta_size = 0;

			Result res = CIAReader_Init(&reader, cia, false);
			assertNotAmOrFs(res);

			if (R_SUCCEEDED(res))
				meta_size = reader.header.MetaSize;

			CIAReader_Close(&reader);

			ipc_command[0] = IPC_MakeHeader(0x0413, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = meta_size;
		}
		break;
	case 0x0414: // get cia meta
		{
			CHECK_HEADER(0x0414, 1, 2)

			u32 size = ipc_command[1];

			CHECK_WRONGARG
			(
				!IPC_VerifySharedHandles(ipc_command[2], 1) ||
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[4]) != size
			)

			Handle cia = ipc_command[3];
			void *meta = (void *)ipc_command[5];

			CIAReader reader;
			u32 read = 0;

			Result res = CIAReader_Init(&reader, cia, false);
			assertNotAmOrFs(res);

			if (R_SUCCEEDED(res))
			{
				res = CIAReader_ExtractMeta(&reader, size, meta, &read);
				assertNotAmOrFs(res);
			}

			CIAReader_Close(&reader);

			ipc_command[0] = IPC_MakeHeader(0x0414, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = read;
			ipc_command[3] = IPC_Desc_Buffer(size, IPC_BUFFER_W);
			ipc_command[4] = (u32)meta;
		}
		break;
	case 0x0415: // check demo launch right
		{
			CHECK_HEADER(0x0415, 2, 0)

			u64 title_id = *((u64 *)(&ipc_command[1]));

			bool has_right = AM_DemoDatabase_HasDemoLaunchRight(&GLOBAL_DemoDatabase, title_id);

			ipc_command[0] = IPC_MakeHeader(0x0415, 2, 0);
			ipc_command[1] = 0;
			ipc_command[2] = (u32)has_right;
		}
		break;
	case 0x0416: // get internal title location info (what?)
		{
			CHECK_HEADER(0x0416, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			InternalTitleLocationInfo info;

			Result res = AM9_GetInternalTitleLocationInfo(&info, media_type, title_id);
			// no assert?

			ipc_command[0] = IPC_MakeHeader(0x0416, 9, 0);
			ipc_command[1] = res;
			_memcpy32_aligned(&ipc_command[2], &info, sizeof(InternalTitleLocationInfo));
		}
		break;
	case 0x0417: // migrate AGB to SAV
		{
			CHECK_HEADER(0x0417, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			Result res = AM9_MigrateAGBToSAV(media_type, title_id);
			// no assert?

			ipc_command[0] = IPC_MakeHeader(0x0417, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0418: // begin user CIA install (overwrite)
		{
			CHECK_HEADER(0x0418, 1, 0)

			MediaType media_type = (MediaType)ipc_command[1];

			Handle import = 0;

			Result res = AM_Pipe_CreateCIAImportHandle(&GLOBAL_PipeManager, media_type, TitleDB, true, false, &import);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x0418, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_MoveHandles(1);
			ipc_command[3] = import;
		}
		break;
	case 0x0419: // begin system CIA install
		{
			CHECK_HEADER(0x0419, 0, 0)

			Handle import = 0;

			Result res = AM_Pipe_CreateCIAImportHandle(&GLOBAL_PipeManager, MediaType_NAND, TitleDB, false, true, &import);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0419, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_MoveHandles(1);
			ipc_command[3] = import;
		}
		break;
	}
}

static void AM_HandleIPC_Range0x1001_0x100D()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;
	u32 cmd_header = ipc_command[0];

	switch ((cmd_header >> 16) & 0xFFFF)
	{
	case 0x1001: // get dlc content info count
		{
			CHECK_HEADER(0x1001, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			Result res = 0;

			u32 count = 0;

			if (!TitleID_IsDLC(title_id))
				res = AM_GENERAL_INVALIDARG;
			else
				res = AM9_GetContentInfoCount(&count, media_type, title_id);

			ipc_command[0] = IPC_MakeHeader(0x1001, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x1002: // find dlc content infos
		{
			CHECK_HEADER(0x1002, 4, 4)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));
			u32 count = ipc_command[4];
			u32 iobjcount = count * sizeof(u16);
			u32 cobjcount = count * sizeof(ContentInfo);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount ||
				!IPC_VerifyBuffer(ipc_command[7], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[7]) != cobjcount
			)

			u16 *indices = (u16 *)ipc_command[6];
			ContentInfo *infos = (ContentInfo *)ipc_command[8];

			Result res = 0;

			if (!TitleID_IsDLC(title_id))
				res = AM_INVALID_TITLE_ID_HIGH;
			else
			{
				res = AM9_FindContentInfos(media_type, title_id, count, indices, infos);
				assertNotAmOrFsWithMedia(res, media_type);
			}

			ipc_command[0] = IPC_MakeHeader(0x1002, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)indices;
			ipc_command[4] = IPC_Desc_Buffer(cobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)infos;
		}
		break;
	case 0x1003: // list dlc content infos
		{
			CHECK_HEADER(0x1003, 5, 2)

			u32 amount = ipc_command[1];
			u32 cobjcount = amount * sizeof(ContentInfo);
			MediaType media_type = (MediaType)ipc_command[2];
			u64 title_id = *((u64 *)(&ipc_command[3]));
			u32 offset = ipc_command[5];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[6], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[6] != cobjcount)
			)

			ContentInfo *infos = (ContentInfo *)ipc_command[7];

			Result res = 0;

			u32 count = 0;

			if (!TitleID_IsDLC(title_id))
				AM_INVALID_TITLE_ID_HIGH;
			else
			{
				res = AM9_ListContentInfos(&count, amount, media_type, title_id, offset, infos);
				assertNotAmOrFsWithMedia(res, media_type);
			}

			ipc_command[0] = IPC_MakeHeader(0x1003, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(cobjcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)infos;
		}
		break;
	case 0x1004: // delete dlc contents
		{
			CHECK_HEADER(0x1004, 4, 2)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));
			u32 count = ipc_command[4];
			u32 iobjcount = count * sizeof(u16);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount
			)

			u16 *indices = (u16 *)ipc_command[6];

			Result res = 0;

			if (!TitleID_IsDLC(title_id))
				res = AM_INVALID_TITLE_ID_HIGH;
			else
			{
				res = AM9_DeleteContents(media_type, title_id, count, indices);
				assertNotAmOrFsWithMedia(res, media_type);
			}

			ipc_command[0] = IPC_MakeHeader(0x1004, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)indices;
		}
		break;
	case 0x1005: // get dlc title infos
		{
			CHECK_HEADER(0x1005, 2, 4)

			MediaType media_type = (MediaType)ipc_command[1];
			u32 count = ipc_command[2];
			u32 iobjcount = count * sizeof(u64);
			u32 cobjcount = count * sizeof(TitleInfo);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[3]) != iobjcount ||
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != cobjcount
			)

			u64 *title_ids = (u64 *)ipc_command[4];
			TitleInfo *infos = (TitleInfo *)ipc_command[6];

			Result res = 0;

			for (u32 i = 0; i < count; i++)
				if (!TitleID_IsDLC(title_ids[i]))
				{
					res = AM_INVALID_TITLE_ID_HIGH;
					break;
				}

			if (R_SUCCEEDED(res))
				res = AM9_GetTitleInfos(media_type, count, title_ids, infos);
			assertNotAmOrFsWithMedia(res, media_type);

			ipc_command[0] = IPC_MakeHeader(0x1005, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)title_ids;
			ipc_command[4] = IPC_Desc_Buffer(cobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)infos;
		}
		break;
	case 0x1006: // get dlc or license ticket count
		{
			CHECK_HEADER(0x1006, 2, 0)

			u64 title_id = *((u64 *)(&ipc_command[1]));

			Result res = 0;

			u32 count = 0;

			if (!TitleID_IsDLCOrLicense(title_id))
				res = AM_INVALID_TITLE_ID_HIGH;
			else
			{
				res = AM9_GetTitleInstalledTicketsCount(&count, title_id);
				assertNotAm(res);
			}

			ipc_command[0] = IPC_MakeHeader(0x1006, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x1007: // list dlc or license ticket infos
		{
			CHECK_HEADER(0x1007, 4, 2)

			u32 amount = ipc_command[1];
			u32 iobjcount = amount * sizeof(TicketInfo);
			u64 title_id = *((u64 *)(&ipc_command[2]));
			u32 offset = ipc_command[4];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount
			)

			TicketInfo *infos = (TicketInfo *)ipc_command[6];

			Result res = 0;

			u32 count = 0;

			if (!TitleID_IsDLCOrLicense(title_id))
				res = AM_INVALID_TITLE_ID_HIGH;
			else
			{
				res = AM9_ListTicketInfos(&count, amount, title_id, offset, infos);
				assertNotAm(res);
			}

			ipc_command[0] = IPC_MakeHeader(0x1007, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)infos;
		}
		break;
	case 0x1008: // get dlc or license item rights
		{
			CHECK_HEADER(0x1008, 7, 2)

			u32 size = ipc_command[1];
			u8 enum_val = (u8)ipc_command[2];
			u64 ticket_tid = *((u64 *)(&ipc_command[3]));
			u64 ticket_id = *((u64 *)(&ipc_command[5]));
			u32 offset = ipc_command[7];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[8], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[8]) != size
			)

			void *data = (void *)ipc_command[9];

			Result res = 0;

			u32 outval1 = 0, outval2 = 0;

			if (!TitleID_IsDLCOrLicense(ticket_tid))
				res = AM_INVALID_TITLE_ID_HIGH;
			else
			{
				res = AM9_GetItemRights(&outval1, &outval1, size, enum_val, ticket_tid, ticket_id, offset, data);
				assertNotAm(res);
			}

			ipc_command[0] = IPC_MakeHeader(0x1008, 3, 2);
			ipc_command[1] = res;
			ipc_command[2] = outval1;
			ipc_command[3] = outval2;
			ipc_command[4] = IPC_Desc_Buffer(size, IPC_BUFFER_W);
			ipc_command[5] = (u32)data;
		}
		break;
	case 0x1009: // dlc title is in use
		{
			CHECK_HEADER(0x1009, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			Result res = 0;

			u8 in_use = 0;

			if (!TitleID_IsDLC(title_id))
				res = AM_INVALID_TITLE_ID_HIGH;
			else
			{
				res = AM9_TitleInUse(&in_use, media_type, title_id);
				assertNotAmOrFsWithMedia(res, media_type);
			}

			ipc_command[0] = IPC_MakeHeader(0x1009, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = (u32)in_use;
		}
		break;
	case 0x100A: // reload SD title database
		{
			CHECK_HEADER(0x100A, 0, 0)

			u8 available = 0;

			Result res = AM9_ReloadTitleDatabase(&available, MediaType_SD);
			assertNotAmOrFsWithMedia(res, MediaType_SD);

			ipc_command[0] = IPC_MakeHeader(0x100A, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = (u32)available;
		}
		break;
	case 0x100B: // get existing dlc content infos count
		{
			CHECK_HEADER(0x100B, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			Result res = 0;

			u32 count = 0;

			if (!TitleID_IsDLC(title_id))
				res = AM_GENERAL_INVALIDARG;
			else
				res = AM9_GetInstalledContentInfoCount(&count, media_type, title_id);

			ipc_command[0] = IPC_MakeHeader(0x100B, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x100C: // list existing dlc content infos
		{
			CHECK_HEADER(0x100C, 5, 2)

			u32 amount = ipc_command[1];
			u32 cobjcount = amount * sizeof(ContentInfo);
			MediaType media_type = (MediaType)ipc_command[2];
			u64 title_id = *((u64 *)(&ipc_command[3]));
			u32 offset = ipc_command[5];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[6], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[6]) != cobjcount
			)

			ContentInfo *infos = (ContentInfo *)ipc_command[7];

			Result res = 0;

			u32 count = 0;

			if (!TitleID_IsDLC(title_id))
				res = AM_INVALID_TITLE_ID_HIGH;
			else
			{
				res = AM9_ListInstalledContentInfos(&count, amount, media_type, title_id, offset, infos);
				assertNotAmOrFsWithMedia(res, media_type);
			}

			ipc_command[0] = IPC_MakeHeader(0x100C, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(cobjcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)infos;
		}
		break;
	case 0x100D: // list update (patch) title infos
		{
			CHECK_HEADER(0x100D, 2, 4)

			MediaType media_type = (MediaType)ipc_command[1];
			u32 count = ipc_command[2];
			u32 tobjcount = count * sizeof(u64);
			u32 iobjcount = count * sizeof(TitleInfo);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[3]) != tobjcount ||
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount
			)

			u64 *title_ids = (u64 *)ipc_command[4];
			TitleInfo *infos = (TitleInfo *)ipc_command[6];

			Result res = 0;

			for (u32 i = 0; i < count; i++)
				if (!TitleID_IsPatch(title_ids[i]))
				{
					res = AM_INVALID_TITLE_ID_HIGH;
					break;
				}

			if (R_SUCCEEDED(res))
			{
				res = AM9_GetTitleInfos(media_type, count, title_ids, infos);
				assertNotAmOrFsWithMedia(res, media_type);
			}

			ipc_command[0] = IPC_MakeHeader(0x100D, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)title_ids;
			ipc_command[4] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)infos;
		}
		break;
	}
}

static void AM_HandleIPC_Range0x801_0x829(AM_SessionData *session)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;
	u32 cmd_header = ipc_command[0];

	switch ((cmd_header >> 16) & 0xFFFF)
	{
	case 0x0801: // install ticket begin
		{
			CHECK_HEADER(0x0801, 0, 0)

			Result res = AM9_InstallTicketBegin();
			assertNotAm(res);

			Handle import = 0;

			if (R_SUCCEEDED(res))
			{
				res = AM_Pipe_CreateTicketImportHandle(&GLOBAL_PipeManager, &import);
				assertNotAm(res);
			}

			ipc_command[0] = IPC_MakeHeader(0x0801, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_MoveHandles(1);
			ipc_command[3] = import;
		}
		break;
	case 0x0802: // install ticket cancel
		{
			CHECK_HEADER(0x0802, 0, 2)

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[1], 1))

			Handle import = ipc_command[2];

			Result res = AM9_InstallTicketCancel();
			assertNotAm(res);

			AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);

			ipc_command[0] = IPC_MakeHeader(0x0802, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0803: // install ticket finish
		{
			CHECK_HEADER(0x0803, 0, 2)

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[1], 1))

			Handle import = ipc_command[2];

			Result res = AM9_InstallTicketFinish();
			assertNotAm(res);
			
			AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);

			ipc_command[0] = IPC_MakeHeader(0x0803, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0804: // install title begin
		{
			CHECK_HEADER(0x0804, 4, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));
			u8 db_type = (u8)ipc_command[4];

			Result res = 0;

			if (session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallTitleBegin(media_type, title_id, db_type);
				assertNotAmOrFsWithMedia(res, media_type);

				if (R_SUCCEEDED(res))
				{
					session->media = media_type;
					session->importing_title = true;
				}
			}

			ipc_command[0] = IPC_MakeHeader(0x0804, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0805: // install title pause
		{
			CHECK_HEADER(0x0805, 0, 0)

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				AM_Pipe_EnsureThreadExit(&GLOBAL_PipeManager);

				session->importing_title = false;

				res = AM9_InstallTitlePause();
				assertNotAmOrFsWithMedia(res, session->media);
			}

			ipc_command[0] = IPC_MakeHeader(0x0805, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0806: // install title resume
		{
			CHECK_HEADER(0x0806, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			Result res = 0;

			if (session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallTitleResume(media_type, title_id);
				assertNotAmOrFsWithMedia(res, media_type);

				if (R_SUCCEEDED(res))
				{
					session->importing_title = true;
					session->media = media_type;
				}
			}

			ipc_command[0] = IPC_MakeHeader(0x0806, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0807: // install title cancel
		{
			CHECK_HEADER(0x0807, 0, 0)

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				AM_Pipe_EnsureThreadExit(&GLOBAL_PipeManager);

				session->importing_title = false;

				res = AM9_InstallTitleCancel();
				assertNotAmOrFsWithMedia(res, session->media);
			}

			ipc_command[0] = IPC_MakeHeader(0x0807, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0808: // install title finish
		{
			CHECK_HEADER(0x0808, 0, 0)

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				session->importing_title = false;

				res = AM9_InstallTitleFinish();
				assertNotAmOrFsWithMedia(res, session->media);
			}

			ipc_command[0] = IPC_MakeHeader(0x0808, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0809: // install titles commit
		{
			CHECK_HEADER(0x0809, 3, 2)

			MediaType media_type = (MediaType)ipc_command[1];
			u32 count = ipc_command[2];
			u32 tobjcount = count * sizeof(u64);
			u8 db_type = ipc_command[3];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[4]) != tobjcount
			)

			u64 *title_ids = (u64 *)ipc_command[5];
			Result res = AM9_InstallTitlesCommit(media_type, count, db_type, title_ids);

			ipc_command[0] = IPC_MakeHeader(0x0809, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)title_ids;
		}
		break;
	case 0x080A: // install TMD begin
		{
			CHECK_HEADER(0x080A, 0, 0)

			Result res = 0;

			Handle import = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallTMDBegin();
				assertNotAmOrFsWithMedia(res, session->media);

				if (R_SUCCEEDED(res))
				{
					res = AM_Pipe_CreateTMDImportHandle(&GLOBAL_PipeManager, &import);
					assertNotAmOrFsWithMedia(res, session->media);
				}
			}

			ipc_command[0] = IPC_MakeHeader(0x080A, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_MoveHandles(1);
			ipc_command[3] = import;
		}
		break;
	case 0x080B: // install TMD cancel
		{
			CHECK_HEADER(0x080B, 0, 2)

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[1], 1))

			Handle import = ipc_command[2];

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallTMDCancel();
				assertNotAmOrFsWithMedia(res, session->media);

				AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);
			}

			ipc_command[0] = IPC_MakeHeader(0x080B, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x080C: // install TMD finish
		{
			CHECK_HEADER(0x080C, 1, 2)

			bool unk = (bool)ipc_command[1];

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[2], 1))

			Handle import = ipc_command[3];

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallTMDFinish(unk);
				assertNotAmOrFsWithMedia(res, session->media);

				AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);
			}

			ipc_command[0] = IPC_MakeHeader(0x080C, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x080D: // create import content contexts
		{
			CHECK_HEADER(0x080D, 1, 2)

			u32 count = ipc_command[1];
			u32 iobjcount = count * sizeof(u16);

			CHECK_WRONGARG(!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_R))

			u16 *indices = (u16 *)ipc_command[3];

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_CreateImportContentContexts(count, indices);
				assertNotAmOrFsWithMedia(res, session->media);
			}

			ipc_command[0] = IPC_MakeHeader(0x080D, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)indices;
		}
		break;
	case 0x080E: // install content begin
		{
			CHECK_HEADER(0x080E, 1, 0)

			u16 content_index = (u16)ipc_command[1];

			Result res = 0;

			Handle import = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallContentBegin(content_index);
				assertNotAmOrFsWithMedia(res, session->media);

				if (R_SUCCEEDED(res))
				{
					res = AM_Pipe_CreateContentImportHandle(&GLOBAL_PipeManager, &import);
					assertNotAmOrFsWithMedia(res, session->media);
				}
			}

			ipc_command[0] = IPC_MakeHeader(0x080E, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_MoveHandles(1);
			ipc_command[3] = import;
		}
		break;
	case 0x080F: // install content pause
		{
			CHECK_HEADER(0x080F, 0, 2)

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[1], 1))

			Handle import = ipc_command[2];

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallContentPause();
				assertNotAmOrFsWithMedia(res, session->media);
			}

			AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);

			ipc_command[0] = IPC_MakeHeader(0x080F, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x810: // install content resume
		{
			CHECK_HEADER(0x0810, 1, 0)

			u16 content_index = (u16)ipc_command[1];

			Result res = 0;

			u64 resume_offset = 0;
			Handle import = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallContentResume(content_index, &resume_offset);
				assertNotAmOrFsWithMedia(res, session->media);

				if (R_SUCCEEDED(res))
				{
					res = AM_Pipe_CreateContentImportHandle(&GLOBAL_PipeManager, &import);
					assertNotAmOrFsWithMedia(res, session->media);
				}
			}

			ipc_command[0] = IPC_MakeHeader(0x0810, 3, 2);
			ipc_command[1] = res;
			ipc_command[2] = LODWORD(resume_offset);
			ipc_command[3] = HIDWORD(resume_offset);
			ipc_command[4] = IPC_Desc_MoveHandles(1);
			ipc_command[5] = import;
		}
		break;
	case 0x0811: // install content cancel
		{
			CHECK_HEADER(0x0811, 0, 2)

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[1], 1))

			Handle import = ipc_command[2];

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallContentCancel();
				assertNotAmOrFsWithMedia(res, session->media);

				AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);
			}

			ipc_command[0] = IPC_MakeHeader(0x0811, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0812: // install content finish
		{
			CHECK_HEADER(0x0812, 0, 2)

			CHECK_WRONGARG(!IPC_VerifyMoveHandles(ipc_command[1], 1))

			Handle import = ipc_command[2];

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallContentFinish();
				assertNotAmOrFsWithMedia(res, session->media);

				AM_Pipe_CloseImportHandle(&GLOBAL_PipeManager, import);
			}

			ipc_command[0] = IPC_MakeHeader(0x0812, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0813: // get current import content contexts count
		{
			CHECK_HEADER(0x0813, 0, 0)

			Result res = 0;

			u32 count = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_GetCurrentImportContentContextsCount(&count);
				assertNotAmOrFsWithMedia(res, session->media);
			}

			ipc_command[0] = IPC_MakeHeader(0x0813, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x0814: // get current import contexts list
		{
			CHECK_HEADER(0x0814, 1, 2)

			u32 amount = ipc_command[1];
			u32 tobjcount = amount * sizeof(u16);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[2]) != tobjcount
			)

			u16 *indices = (u16 *)ipc_command[3];

			Result res = 0;

			u32 count = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_GetCurrentImportContentContextsList(&count, amount, indices);
				assertNotAmOrFsWithMedia(res, session->media);
			}

			ipc_command[0] = IPC_MakeHeader(0x0814, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)indices;
		}
		break;
	case 0x0815: // get current import content contexts
		{
			CHECK_HEADER(0x0815, 1, 4)

			u32 count = ipc_command[1];
			u32 tobjcount = count * sizeof(u16);
			u32 cobjcount = count * sizeof(ImportContentContext);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[2]) != tobjcount ||
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[4]) != cobjcount
			)

			u16 *indices = (u16 *)ipc_command[3];
			ImportContentContext *contexts = (ImportContentContext *)ipc_command[5];

			Result res = 0;

			if (!session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_GetCurrentImportContentContexts(count, indices, contexts);
				assertNotAmOrFsWithMedia(res, session->media);
			}

			ipc_command[0] = IPC_MakeHeader(0x0815, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)indices;
			ipc_command[4] = IPC_Desc_Buffer(cobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)contexts;
		}
		break;
	case 0x0816: // sign
		{
			CHECK_HEADER(0x0816, 5, 6)

			u32 signature_output_size = ipc_command[1];
			u32 certificate_output_size = ipc_command[2];
			u64 title_id = *((u64 *)(&ipc_command[3]));
			u32 input_size = ipc_command[5];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[6], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[6]) != input_size ||
				!IPC_VerifyBuffer(ipc_command[8], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[8]) != signature_output_size ||
				!IPC_VerifyBuffer(ipc_command[10], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[10]) != certificate_output_size
			)

			void *input = (void *)ipc_command[7];
			void *signature = (void *)ipc_command[9];
			void *certificate = (void *)ipc_command[11];

			u8 retval = 0;

			Result res = AM9_Sign(&retval, signature_output_size, certificate_output_size, title_id, input_size, input, signature, certificate);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0816, 2, 6);
			ipc_command[1] = res;
			ipc_command[2] = (u32)retval;
			ipc_command[3] = IPC_Desc_Buffer(input_size, IPC_BUFFER_R);
			ipc_command[4] = (u32)input;
			ipc_command[5] = IPC_Desc_Buffer(signature_output_size, IPC_BUFFER_W);
			ipc_command[6] = (u32)signature;
			ipc_command[7] = IPC_Desc_Buffer(certificate_output_size, IPC_BUFFER_W);
			ipc_command[8] = (u32)certificate;
		}
		break;
	case 0x0817: // verify (stubbed)
		{
			CHECK_HEADER(0x0817, 5, 6)

			u64 unk64 = *((u64 *)(&ipc_command[1]));
			u32 size0 = ipc_command[3], size1 = ipc_command[4], size2 = ipc_command[5];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[6], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[6]) != size0 ||
				!IPC_VerifyBuffer(ipc_command[8], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[8]) != size1 ||
				!IPC_VerifyBuffer(ipc_command[10], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[10]) != size2
			)

			void *buf0 = (void *)ipc_command[7], *buf1 = (void *)ipc_command[9], *buf2 = (void *)ipc_command[11];
			(void)buf0, (void)buf1, (void)buf2, (void)unk64;

			u32 unk = 0; // stock am doesn't initialize this value to anything, i'll just use 0

			ipc_command[0] = IPC_MakeHeader(0x0817, 2, 6);
			ipc_command[1] = AM_NOT_IMPLEMENTED;
			ipc_command[2] = unk;
			ipc_command[3] = IPC_Desc_Buffer(size0, IPC_BUFFER_R);
			ipc_command[4] = (u32)buf0;
			ipc_command[5] = IPC_Desc_Buffer(size1, IPC_BUFFER_R);
			ipc_command[6] = (u32)buf1;
			ipc_command[7] = IPC_Desc_Buffer(size2, IPC_BUFFER_R);
			ipc_command[8] = (u32)buf2;
		}
		break;
	case 0x0818: // get device (CT) certificate
		{
			CHECK_HEADER(0x0818, 1, 2)

			u32 size = ipc_command[1];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[2]) != size
			)

			void *certificate = (void *)ipc_command[3];

			u32 ret = 0;

			Result res = AM9_GetDeviceCertificate(&ret, size, certificate);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0818, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = ret;
			ipc_command[3] = IPC_Desc_Buffer(size, IPC_BUFFER_W);
			ipc_command[4] = (u32)certificate;
		}
		break;
	case 0x0819: // import certificates (why are these ipcs so loooong)
		{
			CHECK_HEADER(0x0819, 4, 8)

			u32 c0_size = ipc_command[1];
			u32 c1_size = ipc_command[2];
			u32 c2_size = ipc_command[3];
			u32 c3_size = ipc_command[4];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[5]) != c0_size ||
				!IPC_VerifyBuffer(ipc_command[7], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[7]) != c1_size ||
				!IPC_VerifyBuffer(ipc_command[9], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[9]) != c2_size ||
				!IPC_VerifyBuffer(ipc_command[11], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[11]) != c3_size
			)

			void *c0 = (void *)ipc_command[6];
			void *c1 = (void *)ipc_command[8];
			void *c2 = (void *)ipc_command[10];
			void *c3 = (void *)ipc_command[12];

			Result res = AM9_ImportCertificates(c0_size, c1_size, c2_size, c3_size, c0, c1, c2, c3);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0819, 1, 8);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(c0_size, IPC_BUFFER_R);
			ipc_command[3] = (u32)c0;
			ipc_command[4] = IPC_Desc_Buffer(c1_size, IPC_BUFFER_R);
			ipc_command[5] = (u32)c1;
			ipc_command[6] = IPC_Desc_Buffer(c2_size, IPC_BUFFER_R);
			ipc_command[7] = (u32)c2;
			ipc_command[8] = IPC_Desc_Buffer(c3_size, IPC_BUFFER_R);
			ipc_command[9] = (u32)c3;
		}
		break;
	case 0x081A: // import certificate
		{
			CHECK_HEADER(0x081A, 1, 2)

			u32 size = ipc_command[1];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[2]) != size
			)

			void *certificate = (void *)ipc_command[3];

			Result res = AM9_ImportCertificate(size, certificate);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x081A, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(size, IPC_BUFFER_R);
			ipc_command[3] = (u32)certificate;
		}
		break;
	case 0x081B: // install titles commit (with optional firm upgrade)
		{
			CHECK_HEADER(0x081B, 3, 2)

			MediaType media_type = (MediaType)ipc_command[1];
			u32 count = ipc_command[2];
			u32 tobjcount = count * sizeof(u64);
			u8 db_type = (u8)ipc_command[3];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[4]) != tobjcount
			)

			u64 *title_ids = (u64 *)ipc_command[5];

			Result res = AM_InstallTitlesCommit_OptionalFirmUpgrade(media_type, count, db_type, title_ids);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x081B, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)title_ids;
		}
		break;
	case 0x081C: // delete ticket id
		{
			CHECK_HEADER(0x081C, 4, 0)

			u64 title_id = *((u64 *)(&ipc_command[1]));
			u64 ticket_id = *((u64 *)(&ipc_command[3]));

			Result res = AM9_DeleteTicketID(title_id, ticket_id);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x081C, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x081D: // get ticket id count for title (using title id)
		{
			CHECK_HEADER(0x081D, 2, 0)

			u64 title_id = *((u64 *)(&ipc_command[1]));

			u32 count = 0;

			Result res = AM9_GetTicketIDCount(&count, title_id);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x081D, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x081E: // get ticket id list
		{
			CHECK_HEADER(0x081E, 4, 2)

			u32 amount = ipc_command[1];
			u32 tobjcount = amount * sizeof(u64);
			u64 title_id = *((u64 *)(&ipc_command[2]));
			bool unk = (bool)ipc_command[4];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != tobjcount
			)

			u64 *ticket_ids = (u64 *)ipc_command[6];

			u32 count = 0;

			Result res = AM9_GetTicketIDList(&count, amount, title_id, unk, ticket_ids);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x081E, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)ticket_ids;
		}
		break;
	case 0x081F: // get ticket count for title (using title id)
		{
			CHECK_HEADER(0x081F, 2, 0)

			u64 title_id = *((u64 *)(&ipc_command[1]));

			u32 count = 0;

			Result res = AM9_GetTitleInstalledTicketsCount(&count, title_id);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x081F, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x0820: // list ticket infos
		{
			CHECK_HEADER(0x0820, 4, 2)

			u32 amount = ipc_command[1];
			u32 iobjcount = amount * sizeof(TicketInfo);
			u64 title_id = *((u64 *)(&ipc_command[2]));
			u32 offset = ipc_command[4];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount
			)

			TicketInfo *infos = (TicketInfo *)ipc_command[6];

			u32 count = 0;

			Result res = AM9_ListTicketInfos(&count, amount, title_id, offset, infos);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0820, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)infos;
		}
		break;
	case 0x0821: // export license ticket
		{
			CHECK_HEADER(0x0821, 5, 2)

			u32 size = ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));
			u64 ticket_id = *((u64 *)(&ipc_command[4]));

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[5]) != size
			)

			void *output = (void *)ipc_command[6];

			u32 actual_size = 0;

			Result res = AM9_ExportLicenseTicket(&actual_size, size, title_id, ticket_id, output);
			assertNotAm(res);

			ipc_command[0] = IPC_MakeHeader(0x0821, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = actual_size;
			ipc_command[3] = IPC_Desc_Buffer(size, IPC_BUFFER_W);
			ipc_command[4] = (u32)output;
		}
		break;
	case 0x0822: // get current content info count
		{
			CHECK_HEADER(0x0822, 0, 0)

			u32 count = 0;

			Result res = AM9_GetCurrentContentInfoCount(&count);
			assertNotAmOrFsWithMedia(res, session->media);

			ipc_command[0] = IPC_MakeHeader(0x0822, 2, 0);
			ipc_command[1] = res;
			ipc_command[2] = count;
		}
		break;
	case 0x0823: // find current content infos
		{
			CHECK_HEADER(0x0823, 1, 4)

			u32 count = ipc_command[1];
			u32 tobjcount = count * sizeof(u16);
			u32 cobjcount = count * sizeof(ContentInfo);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[2]) != tobjcount ||
				!IPC_VerifyBuffer(ipc_command[4], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[4]) != cobjcount
			)

			u16 *indices = (u16 *)ipc_command[3];
			ContentInfo *infos = (ContentInfo *)ipc_command[5];

			Result res = AM9_FindCurrentContentInfos(count, indices, infos);
			assertNotAmOrFsWithMedia(res, session->media);

			ipc_command[0] = IPC_MakeHeader(0x0823, 1, 4);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(tobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)indices;
			ipc_command[4] = IPC_Desc_Buffer(cobjcount, IPC_BUFFER_W);
			ipc_command[5] = (u32)infos;
		}
		break;
	case 0x0824: // list current content infos
		{
			CHECK_HEADER(0x0824, 2, 2)

			u32 amount = ipc_command[1];
			u32 cobjcount = amount * sizeof(ContentInfo);
			u32 offset = ipc_command[2];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[3], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[3]) != cobjcount
			)

			ContentInfo *infos = (ContentInfo *)ipc_command[4];

			u32 count = 0;

			Result res = AM9_ListCurrentContentInfos(&count, amount, offset, infos);
			assertNotAmOrFsWithMedia(res, session->media);

			ipc_command[0] = IPC_MakeHeader(0x0824, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = count;
			ipc_command[3] = IPC_Desc_Buffer(cobjcount, IPC_BUFFER_W);
			ipc_command[4] = (u32)infos;
		}
		break;
	case 0x0825: // calculate optional DLC contents required size
		{
			CHECK_HEADER(0x0825, 4, 2)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));
			u32 count = ipc_command[4];
			u32 iobjcount = count * sizeof(u16);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[5]) != iobjcount
			)

			u16 *indices = (u16 *)ipc_command[6];

			u64 size = 0;

			Result res = AMNet_CalculateContextRequiredSize(media_type, title_id, indices, count, &size);
			assertNotAmOrFsWithMedia(res, media_type); // stock am doesn't assert here :)

			ipc_command[0] = IPC_MakeHeader(0x0825, 3, 2);
			ipc_command[1] = res;
			ipc_command[2] = LODWORD(size);
			ipc_command[3] = HIDWORD(size);
			ipc_command[4] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_R);
			ipc_command[5] = (u32)indices;
		}
		break;
	case 0x0826: // update import content contexts
		{
			CHECK_HEADER(0x0826, 2, 2)

			u32 count = ipc_command[1];
			u32 iobjcount = count * sizeof(u16);

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[2], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[2]) != iobjcount
			)

			u16 *indices = (u16 *)ipc_command[3];

			Result res = AM9_UpdateImportContentContexts(count, indices);

			ipc_command[0] = IPC_MakeHeader(0x0826, 1, 2);
			ipc_command[1] = res;
			ipc_command[2] = IPC_Desc_Buffer(iobjcount, IPC_BUFFER_R);
			ipc_command[3] = (u32)indices;
		}
		break;
	case 0x0827: // clear demo database (reset header)
		{
			CHECK_HEADER(0x0827, 0, 0)

			AM_DemoDatabase_InitializeHeader(&GLOBAL_DemoDatabase);

			ipc_command[0] = IPC_MakeHeader(0x0827, 0, 0);
			ipc_command[1] = 0;
		}
		break;
	case 0x0828: // install title begin (overwrite)
		{
			CHECK_HEADER(0x0828, 3, 0)

			MediaType media_type = (MediaType)ipc_command[1];
			u64 title_id = *((u64 *)(&ipc_command[2]));

			Result res = 0;

			if (session->importing_title)
				res = AM_INVALID_CONTENT_IMPORT_STATE;
			else
			{
				res = AM9_InstallTitleBeginOverwrite(media_type, title_id);
				assertNotAmOrFsWithMedia(res, media_type);

				if (R_SUCCEEDED(res))
				{
					session->media = media_type;
					session->importing_title = true;
				}
			}

			ipc_command[0] = IPC_MakeHeader(0x0828, 1, 0);
			ipc_command[1] = res;
		}
		break;
	case 0x0829: // export ticket wrapped
		{
			CHECK_HEADER(0x0829, 6, 4)

			u32 crypted_ticket_size = ipc_command[1];
			u32 crypted_keyiv_size = ipc_command[2];
			u64 title_id = *((u64 *)(&ipc_command[3]));
			u64 ticket_id = *((u64 *)(&ipc_command[5]));

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[7], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[7]) != crypted_ticket_size ||
				!IPC_VerifyBuffer(ipc_command[9], IPC_BUFFER_W) ||
				IPC_GetBufferSize(ipc_command[9]) != crypted_keyiv_size
			)

			void *crypted_ticket = (void *)ipc_command[8];
			void *crypted_keyiv = (void *)ipc_command[10];

			Result res = 0;

			u32 actual_crypted_ticket_size = 0;
			u32 actual_crypted_keyiv_size = 0;

			if (TitleID_IsAnySystem(title_id) || TitleID_IsDLPChild(title_id))
				res = AM_INVALID_TITLE_ID_HIGH;
			else
			{
				res = AM9_ExportTicketWrapped(&actual_crypted_ticket_size, &actual_crypted_keyiv_size, crypted_ticket_size, crypted_keyiv_size, title_id, ticket_id, crypted_ticket, crypted_keyiv);
				assertNotAm(res);
			}

			ipc_command[0] = IPC_MakeHeader(0x0829, 3, 4);
			ipc_command[1] = res;
			ipc_command[2] = actual_crypted_ticket_size;
			ipc_command[3] = actual_crypted_keyiv_size;
			ipc_command[4] = IPC_Desc_Buffer(crypted_ticket_size, IPC_BUFFER_W);
			ipc_command[5] = (u32)crypted_ticket;
			ipc_command[6] = IPC_Desc_Buffer(crypted_keyiv_size, IPC_BUFFER_W);
			ipc_command[7] = (u32)crypted_keyiv;
		}
		break;
	}
}

#define CMD_ID_RANGE(id, lower, upper) \
	(id >= lower && id <= upper)

// 0x1-0x2D: net, sys, u
// 0x401-0x419: net, u
// 0x801-0x829: net
// 0x1001-0x100D: net, sys, u, app

// net: 0x1-0x2D, 0x410-0x419, 0x801-0x829, 0x1001-0x100D
// u  : 0x1-0x2D, 0x401-0x419, 0x1001-0x100D
// sys: 0x1-0x2D, 0x1001-0x100D
// app: 0x1001-0x100D

#ifdef DEBUG_PRINTS
	#define GET_OGHDR u32 header_og = ipc_command[0];
	#define PRINT_RET(name) \
		u32 header_ret = ipc_command[0]; \
		Result res = ipc_command[1]; \
		DEBUG_PRINTF3("[am:" name "] src (", header_og, ") -> (", header_ret, ") replying with result ", res);
#else
	#define GET_OGHDR
	#define PRINT_RET(name)
#endif

void AMNET_HandleIPC(AM_SessionData *session)
{
	u32 *ipc_command  = getThreadLocalStorage()->ipc_command;
	GET_OGHDR
	u16 cmd_id = (u16)(ipc_command[0] >> 16);

	if      (CMD_ID_RANGE(cmd_id, 0x1   , 0x2D  )) AM_HandleIPC_Range0x1_0x2D();
	else if (CMD_ID_RANGE(cmd_id, 0x401 , 0x419 )) AM_HandleIPC_Range0x401_0x419(session);
	else if (CMD_ID_RANGE(cmd_id, 0x801 , 0x829 )) AM_HandleIPC_Range0x801_0x829(session);
	else if (CMD_ID_RANGE(cmd_id, 0x1001, 0x100D)) AM_HandleIPC_Range0x1001_0x100D();
	else RET_OS_INVALID_IPCARG

	PRINT_RET("net")
}

void AMU_HandleIPC(AM_SessionData *session)
{
	u32 *ipc_command  = getThreadLocalStorage()->ipc_command;
	GET_OGHDR
	u16 cmd_id = (u16)(ipc_command[0] >> 16);

	if      (CMD_ID_RANGE(cmd_id, 0x1, 0x2D))      AM_HandleIPC_Range0x1_0x2D();
	else if (CMD_ID_RANGE(cmd_id, 0x401, 0x419))   AM_HandleIPC_Range0x401_0x419(session);
	else if (CMD_ID_RANGE(cmd_id, 0x1001, 0x100D)) AM_HandleIPC_Range0x1001_0x100D();
	else RET_OS_INVALID_IPCARG;

	PRINT_RET("u")
}

void AMSYS_HandleIPC(AM_SessionData *session)
{
	(void)session;

	u32 *ipc_command  = getThreadLocalStorage()->ipc_command;
	GET_OGHDR
	u16 cmd_id = (u16)(ipc_command[0] >> 16);

	if      (CMD_ID_RANGE(cmd_id, 0x1, 0x2D))      AM_HandleIPC_Range0x1_0x2D();
	else if (CMD_ID_RANGE(cmd_id, 0x1001, 0x100D)) AM_HandleIPC_Range0x1001_0x100D();
	else RET_OS_INVALID_IPCARG;

	PRINT_RET("sys")
}

void AMAPP_HandleIPC(AM_SessionData *session)
{
	(void)session;

	u32 *ipc_command  = getThreadLocalStorage()->ipc_command;
	GET_OGHDR
	u16 cmd_id = (u16)(ipc_command[0] >> 16);

	if (CMD_ID_RANGE(cmd_id, 0x1001, 0x100D)) AM_HandleIPC_Range0x1001_0x100D();
	else RET_OS_INVALID_IPCARG;

	PRINT_RET("app")
}

void (* AM_IPCHandlers[4])(AM_SessionData *) =
{
	&AMNET_HandleIPC,
	&AMSYS_HandleIPC,
	&AMU_HandleIPC,
	&AMAPP_HandleIPC
};
