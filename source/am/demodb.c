#include <am/demodb.h>

__attribute__((section(".data.demodb"), aligned(4))) Database GLOBAL_DemoDatabase;

static bool findIndex(Database *db, u64 title_id, u16 *index)
{
	title_id &= 0x0000FFFFFFFFFFFF; // remove console type

	if (!db->info.entry_count) // no entries
		return false;

	u32 remain = sizeof(u64) * db->info.entry_count;
	u32 offset = sizeof(DatabaseHeader);
	bool found = false;
	u16 tid_index = 0;
	u32 read;

	while (remain)
	{
		u32 batch = MIN(sizeof(db->tid_buf), remain);

		Err_FailedThrow(FSFile_Read(&read, db->db_file, offset, batch, &db->tid_buf))

		for (u32 i = 0; i < batch / sizeof(u64); i++, tid_index++)
		{
			if (db->tid_buf[i] == title_id)
			{
				*index = tid_index;
				found = true;
				goto ret;
			}
		}

		offset += batch;
		remain -= batch;
	}

ret:
	return found; // stock am returns a Result (AM_NOT_FOUND) if index isn't found but who cares
}

static u8 readPlayCount(Database *db, u16 index)
{
	DatabaseEntry entry;
	u32 read;

	Err_FailedThrow(FSFile_Read(&read, db->db_file, sizeof(DatabaseHeader) + (sizeof(u64) * SAVE_MAX_ENTRIES) + (sizeof(DatabaseEntry) * index), sizeof(DatabaseEntry), &entry))

	return entry.play_count;
}

static void writePlayCount(Database *db, u16 index, u8 count)
{
	DatabaseEntry entry;
	u32 written;

	entry.play_count = count;

	Err_FailedThrow(FSFile_Write(&written, db->db_file, sizeof(DatabaseHeader) + (sizeof(u64) * SAVE_MAX_ENTRIES) + (sizeof(DatabaseEntry) * index), sizeof(DatabaseEntry), FS_WRITE_FLUSH, &entry))
}

static void writeTitleId(Database *db, u16 index, u64 title_id)
{
	title_id &= 0x0000FFFFFFFFFFFF; // remove console type
	u32 written;

	Err_FailedThrow(FSFile_Write(&written,db->db_file, sizeof(DatabaseHeader) + (sizeof(u64) * index), sizeof(u64), FS_WRITE_FLUSH, &title_id))
}

void AM_DemoDatabase_Initialize(Database *db)
{
	_memset32_aligned(db, 0, sizeof(Database));

	RecursiveLock_Init(&db->lock);

	FS_SystemSaveDataInfo save_info = { MediaType_NAND, { 0, 0, 0 }, SAVE_ID };
	bool save_created_new = false;
	Result res;

	res = FSUser_OpenArchive(ARCHIVE_SYSTEM_SAVEDATA, PATH_BINARY, &save_info, sizeof(save_info), &db->save_archive);

	// perhaps archive doesn't exist yet
	if (R_FAILED(res))
	{
		// if it's not a normal not found error, delete archive
		if (!R_MODULEDESCRANGE(res, RM_FS, 100, 179))
			FSUser_DeleteSystemSaveData(&save_info);

		// can now recreate archive
		Err_FailedThrow(FSUser_CreateSystemSaveData(&save_info, SAVE_ARCHIVE_SIZE, 0x1000, 10, 10, getBucketCount(10), getBucketCount(10), true));

		// if still fail, something is seriously fucked
		Err_FailedThrow(FSUser_OpenArchive(ARCHIVE_SYSTEM_SAVEDATA, PATH_BINARY, &save_info, sizeof(save_info), &db->save_archive))
	}

	// let's see if the file exists now
	res = FSUser_OpenFile(0, db->save_archive, PATH_UTF16, sizeof(SAVE_FILE_PATH), FS_OPEN_READ | FS_OPEN_WRITE, 0, SAVE_FILE_PATH, &db->db_file);

	// could be simply the fact that it doesn't exist
	if (R_FAILED(res))
	{
		Err_FailedThrow(FSUser_CreateFile(0, db->save_archive, PATH_UTF16, sizeof(SAVE_FILE_PATH), 0, SAVE_FILE_SIZE, SAVE_FILE_PATH))
		Err_FailedThrow(FSUser_OpenFile(0, db->save_archive, PATH_UTF16, sizeof(SAVE_FILE_PATH), FS_OPEN_READ | FS_OPEN_WRITE, 0, SAVE_FILE_PATH, &db->db_file))

		save_created_new = true;
	}

	// need to fill file with zeros else hash errors on next read
	if (save_created_new)
	{
		u32 written;
		Err_FailedThrow(FSFile_Write(&written, db->db_file, 0, 8, FS_WRITE_FLUSH, db->tid_buf))

		for (u32 i = 0; i < (SAVE_FILE_SIZE - 8) / sizeof(db->tid_buf); i += sizeof(db->tid_buf))
			Err_FailedThrow(FSFile_Write(&written, db->db_file, i + sizeof(DatabaseHeader), sizeof(db->tid_buf), FS_WRITE_FLUSH, db->tid_buf))

		AM_DemoDatabase_CommitSaveData(db);
		return;
	}

	u32 read;

	// read header, since if code reaches here we have an existing file with the correct size
	Err_FailedThrow(FSFile_Read(&read, db->db_file, 0, sizeof(DatabaseHeader), &db->info))
}

