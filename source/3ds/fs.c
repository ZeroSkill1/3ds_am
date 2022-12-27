#include <3ds/ipc.h>
#include <3ds/fs.h>

Handle fsuser_session;
bool fsuser_init;

static const char fs_srvname[8] = "fs:USER";

Result fsUserInit()
{
	if (!fsuser_init)
	{
		Result res = SRV_GetServiceHandle(&fsuser_session, fs_srvname, 7, 0);

		if (R_SUCCEEDED(res))
			fsuser_init = true;

		Err_FailedThrow(FSUser_InitializeWithSDKVersion(0xB0502C8))

		return res;
	}

	return 0;
}

void fsUserExit()
{
	if (fsuser_init)
	{
		Err_FailedThrow(svcCloseHandle(fsuser_session))
		fsuser_session = 0;
		fsuser_init = false;
	}
}


u32 getBucketCount(u32 count)
{
	if (count < 20)
		return (count < 4) ? 3 : count | 1;
	
	for (int i = 0; i < 100; ++i)
	{
		u32 ret = count + i;

		if (ret & 1 && ret % 3 && ret % 5 && ret % 7 && ret % 11 && ret % 13 && ret % 17)
			return ret;
	}

	return count | 1;
}

// fs

Result FSUser_CreateSystemSaveData(FS_SystemSaveDataInfo *save_info, u32 total_size, u32 block_size,
							   u32 dircount, u32 file_count, u32 dir_bucket_count,
							   u32 file_bucket_count, bool duplicate_data)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSUser_CreateSystemSaveData, 9, 0);
	_memcpy32_aligned(&ipc_command[1], save_info, sizeof(FS_SystemSaveDataInfo));
	ipc_command[3] = total_size;
	ipc_command[4] = block_size;
	ipc_command[5] = dircount;
	ipc_command[6] = file_count;
	ipc_command[7] = dir_bucket_count;
	ipc_command[8] = file_bucket_count;
	ipc_command[9] = (u32)duplicate_data;

	BASIC_RET(fsuser_session)
}

Result FSUser_DeleteSystemSaveData(FS_SystemSaveDataInfo *save_info)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSUser_DeleteSystemSaveData, 2, 0);

	ipc_command[1] = LODWORD(*((u64 *)save_info));
	ipc_command[2] = HIDWORD(*((u64 *)save_info));

	BASIC_RET(fsuser_session)
}

// fsuser

Result FSUser_OpenFile(u32 transaction, FS_Archive archive, FS_PathType path_type, u32 path_size,
					   u32 open_flags, u32 attributes, void *path_data, Handle *file)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSUser_OpenFile, 7, 2);
	ipc_command[1] = transaction;
	ipc_command[2] = LODWORD(archive);
	ipc_command[3] = HIDWORD(archive);
	ipc_command[4] = path_type;
	ipc_command[5] = path_size;
	ipc_command[6] = open_flags;
	ipc_command[7] = attributes;
	ipc_command[8] = IPC_Desc_StaticBuffer(path_size, 0);
	ipc_command[9] = (u32)path_data;

	CHECK_RET(fsuser_session)

	// ipc_command[2] = 0x10, move handles
	if (file) *file = ipc_command[3];

	return res;
}

Result FSUser_DeleteFile(u32 transaction, FS_Archive archive, FS_Archive path_type, u32 path_size, void *path_data)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSUser_DeleteFile, 5, 2);
	ipc_command[1] = transaction;
	ipc_command[2] = LODWORD(archive);
	ipc_command[3] = HIDWORD(archive);
	ipc_command[4] = path_type;
	ipc_command[5] = path_size;
	ipc_command[6] = IPC_Desc_Buffer(path_size, IPC_BUFFER_R);
	ipc_command[7] = (u32)path_data;

	BASIC_RET(fsuser_session)
}

