#ifndef _AM_3DS_AM9_H
#define _AM_3DS_AM9_H

#include <3ds/types.h>
#include <3ds/ipc.h>
#include <3ds/err.h>
#include <3ds/svc.h>
#include <3ds/fs.h>
#include <memops.h>

extern Handle am9_session;
extern bool am9_init;
extern bool importing_content;

Result am9Init();
void am9Exit();

enum
{
	IMPORT_STATE_NONE            = 0,
	IMPORT_STATE_NOT_STARTED_YET = 1,
	IMPORT_STATE_PAUSED          = 2,
	IMPORT_STATE_WAIT_FOR_COMMIT = 3,
	IMPORT_STATE_ALREADY_EXISTS  = 4,
	IMPORT_STATE_DELETING        = 5,
	IMPORT_STATE_NEEDS_CLEANUP   = 6
};

enum
{
	TitleDB = 0,
	TempDB  = 1
};

enum
{
	ContentTypeFlag_Encrypted = 0x0001,
	ContentTypeFlag_Disk      = 0x0002,
	ContentTypeFlag_CFM       = 0x0004,
	ContentTypeFlag_Optional  = 0x4000,
	ContentTypeFlag_Shared    = 0x8000,
};

enum
{
	ID_AM9_GetTitleCount                        = 0x0001,
	ID_AM9_GetTitleList                         = 0x0002,
	ID_AM9_GetTitleInfos                        = 0x0003,
	ID_AM9_DeleteTitle                          = 0x0004,
	ID_AM9_GetTitleProductCode                  = 0x0005,
	ID_AM9_GetTitleExtDataID                    = 0x0006,
	ID_AM9_DeletePendingTitles                  = 0x0007,
	ID_AM9_InstallFIRM                          = 0x0008,
	ID_AM9_InstallTicketBegin                   = 0x0009,
	ID_AM9_InstallTicketWrite                   = 0x000A,
	ID_AM9_InstallTicketCancel                  = 0x000B,
	ID_AM9_InstallTicketFinish                  = 0x000C,
	ID_AM9_DeleteTicket                         = 0x000D,
	ID_AM9_GetTicketCount                       = 0x000E,
	ID_AM9_GetTicketList                        = 0x000F,
	ID_AM9_InstallTitleBegin                    = 0x0010,
	ID_AM9_InstallTitlePause                    = 0x0011,
	ID_AM9_InstallTitleResume                   = 0x0012,
	ID_AM9_InstallTMDBegin                      = 0x0013,
	ID_AM9_InstallTMDWrite                      = 0x0014,
	ID_AM9_InstallTMDCancel                     = 0x0015,
	ID_AM9_InstallTMDFinish                     = 0x0016,
	ID_AM9_InstallContentBegin                  = 0x0017,
	ID_AM9_InstallContentWrite                  = 0x0018,
	ID_AM9_InstallContentPause                  = 0x0019,
	ID_AM9_InstallContentCancel                 = 0x001A,
	ID_AM9_InstallContentResume                 = 0x001B,
	ID_AM9_InstallContentFinish                 = 0x001C,
	ID_AM9_GetPendingTitleCount                 = 0x001D,
	ID_AM9_GetPendingTitleList                  = 0x001E,
	ID_AM9_GetPendingTitleInfo                  = 0x001F,
	ID_AM9_DeletePendingTitle                   = 0x0020,
	ID_AM9_GetImportContentContextsCount        = 0x0021,
	ID_AM9_GetImportContentContextsList         = 0x0022,
	ID_AM9_GetImportContentContexts             = 0x0023,
	ID_AM9_DeleteImportContentContexts          = 0x0024,
	ID_AM9_GetCurrentImportContentContextsCount = 0x0025,
	ID_AM9_GetCurrentImportContentContextsList  = 0x0026,
	ID_AM9_GetCurrentImportContentContexts      = 0x0027,
	ID_AM9_InstallTitleCancel                   = 0x0028,
	ID_AM9_InstallTitleFinish                   = 0x0029,
	ID_AM9_InstallTitlesCommit                  = 0x002A,
	ID_AM9_Stubbed_0x2B                         = 0x002B,
	ID_AM9_Stubbed_0x2C                         = 0x002C,
	ID_AM9_Stubbed_0x2D                         = 0x002D,
	ID_AM9_Stubbed_0x2E                         = 0x002E,
	ID_AM9_Stubbed_0x2F                         = 0x002F,
	ID_AM9_Stubbed_0x30                         = 0x0030,
	ID_AM9_Stubbed_0x31                         = 0x0031,
	ID_AM9_Stubbed_0x32                         = 0x0032,
	ID_AM9_Stubbed_0x33                         = 0x0033,
	ID_AM9_Stubbed_0x34                         = 0x0034,
	ID_AM9_Stubbed_0x35                         = 0x0035,
	ID_AM9_Stubbed_0x36                         = 0x0036,
	ID_AM9_Stubbed_0x37                         = 0x0037,
	ID_AM9_Stubbed_0x38                         = 0x0038,
	ID_AM9_Sign                                 = 0x0039,
	ID_AM9_Stubbed_0x3A                         = 0x003A,
	ID_AM9_GetDeviceCertificate                 = 0x003B,
	ID_AM9_GetDeviceID                          = 0x003C,
	ID_AM9_ImportCertificates                   = 0x003D,
	ID_AM9_ImportCertificate                    = 0x003E,
	ID_AM9_ImportDatabaseInitialized            = 0x003F,
	ID_AM9_Cleanup                              = 0x0040,
	ID_AM9_DeleteTemporaryTitles                = 0x0041,
	ID_AM9_InstallTitlesFinishAndInstallFIRM    = 0x0042,
	ID_AM9_DSiWareExportVerifyFooter            = 0x0043,
	ID_AM9_Stubbed_0x44                         = 0x0044,
	ID_AM9_DSiWareExportDecryptData             = 0x0045,
	ID_AM9_DSiWareWriteSaveData                 = 0x0046,
	ID_AM9_InitializeTitleDatabase              = 0x0047,
	ID_AM9_ReloadTitleDatabase                  = 0x0048,
	ID_AM9_GetTicketIDCount                     = 0x0049,
	ID_AM9_GetTicketIDList                      = 0x004A,
	ID_AM9_DeleteTicketID                       = 0x004B,
	ID_AM9_GetPersonalizedTicketInfos           = 0x004C,
	ID_AM9_DSiWareExportCreate                  = 0x004D,
	ID_AM9_DSiWareExportInstallTitleBegin       = 0x004E,
	ID_AM9_DSiWareExportGetSize                 = 0x004F,
	ID_AM9_GetTWLTitleListForReboot             = 0x0050,
	ID_AM9_DeleteUserDSiWareTitles              = 0x0051,
	ID_AM9_DeleteExpiredUserTitles              = 0x0052,
	ID_AM9_DSiWareExportVerifyMovableSedHash    = 0x0053,
	ID_AM9_GetTWLArchiveResourceInfo            = 0x0054,
	ID_AM9_DSiWareExportValidateSectionMAC      = 0x0055,
	ID_AM9_CheckContentRight                    = 0x0056,
	ID_AM9_CreateImportContentContexts          = 0x0057,
	ID_AM9_GetContentInfoCount                  = 0x0058,
	ID_AM9_FindContentInfos                     = 0x0059,
	ID_AM9_ListContentInfos                     = 0x005A,
	ID_AM9_GetCurrentContentInfoCount           = 0x005B,
	ID_AM9_FindCurrentContentInfos              = 0x005C,
	ID_AM9_ListCurrentContentInfos              = 0x005D,
	ID_AM9_DeleteContents                       = 0x005E,
	ID_AM9_GetTitleInstalledTicketsCount        = 0x005F,
	ID_AM9_ListTicketInfos                      = 0x0060,
	ID_AM9_ExportLicenseTicket                  = 0x0061,
	ID_AM9_GetTicketLimitInfos                  = 0x0062,
	ID_AM9_UpdateImportContentContexts          = 0x0063,
	ID_AM9_GetInternalTitleLocationInfo         = 0x0064,
	ID_AM9_MigrateAGBToSAV                      = 0x0065,
	ID_AM9_Stubbed_0x66                         = 0x0066,
	ID_AM9_DeleteTitles                         = 0x0067,
	ID_AM9_GetItemRights                        = 0x0068,
	ID_AM9_TitleInUse                           = 0x0069,
	ID_AM9_GetInstalledContentInfoCount         = 0x006A,
	ID_AM9_ListInstalledContentInfos            = 0x006B,
	ID_AM9_InstallTitleBeginOverwrite           = 0x006C,
	ID_AM9_ExportTicketWrapped                  = 0x006D,
};

