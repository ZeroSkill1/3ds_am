#ifndef _AM_3DS_FS_H
#define _AM_3DS_FS_H

#include <3ds/result.h>
#include <3ds/types.h>
#include <3ds/err.h>
#include <3ds/svc.h>
#include <3ds/srv.h>

typedef enum MediaType
{
	MediaType_NAND      = 0,
	MediaType_SD        = 1,
	MediaType_Gamecard  = 2
} MediaType;

typedef struct __attribute__((packed)) FS_SystemSaveDataInfo
{
    u8 media_type;
    u8 unk_reserved[3];
    u32 save_id;
} FS_SystemSaveDataInfo;

typedef enum FS_PathType
{
	PATH_INVALID = 0,
	PATH_EMPTY   = 1,
	PATH_BINARY  = 2,
	PATH_ASCII   = 3,
	PATH_UTF16   = 4,
} FS_PathType;

typedef enum FS_ArchiveID
{
	ARCHIVE_ROMFS                    = 0x00000003,
	ARCHIVE_SAVEDATA                 = 0x00000004,
	ARCHIVE_EXTDATA                  = 0x00000006,
	ARCHIVE_SHARED_EXTDATA           = 0x00000007,
	ARCHIVE_SYSTEM_SAVEDATA          = 0x00000008,
	ARCHIVE_SDMC                     = 0x00000009,
	ARCHIVE_SDMC_WRITE_ONLY          = 0x0000000A,
	ARCHIVE_BOSS_EXTDATA             = 0x12345678,
	ARCHIVE_CARD_SPIFS               = 0x12345679,
	ARCHIVE_EXTDATA_AND_BOSS_EXTDATA = 0x1234567B,
	ARCHIVE_SYSTEM_SAVEDATA2         = 0x1234567C,
	ARCHIVE_NAND_RW                  = 0x1234567D,
	ARCHIVE_NAND_RO                  = 0x1234567E,
	ARCHIVE_NAND_RO_WRITE_ACCESS     = 0x1234567F,
	ARCHIVE_SAVEDATA_AND_CONTENT     = 0x2345678A,
	ARCHIVE_SAVEDATA_AND_CONTENT2    = 0x2345678E,
	ARCHIVE_NAND_CTR_FS              = 0x567890AB,
	ARCHIVE_TWL_PHOTO                = 0x567890AC,
	ARCHIVE_TWL_SOUND                = 0x567890AD,
	ARCHIVE_NAND_TWL_FS              = 0x567890AE,
	ARCHIVE_NAND_W_FS                = 0x567890AF,
	ARCHIVE_GAMECARD_SAVEDATA        = 0x567890B1,
	ARCHIVE_USER_SAVEDATA            = 0x567890B2,
	ARCHIVE_DEMO_SAVEDATA            = 0x567890B4,
} FS_ArchiveID;

typedef enum FS_ArchiveAction
{
	ARCHIVE_ACTION_COMMIT_SAVE_DATA = 0,
	ARCHIVE_ACTION_GET_TIMESTAMP    = 1,
	ARCHIVE_ACTION_UNKNOWN          = 0x789D,
} FS_ArchiveAction;


enum
{
	FS_OPEN_READ   = BIT(0),
	FS_OPEN_WRITE  = BIT(1),
	FS_OPEN_CREATE = BIT(2),
};

/// Write flags.
enum
{
	FS_WRITE_FLUSH       = BIT(0),
	FS_WRITE_UPDATE_TIME = BIT(8),
};

/// Attribute flags.
enum
{
	FS_ATTRIBUTE_DIRECTORY = BIT(0), 
	FS_ATTRIBUTE_HIDDEN    = BIT(8), 
	FS_ATTRIBUTE_ARCHIVE   = BIT(16),
	FS_ATTRIBUTE_READ_ONLY = BIT(24),
};

enum
{
	ID_FSFile_OpenSubFile = 0x0801,
	ID_FSFile_Read        = 0x0802,
	ID_FSFile_Write       = 0x0803,
	ID_FSFile_GetSize     = 0x0804,
	ID_FSFile_Close       = 0x0808,
};

enum
{
	ID_FSUser_OpenFile                 = 0x0802,
	ID_FSUser_DeleteFile               = 0x0804,
	ID_FSUser_CreateFile               = 0x0808,
	ID_FSUser_OpenArchive              = 0x080C,
	ID_FSUser_ControlArchive           = 0x080D,
	ID_FSUser_CloseArchive             = 0x080E,
	ID_FSUser_CreateSystemSaveData     = 0x0856,
	ID_FSUser_DeleteSystemSaveData     = 0x0857,
	ID_FSUser_InitializeWithSDKVersion = 0x0861,
};

typedef u64 FS_Archive;

#ifdef REPLACE_AM
#define SAVE_ID 0x00010015
#else
#define SAVE_ID 0x00010054
#endif

extern Handle fsuser_session;
extern bool fsuser_init;

Result fsUserInit();
void fsUserExit();

u32 getBucketCount(u32 count);

Result FSUser_OpenFile(u32 transaction, FS_Archive archive, FS_PathType path_type, u32 path_size, u32 open_flags, u32 attributes, void *path_data, Handle *file);
Result FSUser_DeleteFile(u32 transaction, FS_Archive archive, FS_Archive path_type, u32 path_size, void *path_data);
Result FSUser_CreateFile(u32 transaction, FS_Archive archive, FS_PathType path_type, u32 path_size, u32 attributes, u64 zero_bytes_count, void *path_data);
Result FSUser_OpenArchive(FS_ArchiveID archive_id, FS_PathType path_type, void *path_data, u32 path_size, FS_Archive *archive);
Result FSUser_ControlArchive(FS_Archive archive, FS_ArchiveAction action, void *input, u32 input_size, void *output, u32 output_size);
Result FSUser_CloseArchive(FS_Archive archive);
Result FSUser_CreateSystemSaveData(FS_SystemSaveDataInfo *save_info, u32 total_size, u32 block_size, u32 dircount, u32 filecount, u32 dirbucketcount, u32 filebucketcount, bool duplicate_data);
Result FSUser_DeleteSystemSaveData(FS_SystemSaveDataInfo *save_info);
Result FSUser_InitializeWithSDKVersion(u32 version);

Result FSFile_OpenSubFile(Handle file, Handle *sub_file, u64 offset, u64 size);
Result FSFile_Read(u32 *read, Handle file, u64 offset, u32 data_size, void *data);
Result FSFile_Write(u32 *written, Handle file, u64 offset, u32 size, u32 write_option, void *data);
Result FSFile_GetSize(u64 *size, Handle file);
Result FSFile_Close(Handle file);

#endif