Result FSUser_CreateFile(u32 transaction, FS_Archive archive, FS_PathType path_type, u32 path_size,
						 u32 attributes, u64 zero_bytes_count, void *path_data)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0]  = IPC_MakeHeader(ID_FSUser_CreateFile, 8, 2);
	ipc_command[1]  = transaction;
	ipc_command[2]  = LODWORD(archive);
	ipc_command[3]  = HIDWORD(archive);
	ipc_command[4]  = path_type;
	ipc_command[5]  = path_size;
	ipc_command[6]  = attributes;
	ipc_command[7]  = LODWORD(zero_bytes_count);
	ipc_command[8]  = HIDWORD(zero_bytes_count);
	ipc_command[9]  = IPC_Desc_StaticBuffer(path_size, 0);
	ipc_command[10] = (u32)path_data;

	BASIC_RET(fsuser_session)
}

Result FSUser_OpenArchive(FS_ArchiveID archive_id, FS_PathType path_type, void *path_data,
						  u32 path_size, FS_Archive *archive)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSUser_OpenArchive, 3, 2);
	ipc_command[1] = archive_id;
	ipc_command[2] = path_type;
	ipc_command[3] = path_size;
	ipc_command[4] = IPC_Desc_StaticBuffer(path_size, 0);
	ipc_command[5] = (u32)path_data;

	CHECK_RET(fsuser_session)

	if (archive) *archive = *((u64 *)&ipc_command[2]);

	return res;
}

Result FSUser_ControlArchive(FS_Archive archive, FS_ArchiveAction action, void *input, u32 input_size,
							 void *output, u32 output_size)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSUser_ControlArchive, 5, 4);
	ipc_command[1] = LODWORD(archive);
	ipc_command[2] = HIDWORD(archive);
	ipc_command[3] = (u32)action;
	ipc_command[4] = input_size;
	ipc_command[5] = output_size;
	ipc_command[6] = IPC_Desc_Buffer(input_size, IPC_BUFFER_R);
	ipc_command[7] = (u32)input;
	ipc_command[8] = IPC_Desc_Buffer(output_size, IPC_BUFFER_W);
	ipc_command[9] = (u32)output;

	BASIC_RET(fsuser_session)
}

Result FSUser_CloseArchive(FS_Archive archive)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSUser_CloseArchive, 2, 0);
	ipc_command[1] = LODWORD(archive);
	ipc_command[2] = HIDWORD(archive);

	BASIC_RET(fsuser_session)
}

Result FSUser_InitializeWithSDKVersion(u32 version)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSUser_InitializeWithSDKVersion, 1, 2);
	ipc_command[1] = version;
	ipc_command[2] = 0x20; // process ID header

	BASIC_RET(fsuser_session)
}

// fsfile

Result FSFile_OpenSubFile(Handle file, Handle *sub_file, u64 offset, u64 size)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSFile_OpenSubFile, 4, 0);
	ipc_command[1] = LODWORD(offset);
	ipc_command[2] = HIDWORD(offset);
	ipc_command[3] = LODWORD(size);
	ipc_command[4] = HIDWORD(size);

	CHECK_RET(file)

	if (sub_file) *sub_file = ipc_command[3];

	return res;
}

Result FSFile_Read(u32 *read, Handle file, u64 offset, u32 data_size, void *data)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSFile_Read, 3, 2);
	ipc_command[1] = LODWORD(offset);
	ipc_command[2] = HIDWORD(offset);
	ipc_command[3] = data_size;
	ipc_command[4] = IPC_Desc_Buffer(data_size, IPC_BUFFER_W);
	ipc_command[5] = (u32)data;

	CHECK_RET(file)

	if (read) *read = ipc_command[2];

	return res;
}


Result FSFile_Write(u32 *written, Handle file, u64 offset, u32 size, u32 write_option, void *data)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSFile_Write, 4, 2);
	ipc_command[1] = LODWORD(offset);
	ipc_command[2] = HIDWORD(offset);
	ipc_command[3] = size;
	ipc_command[4] = write_option;
	ipc_command[5] = IPC_Desc_Buffer(size, IPC_BUFFER_R);
	ipc_command[6] = (u32)data;

	CHECK_RET(file)

	if (written) *written = ipc_command[2];

	return res;
}

Result FSFile_GetSize(u64 *size, Handle file)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSFile_GetSize, 0, 0);

	CHECK_RET(file)

	if (size) *size = *((u64 *)&ipc_command[2]);

	return res;
}

Result FSFile_Close(Handle file)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_FSFile_Close, 0, 0);

	CHECK_RET(file)

	svcCloseHandle(file);
	
	return res;
}