typedef struct __attribute__((aligned(4))) ContentInfo
{
	u16 index;
	u16 type;
	u32 id;
	u64 size;
	u8 flags; ///< BIT(0): downloaded, BIT(1): owned
	u8 pad[0x7];
} ContentInfo;

typedef struct __attribute__((aligned(8))) TWLArchiveResourceInfo
{
	u64 total_capacity;
	u64 total_free_space;
	u64 titles_capacity;
	u64 titles_free_space;
} TWLArchiveResourceInfo;

typedef struct __attribute__((aligned(4))) TitleInfo
{
	u64 title_id;
	u64 size;
	u16 version;
	u8 pad[2];
	u32 type;
} TitleInfo;

typedef struct __attribute__((aligned(4))) TicketInfo
{
	u64 title_id;
	u64 ticket_id;
	u16 version;
	u16 padding;
	u32 size;
} TicketInfo;

typedef struct __attribute__((aligned(4))) ImportTitleContext
{
	u64 title_id;
	u16 version;
	u16 state;
	u32 type;
	u64 size;
} ImportTitleContext;

typedef struct __attribute__((aligned(4))) ImportContentContext
{
	u32 content_id;
	u16 content_index;
	u16 state;
	u64 size;
	u64 current_install_offset;
} ImportContentContext;

