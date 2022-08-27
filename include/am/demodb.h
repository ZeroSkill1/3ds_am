#ifndef _AM_DEMODB_H
#define _AM_DEMODB_H

#include <3ds/synchronization.h>
#include <3ds/types.h>
#include <3ds/am9.h>
#include <3ds/fs.h>

#define SAVE_ARCHIVE_SIZE 0x40000
#define SAVE_FILE_SIZE    0x10008
#define SAVE_FILE_PATH    L"/database.bin"
#define SAVE_MAX_ENTRIES  0x1000 // 4096

/*
	stock am refers to the AM save archive as "amsa:", but since we don't use SDK archives,
	this archive naming is omitted entirely. the file is called database.bin, the format is as follows:

	everything is little endian

	----------------------------------------------------------|
	| offset  | size   | description                          |
	|---------------------------------------------------------|
	|  0x0    | 0x8    | header                               |
	|---------------------------------------------------------|
	|  0x8    | 0x8000 | title ids, without console type 0004 |
	|---------------------------------------------------------|
	|  0x8008 | 0x8000 | demo play count data                 |
	|---------------------------------------------------------|
	
	"without console type 0004", for example: 0004000000123400 -> 0000000000123400

	an indexing system is used. first, the title id is resolved using the title id 
	data in the file. the file is read in chunks. once the title id has been found,
	it uses the index to get/set the demo play count in the demo play data.

	each 8-byte entry in the play count data corresponds to a title id in the title
	id data. only the first byte is used, which is the play count (u8).
*/

typedef struct __attribute__((aligned(4))) DatabaseHeader
{
	u8 pad[4];
	u16 next_entry_index;
	u16 entry_count;
} DatabaseHeader;

typedef struct __attribute__((aligned(4))) DatabaseEntry
{
	u8 play_count;
	u8 unused[7];
} DatabaseEntry;

typedef struct __attribute__((aligned(4))) Database
{
	FS_Archive save_archive;
	Handle db_file;
	u64 tid_buf[128];
	DatabaseHeader info;
	RecursiveLock lock;
} Database;

typedef struct __attribute__((aligned(4))) DemoDatabaseFile
{
	DatabaseHeader header;
	u64 title_ids[SAVE_MAX_ENTRIES];
	DatabaseEntry entries[SAVE_MAX_ENTRIES];
} DatabaseFile;

typedef TicketLimitInfo DemoLaunchInfo;

void AM_DemoDatabase_Initialize(Database *db);
void AM_DemoDatabase_WriteHeader(Database *db);
void AM_DemoDatabase_CommitSaveData(Database *db);
void AM_DemoDatabase_InitializeHeader(Database *db);
void AM_DemoDatabase_Close(Database *db);

void AM_DemoDatabase_GetLaunchInfos(Database *db, u64 *title_ids, u32 count, DemoLaunchInfo *infos);
bool AM_DemoDatabase_HasDemoLaunchRight(Database *db, u64 title_id);

#endif