void AM_DemoDatabase_WriteHeader(Database *db)
{
	u32 written;
	Err_FailedThrow(FSFile_Write(&written, db->db_file, 0, sizeof(DatabaseHeader), FS_WRITE_FLUSH, &db->info))
}

void AM_DemoDatabase_CommitSaveData(Database *db)
{
	u8 input, output;
	Err_FailedThrow(FSUser_ControlArchive(db->save_archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, &input, 1, &output, 1))
}

void AM_DemoDatabase_InitializeHeader(Database *db)
{
	_memset32_aligned(&db->info, 0x00, sizeof(DatabaseHeader)); // clear everything
	AM_DemoDatabase_WriteHeader(db);
	AM_DemoDatabase_CommitSaveData(db);
}

void AM_DemoDatabase_Close(Database *db)
{
	AM_DemoDatabase_CommitSaveData(db);
	FSFile_Close(db->db_file);
	FSUser_CloseArchive(db->save_archive);
}

void AM_DemoDatabase_GetLaunchInfos(Database *db, u64 *title_ids, u32 count, DemoLaunchInfo *infos)
{
	RecursiveLock_Lock(&db->lock);

	_memset32_aligned(infos, 0x00, sizeof(DemoLaunchInfo) * count);

	u32 remain = sizeof(u64) * db->info.entry_count;
	u32 offset = sizeof(DatabaseHeader);
	u32 read;

	u32 processed = 0;

	while (remain)
	{
		u32 batch = MIN(sizeof(db->tid_buf), remain);
		u32 batch_tids_count = batch / sizeof(u64);

		Err_FailedThrow(FSFile_Read(&read, db->db_file, offset, batch, &db->tid_buf))

		for (u32 i = 0; i < batch_tids_count; i++)
			for (u32 j = 0; j < count; j++)
				if (db->tid_buf[i] == (title_ids[j] & 0x0000FFFFFFFFFFFF))
				{
					infos[j].flags |= 8;
					infos[j].playcount = readPlayCount(db, processed + i);
				}

		processed += batch_tids_count;
		
		offset += batch;
		remain -= batch;
	}

	RecursiveLock_Unlock(&db->lock);
}

bool AM_DemoDatabase_HasDemoLaunchRight(Database *db, u64 title_id)
{
	RecursiveLock_Lock(&db->lock);

	TicketLimitInfo ticket_limit_info;
	DemoLaunchInfo demo_launch_info;

	if (R_FAILED(AM9_GetTicketLimitInfos(1, &title_id, &ticket_limit_info)))
	{
		RecursiveLock_Unlock(&db->lock);
		return false;
	}

	if (ticket_limit_info.flags)
	{
		AM_DemoDatabase_GetLaunchInfos(db, &title_id, 1, &demo_launch_info);

		if (demo_launch_info.flags & 0x8 && demo_launch_info.playcount >= ticket_limit_info.playcount)
		{
			RecursiveLock_Unlock(&db->lock);
			return false;
		}
	}

	// at this point we have the launch right

	u16 index;

	// title id not in db
	if (!findIndex(db, title_id, &index))
	{
		writeTitleId(db, db->info.next_entry_index, title_id);
		writePlayCount(db, db->info.next_entry_index, 1);

		if (db->info.entry_count < 4096)
			db->info.entry_count++;

		// if we are at the last index (0xFFF or 4095), we overlap back to the 0th index)
		db->info.next_entry_index = db->info.next_entry_index == 4095 ? 0 : db->info.next_entry_index + 1;

		AM_DemoDatabase_WriteHeader(db);
	}
	else
	{
		// tid in db, go do stuff
		u8 playcount = readPlayCount(db, index);
		playcount++;
		writePlayCount(db, index, playcount);
	}

	AM_DemoDatabase_CommitSaveData(db);

	RecursiveLock_Unlock(&db->lock);
	return true;
}