typedef struct __attribute__((aligned(4))) InternalTitleLocationInfo
{
	u8 data[32];
} InternalTitleLocationInfo;

typedef struct __attribute__((aligned(4))) TicketLimitInfo
{
	u8 flags;
	u8 playcount;
	u8 unk[14];
} TicketLimitInfo;

Result AM9_GetTitleCount(u32 *count, MediaType media_type);
Result AM9_GetTitleList(u32 *count, u32 amount, MediaType media_type, u64 *title_ids);
Result AM9_GetTitleInfos(MediaType media_type, u32 count, u64 *title_ids, TitleInfo *infos);
Result AM9_DeleteTitle(MediaType media_type, u64 title_id);
Result AM9_GetTitleProductCode(char *product_code, MediaType media_type, u64 title_id);
Result AM9_GetTitleExtDataID(u64 *extdata_id, MediaType media_type, u64 title_id);
Result AM9_DeletePendingTitles(MediaType media_type, u8 flags);
Result AM9_InstallFIRM(u64 title_id);
Result AM9_InstallTicketBegin();
Result AM9_InstallTicketWrite(void *tik_data, u32 tik_data_size);
Result AM9_InstallTicketCancel();
Result AM9_InstallTicketFinish();
Result AM9_DeleteTicket(u64 title_id);
Result AM9_GetTicketCount(u32 *count);
Result AM9_GetTicketList(u32 *count, u32 amount, u32 offset, u64 *title_ids);
Result AM9_InstallTitleBegin(MediaType media_type, u64 title_id, u8 db_type);
Result AM9_InstallTitlePause();
Result AM9_InstallTitleResume(MediaType media_type, u64 title_id);
Result AM9_InstallTMDBegin();
Result AM9_InstallTMDWrite(void *tmd_data, u32 tmd_data_size);
Result AM9_InstallTMDCancel();
Result AM9_InstallTMDFinish(bool unknown);
Result AM9_InstallContentBegin(u16 content_index);
Result AM9_InstallContentWrite(void *content_data, u32 content_data_size);
Result AM9_InstallContentPause();
Result AM9_InstallContentCancel();
Result AM9_InstallContentResume(u16 content_index, u64 *resume_offset);
Result AM9_InstallContentFinish();
Result AM9_GetPendingTitleCount(u32 *count, MediaType media_type, u8 status_mask);
Result AM9_GetPendingTitleList(u32 *count, u32 amount, MediaType media_type, u8 status_mask, u64 *title_ids);
Result AM9_GetPendingTitleInfo(u32 count, MediaType media_type, u64 *title_ids, TitleInfo *title_infos);
Result AM9_DeletePendingTitle(MediaType media_type, u64 title_id);
Result AM9_GetImportContentContextsCount(u32 *count, MediaType media_type, u64 title_id);
Result AM9_GetImportContentContextsList(u32 *count, u32 amount, MediaType media_type, u64 title_id, u16 *indices);
Result AM9_GetImportContentContexts(u32 count, MediaType media_type, u64 title_id, u16 *indices, ImportContentContext *contexts);
Result AM9_DeleteImportContentContexts(u32 count, MediaType media_type, u64 title_id, u16 *indices);
Result AM9_GetCurrentImportContentContextsCount(u32 *count);
Result AM9_GetCurrentImportContentContextsList(u32 *count, u32 amount, u16 *indices);
Result AM9_GetCurrentImportContentContexts(u32 count, u16 *indices, ImportContentContext *contexts);
Result AM9_InstallTitleCancel();
Result AM9_InstallTitleFinish();
Result AM9_InstallTitlesCommit(MediaType media_type, u32 count, u8 db_type, u64 *title_ids);
Result AM9_Sign(u8 *retval, u32 sig_outsize, u32 cert_outsize, u64 title_id, u32 data_size, void *data, void *sig, void *cert);
Result AM9_GetDeviceCertificate(u32 *retval, u32 out_data_size, void *out_data);
Result AM9_GetDeviceID(u64 *id);
Result AM9_ImportCertificates(u32 certsize_1, u32 certsize_2, u32 certsize_3, u32 certsize_4, void *cert_1, void *cert_2, void *cert_3, void *cert_4);
Result AM9_ImportCertificate(u32 certsize, void *cert);
Result AM9_ImportDatabaseInitialized(u8 *initialized, MediaType media_type);
Result AM9_Cleanup(MediaType media_type);
Result AM9_DeleteTemporaryTitles();
Result AM9_InstallTitlesFinishAndInstallFIRM(MediaType media_type, u32 count, u64 firm_title_id, u8 db_type, u64 *title_ids);
Result AM9_DSiWareExportVerifyFooter(u64 twl_title_id, u32 data_size, u32 ecdsa_sigsize, u32 device_cert_size, u32 apcert_size, u8 dsiware_export_section_index, void *data, void *ecdsa_sig, void *device_cert, void *apcert);
Result AM9_DSiWareExportDecryptData(u32 input_size, u32 output_size, u32 iv_size, u8 dsiware_export_section_index, void *input, void *input_iv, void *output, void *output_iv);
Result AM9_DSiWareWriteSaveData(u64 twl_title_id, u32 data_size, u32 nand_file_offset, u8 section_type, u8 operation, void *data);
Result AM9_InitializeTitleDatabase(MediaType media_type, bool overwrite);
Result AM9_ReloadTitleDatabase(u8 *available, MediaType media_type);
Result AM9_GetTicketIDCount(u32 *count, u64 title_id);
Result AM9_GetTicketIDList(u32 *count, u32 amount, u64 title_id, bool unknown, u64 *ticket_ids);
Result AM9_DeleteTicketID(u64 title_id, u64 ticket_id);
Result AM9_GetPersonalizedTicketInfos(u32 *count, u32 amount, TicketInfo *infos);
Result AM9_DSiWareExportCreate(u64 twl_title_id, u32 path_size, u32 buffer_size, u8 export_type, void *path, void *buffer);
Result AM9_DSiWareExportInstallTitleBegin(u64 title_id, u8 export_type);
Result AM9_DSiWareExportGetSize(u32 *size, u64 twl_title_id, u8 export_type);
Result AM9_GetTWLTitleListForReboot(u32 *count, u32 amount, u64 *title_ids, u32 *content_ids);
Result AM9_DeleteUserDSiWareTitles();
Result AM9_DeleteExpiredUserTitles(MediaType media_type);
Result AM9_DSiWareExportVerifyMovableSedHash(u32 buf0_size, u32 buf1_size, void *buf0, void *buf1);
Result AM9_GetTWLArchiveResourceInfo(TWLArchiveResourceInfo *info);
Result AM9_DSiWareExportValidateSectionMAC(u32 mac_size, u32 hash_size, u8 dsiware_export_section_index, void *mac, void *hash);
Result AM9_CheckContentRight(u8 *has_right, u64 title_id, u16 content_index);
Result AM9_CreateImportContentContexts(u32 count, u16 *indices);
Result AM9_GetContentInfoCount(u32 *count, MediaType media_type, u64 title_id);
Result AM9_FindContentInfos(MediaType media_type, u64 title_id, u32 count, u16 *indices, ContentInfo *infos);
Result AM9_ListContentInfos(u32 *count, u32 amount, MediaType media_type, u64 title_id, u32 offset, ContentInfo *infos);
Result AM9_GetCurrentContentInfoCount(u32 *count);
Result AM9_FindCurrentContentInfos(u32 count, u16 *indices, ContentInfo *infos);
Result AM9_ListCurrentContentInfos(u32 *count, u32 amount, u32 offset, ContentInfo *infos);
Result AM9_DeleteContents(MediaType media_type, u64 title_id, u32 count, u16 *indices);
Result AM9_GetTitleInstalledTicketsCount(u32 *count, u64 title_id);
Result AM9_ListTicketInfos(u32 *count, u32 amount, u64 title_id, u32 offset, TicketInfo *infos);
Result AM9_ExportLicenseTicket(u32 *actual_size, u32 data_size, u64 title_id, u64 ticket_id, void *data);
Result AM9_GetTicketLimitInfos(u32 count, u64 *ticket_ids, TicketLimitInfo *infos);
Result AM9_UpdateImportContentContexts(u32 count, u16 *indices);
Result AM9_GetInternalTitleLocationInfo(InternalTitleLocationInfo *info, MediaType media_type, u64 title_id);
Result AM9_MigrateAGBToSAV(MediaType media_type, u64 title_id);
Result AM9_DeleteTitles(MediaType media_type, u32 count, u64 *title_ids);
Result AM9_GetItemRights(u32 *outval1, u32 *outval2, u32 data_size, u32 unk_enumval, u64 title_id, u64 ticket_id, u32 offset, void *data);
Result AM9_TitleInUse(u8 *in_use, MediaType media_type, u64 title_id);
Result AM9_GetInstalledContentInfoCount(u32 *count, MediaType media_type, u64 title_id);
Result AM9_ListInstalledContentInfos(u32 *count, u32 amount, MediaType media_type, u64 title_id, u32 offset, ContentInfo *infos);
Result AM9_InstallTitleBeginOverwrite(MediaType media_type, u64 title_id);
Result AM9_ExportTicketWrapped(u32 *crypted_ticket_size, u32 *crypted_key_iv_size, u32 crypted_ticket_buffer_size, u32 crypted_key_iv_buffer_size, u64 title_id, u64 ticket_id, void *crypted_ticket, void *crypted_key_iv);

#endif