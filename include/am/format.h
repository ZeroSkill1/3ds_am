#ifndef _AM_FORMAT_H
#define _AM_FORMAT_H

#include <3ds/types.h>
#include <sha256.h>

#define HDR_START      0x0
#define HDR_END        0x20
#define IDX_START      HDR_END
#define IDX_END        (IDX_START + 0x2000)
#define CRT_START      ALIGN(IDX_END, 0x40)
#define CRT_END(x)     (CRT_START + (x)->header.CertificateChainSize)
#define TIK_START(x)   ALIGN(CRT_END((x)), 0x40)
#define TIK_END(x)     (TIK_START((x)) + (x)->header.TicketSize)
#define TMD_START(x)   ALIGN(TIK_END((x)), 0x40)
#define TMD_END(x)     (TMD_START((x)) + (x)->header.TMDSize)
#define CON_START(x)   ALIGN(TMD_END((x)), 0x40)
#define CON_END(x)     (CON_START((x)) + (x)->header.ContentSize)

typedef struct __attribute__((packed)) TicketHeader
{
	char Signature[0x140];
	char Issuer[0x40];
	u8 ECCPublicKey[0x3C];
	u8 Version;
	u8 CACRLVersion;
	u8 SignerCRLVersion;
	u8 Titlekey[0x10];
	u8 reserved_0;
	u64 TicketID;
	u32 ConsoleID;
	u64 TitleID;
	u8 reserved_1[2];
	u16 TitleVersion;
	u8 reserved_2[0x8];
	u8 LicenseType;
	u8 CommonKeyYIndex;
	u8 reserved_3[0x2A];
	u32 AccountID;
	u8 reserved_4;
	u8 Audit;
	u8 reserved_5[0x42];
	u8 Limits[0x40];
} TicketHeader;

typedef struct __attribute__((packed)) TMDHeader
{
	u8 Signature[0x140];
	char Issuer[0x40];
	u8 Version;
	u8 CACRLVersion;
	u8 SignerCRLVersion;
	u8 reserved_0;
	u64 SystemVersion;
	u64 TitleID;
	u32 TitleType;
	u16 GroupID;
	union
	{
		u32 CTRSaveDataSize;
		u32 SRLPublicSaveDataSize;
	} SaveSize;
	u32 SRLPrivateSaveDataSize;
	u8 reserved_1[4];
	u8 SRLFlag;
	u8 reserved_2[0x31];
	u32 AccessRights;
	u16 TitleVersion;
	u16 ContentCount;
	u16 BootContent;
	u8 padding[2];
	u8 ContentInfoRecordsHash[SIZE_OF_SHA_256_HASH];
} TMDHeader;

typedef struct __attribute__((packed)) ContentChunkRecord
{
	u32 ID;
	u16 Index;
	u16 Type;
	u64 Size;
	u8  Hash[SIZE_OF_SHA_256_HASH];
} ContentChunkRecord;

typedef struct __attribute__((packed)) ContentInfoRecord
{
	u16 IndexOffset;
	u16 Count;
	u8 hash[SIZE_OF_SHA_256_HASH];
} ContentInfoRecord;

typedef struct __attribute__((packed)) CIAHeader
{
	s32 Size;
	u16 Type;
	u16 Version;
	u32 CertificateChainSize;
	u32 TicketSize;
	u32 TMDSize;
	u32 MetaSize;
	u64 ContentSize;
} CIAHeader;

#endif