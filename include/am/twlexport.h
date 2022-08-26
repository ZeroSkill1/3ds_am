#ifndef _AM_TWLEXPORT_H
#define _AM_TWLEXPORT_H

#include <3ds/result.h>
#include <3ds/types.h>
#include <3ds/am9.h>
#include <am/util.h>
#include <3ds/fs.h>
#include <sha256.h>
#include <errors.h>

typedef enum AM_TWLExportType
{
	// 11 content sections
	V2_11ContentSectionsDefault0 = 0x0,
	V2_11ContentSections2        = 0x2,
	V2_11ContentSections4        = 0x4,
	V2_11ContentSections5        = 0x5,
	// 12 content sections
	V2_12ContentSections7        = 0x7,
	V2_12ContentSections8        = 0x8,
	V2_12ContentSections9        = 0x9,
	V2_12ContentSectionsA        = 0xA,
	V2_12ContentSectionsB        = 0xB,
	// 4 content sections
	V1_4ContentSectionsC         = 0xC,  // this type is bugged, am9 doesn't write save data size into the header so it's impossible to import
	V1_4ContentSectionsD         = 0xD,  // the only working v1_4 export type
} AM_TWLExportType;

typedef enum AM_TWLExportSectionIndex
{
	TWLExport_Banner = 0x0,
	TWLExport_Header = 0x1,
	TWLExport_Footer = 0x2,
	TWLExport_TMD = 0x3,
	TWLExport_Content = 0x4,
	TWLExport_PublicSaveData = 0x5,
	TWLExport_BannerSaveData = 0x6,
	TWLExport_PrivateSaveData = 0x7,
} AM_TWLExportSectionIndex;

typedef struct AM_TWLExportTypeInfo
{
	u32 header_size;
	u32 footer_size;
	u32 section_count;
	u32 footer_data_verifysize;
} AM_TWLExportTypeInfo;

typedef struct __attribute__((packed)) AM_TWLExportBlockMetadata
{
	u8 cmac[0x10];
	u8 iv[0x10];
} AM_TWLExportBlockMetadata;

typedef struct __attribute__((packed)) AM_TWLExportHeaderBase // size: 0x48
{
	u32 magic;              // 0x0  : 0x4
	u16 group_id;           // 0x4  : 0x6
	u16 title_version;      // 0x6  : 0x8
	u8  movable_hash[0x20]; // 0x8  : 0x28
	u8  empty_cbc[0x10];    // 0x28 : 0x38
	u64 twl_title_id;       // 0x38 : 0x40
	u64 required_size;      // 0x40 : 0x48
} AM_TWLExportHeaderBase;

typedef struct __attribute__((packed)) AM_TWLExportHeaderExtraData
{
	u32 public_save_size;
	u32 private_save_size;
	u8  unk[54];
} AM_TWLExportHeaderExtraData;

typedef struct __attribute__((packed)) AM_TWLExportHeaderV1_4 // size: 0xA0 (160)
{
	AM_TWLExportHeaderBase base;            // 0x0  : 0x48
	u32 payload_sizes[4];                   // 0x48 : 0x58
	u32 unknown_2;                          // 0x58 : 0x5C
	AM_TWLExportHeaderExtraData extra_data; // 0x5C : 0x9A
	u16 content_index;                      // 0x9A : 0x9C
	u8  unknown_4[4];                       // 0x9C : 0xA0
} AM_TWLExportHeaderV1_4;

typedef struct __attribute__((packed)) AM_TWLExportHeaderV2_11 // size: 0xF0 (240)
{
	AM_TWLExportHeaderBase base;            // 0x0  : 0x48
	u32 payload_sizes[11];                  // 0x48 : 0x74
	u8  unknown[32];                        // 0x74 : 0x94
	u16 content_indices[8];                 // 0x94 : 0xA4
	AM_TWLExportHeaderExtraData extra_data; // 0xA4 : 0xE2
	u8  pad[0xE];                           // 0xE2 : 0xF0
} AM_TWLExportHeaderV2_11;

typedef struct __attribute__((packed)) AM_TWLExportHeaderV2_12 // size: 0x100 (256)
{
	AM_TWLExportHeaderBase base;            // 0x0  : 0x48
	u32 payload_sizes[12];                  // 0x48 : 0x78
	u8  unknown_2[28];                      // 0x78 : 0x94
	u16 content_indices[8];                 // 0x94 : 0xA4
	AM_TWLExportHeaderExtraData extra_data; // 0xA4 : 0xE2
	u8  pad[0xE];                           // 0xE2 : 0xF0
	u32 private_save_size;                  // 0xF0 : 0xF4
	u8  pad2[0xC];                          // 0xF4 : 0x100
} AM_TWLExportHeaderV2_12;

typedef struct __attribute__((packed)) AM_TWLExportFooterBase
{
	u8  banner_hash[0x20];
	u8  header_hash[0x20];
} AM_TWLExportFooterBase;

typedef struct __attribute__((packed)) AM_TWLExportFooterV1_4
{
	AM_TWLExportFooterBase base;
	u8  content_section_hashes[4][0x20];
	u8  ecdsa_sig[60];
	u8  ecdsa_apcert[384];
	u8  ecdsa_ctcert[384];
	u32 pad;
} AM_TWLExportFooterV1_4;

typedef struct __attribute__((packed)) AM_TWLExportFooterV2_11
{
	AM_TWLExportFooterBase base;
	u8  content_section_hashes[11][0x20];
	u8  ecdsa_sig[60];
	u8  ecdsa_apcert[384];
	u8  ecdsa_ctcert[384];
	u32 pad;
} AM_TWLExportFooterV2_11;

typedef struct __attribute__((packed)) AM_TWLExportFooterV2_12
{
	AM_TWLExportFooterBase base;
	u8  content_section_hashes[12][0x20];
	u8  ecdsa_sig[60];
	u8  ecdsa_apcert[384];
	u8  ecdsa_ctcert[384];
	u32 pad;
} AM_TWLExportFooterV2_12;

typedef struct __attribute__((packed)) AM_TWLExportInfo
{
	u64 twl_title_id;
	u16 group_id;
	u16 title_version;
	u32 public_save_size;
	u32 private_save_size;
	u32 reserved_pad;
	u64 required_size;
} AM_TWLExportInfo;

Result AM_ImportTWLBackup(u32 buffer_size, AM_TWLExportType export_type, Handle file, void *buffer);
Result AM_ReadTWLBackupInfo(u32 buffer_size, u32 export_info_size, u32 banner_size, AM_TWLExportType type, Handle file, void *buffer, void *out_export_info, void *banner);

#endif