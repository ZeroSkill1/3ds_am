#include <am/twlexport.h>

#define BANNER_SIZE       0x4000 // always the same
#define TWLEXPORT_HDR_MAX 0x100
#define TWLEXPORT_FTR_MAX 0x500

#define HB(x) ((AM_TWLExportHeaderBase *)((x)))
#define FB(x) ((AM_TWLExportFooterBase *)((x)))
#define HF(x) ((AM_TWLExportHeaderV2_12 *)((x)))
#define FF(x) ((AM_TWLExportFooterV2_12 *)((x)))

#define FOOTER_ECDSA_SIG(x, info) ((x) + sizeof(AM_TWLExportFooterBase) + (0x20 * info->section_count))
#define FOOTER_ECDSA_APCERT(x, info) ((x) + sizeof(AM_TWLExportFooterBase) + (0x20 * info->section_count) + 60)
#define FOOTER_ECDSA_CTCERT(x, info) ((x) + sizeof(AM_TWLExportFooterBase) + (0x20 * info->section_count) + 60 + 384)
#define FOOTER_SECTION_HASH(x, info, idx) ((x) + sizeof(AM_TWLExportFooterBase) + (0x20 * info->section_count) + 60 + 384 + (idx * 0x20))

#define GET_BLOCKMETA(buf,siz) ((AM_TWLExportBlockMetadata *)(((u8 *)(buf)) + siz))

#define HEADER_OFFSET(buf) (buf + alignMeta(BANNER_SIZE))
#define FOOTER_OFFSET(buf, info) (HEADER_OFFSET(buf) + alignMeta(info->header_size))

#define READ_BLKMETA(section_size) \
		if (R_FAILED(res = FSFile_Read(&read, file, *off + section_size, sizeof(AM_TWLExportBlockMetadata), &blockmeta))) \
			CLOSEFILE_RET(res); \
		if (read != sizeof(AM_TWLExportBlockMetadata)) \
			CLOSEFILE_RET(AM_FAILED_READING_BLOCKMETA)

#define CLOSEFILE_RET(x) {\
	FSFile_Close(file); \
 	return x; }

// can be simplified into structures like this

static const AM_TWLExportTypeInfo EXPORTINFO_V2_12 = { 0x100, 0x500, 12, 0x1C0 };
static const AM_TWLExportTypeInfo EXPORTINFO_V2_11 = {  0xF0, 0x4E0, 11, 0x1A0 };
static const AM_TWLExportTypeInfo EXPORTINFO_V1_4  = {  0xA0, 0x400,  4,  0xC0 };

static const AM_TWLExportTypeInfo *getExportTypeInfo(AM_TWLExportType type)
{
	switch (type)
	{
	// 12 content sections
	case V2_12ContentSections7:
	case V2_12ContentSections8:
	case V2_12ContentSections9:
	case V2_12ContentSectionsA:
	case V2_12ContentSectionsB:
		return &EXPORTINFO_V2_12;
	// 4 content sections
	case V1_4ContentSectionsC:
	case V1_4ContentSectionsD:
		return &EXPORTINFO_V1_4;
	// everything else uses 11 content sections
	default:
		return &EXPORTINFO_V2_11;
	}
}

static inline u32 alignMeta(u32 size)
{
	u32 siz = ALIGN(size, 0x10);
	if (siz) siz += sizeof(AM_TWLExportBlockMetadata);
	return siz;
}

static void convertV1HeaderToV2_12Header(void *out, void *in)
{
	AM_TWLExportHeaderV2_12 *v2_header = (AM_TWLExportHeaderV2_12 *)out;
	AM_TWLExportHeaderV1_4 *v1_header = (AM_TWLExportHeaderV1_4 *)in;

	_memset32_aligned(v2_header, 0x00, sizeof(AM_TWLExportHeaderV2_12));

	_memcpy32_aligned(&v2_header->base, &v1_header->base, sizeof(AM_TWLExportHeaderBase)); // data structure same, no need to do this manually
	
	v2_header->payload_sizes[0] = v1_header->payload_sizes[0]; // TMD size
	v2_header->payload_sizes[1] = v1_header->payload_sizes[1]; // content size
	v2_header->payload_sizes[9] = v1_header->payload_sizes[2]; // public.sav size
	v2_header->payload_sizes[10] = v1_header->payload_sizes[3]; // banner.sav size
	v2_header->payload_sizes[11] = v1_header->unknown_2; // i have no idea what this is tbh

	v2_header->content_indices[0] = v1_header->content_index;

	_memcpy32_aligned(&v2_header->extra_data, &v1_header->extra_data, sizeof(AM_TWLExportHeaderExtraData));
}

static void convertV1FooterToV2_12Footer(void *out, void *in)
{
	AM_TWLExportFooterV2_12 *v2_footer = (AM_TWLExportFooterV2_12 *)out;
	AM_TWLExportFooterV1_4 *v1_footer = (AM_TWLExportFooterV1_4 *)in;

	_memset32_aligned(out, 0x00, sizeof(AM_TWLExportFooterV2_12));

	_memcpy32_aligned(out, in, SIZE_OF_SHA_256_HASH * 2); // banner + header hashes can be done directly
	_memcpy32_aligned(v2_footer->content_section_hashes[0], v1_footer->content_section_hashes[0], SIZE_OF_SHA_256_HASH); // tmd
	_memcpy32_aligned(v2_footer->content_section_hashes[1], v1_footer->content_section_hashes[1], SIZE_OF_SHA_256_HASH); // content
	_memcpy32_aligned(v2_footer->content_section_hashes[9], v1_footer->content_section_hashes[2], SIZE_OF_SHA_256_HASH); // ??
	_memcpy32_aligned(v2_footer->content_section_hashes[10], v1_footer->content_section_hashes[3], SIZE_OF_SHA_256_HASH); // ??

	// copy sigs etc

	_memcpy32_aligned(v2_footer->ecdsa_sig, v1_footer->ecdsa_sig, 60 + (384 * 2) + 4); // real am doesn't do this because N doesn't feel like it
}

static Result decryptSectionVerify(void *in, void *out, void *out_hash, u32 size, AM_TWLExportSectionIndex index)
{
	u8 newiv[0x10];
	AM_TWLExportBlockMetadata *meta = GET_BLOCKMETA(in, size);
	Result res = AM9_DSiWareExportDecryptData(size, size, 0x10, index, in, meta->iv, out, newiv);
	if (R_FAILED(res))
		return res;
	u8 hash[SIZE_OF_SHA_256_HASH];
	struct SHA256 sha;
	sha_256_init(&sha, hash);
	sha_256_write(&sha, out, size);
	sha_256_close(&sha);
	res = AM9_DSiWareExportValidateSectionMAC(0x10, SIZE_OF_SHA_256_HASH, index, meta->cmac, hash);
	if (R_FAILED(res))
		return res;
	if (out_hash)
		_memcpy32_aligned(out_hash, hash, SIZE_OF_SHA_256_HASH);
	return res;
}

static u64 getExportSize(AM_TWLExportHeaderV2_12 *header, const AM_TWLExportTypeInfo *info)
{
	u64 ret = 0;

	#define I(x) alignMeta(x)

	ret += I(BANNER_SIZE);
	ret += I(info->header_size);
	ret += I(info->footer_size);
	ret += I(header->private_save_size);

	for (u8 i = 0; i < 11; i++) // max supported is 11
		ret += I(header->payload_sizes[i]);

	#undef I

	return ret;
}

static Result verifyExportHeader(void *header, const AM_TWLExportTypeInfo *info, u64 filesize)
{
	AM_TWLExportHeaderV2_12 *hdr = (AM_TWLExportHeaderV2_12 *)header;
	return
		hdr->base.magic != 0x54444633 /* '3FDT' */ ||
		getExportSize(hdr, info) != filesize ||
		!hdr->payload_sizes[0] /* tmd */ ||
		!hdr->payload_sizes[1] /* content 0 */ ?
			AM_INVALID_EXPORT : 0;
}

static Result processBegin(AM_TWLExportSectionIndex index, u16 content_index /* null for non-content */)
{
	switch (index)
	{
	case TWLExport_TMD:
		return AM9_InstallTMDBegin();
	case TWLExport_Content:
		return AM9_InstallContentBegin(content_index);
	case TWLExport_PublicSaveData:
	case TWLExport_BannerSaveData:
	case TWLExport_PrivateSaveData:
		return 0;
	default:
		return AM_TWL_EXPORT_INVALID_ENUM_VALUE;
	}
}

static Result processWrite(AM_TWLExportSectionIndex index, u64 twl_title_id, AM_TWLExportType type,
						   void *buffer, u32 data_size, u64 nand_save_offset)
{
	switch (index)
	{
	case TWLExport_TMD:
		return AM9_InstallTMDWrite(buffer, data_size);
	case TWLExport_Content:
		return AM9_InstallContentWrite(buffer, data_size);
	case TWLExport_PublicSaveData:
	case TWLExport_BannerSaveData:
	case TWLExport_PrivateSaveData:
		return AM9_DSiWareWriteSaveData(twl_title_id, data_size, nand_save_offset, index, type, buffer);
	default:
		return AM_TWL_EXPORT_INVALID_ENUM_VALUE;
	}
}

static Result processFinish(AM_TWLExportSectionIndex index)
{
	switch (index)
	{
	case TWLExport_TMD:
		return AM9_InstallTMDFinish(true);
	case TWLExport_Content:
		return AM9_InstallContentFinish();
	case TWLExport_PublicSaveData:
	case TWLExport_BannerSaveData:
	case TWLExport_PrivateSaveData:
		return 0;
	default:
		return AM_TWL_EXPORT_INVALID_ENUM_VALUE;
	}
}

static Result processCancel(AM_TWLExportSectionIndex index)
{
	switch (index)
	{
	case TWLExport_TMD:
		return AM9_InstallTMDCancel();
	case TWLExport_Content:
		return AM9_InstallContentCancel();
	case TWLExport_PublicSaveData:
	case TWLExport_BannerSaveData:
	case TWLExport_PrivateSaveData:
		return 0;
	default:
		return AM_TWL_EXPORT_INVALID_ENUM_VALUE;
	}
}

// it's ninty, it must be a mess

static Result processSection(u32 size, u8 *inhash, void *buf, u32 bufsize, Handle file, u64 *off,
							bool onlyverify, AM_TWLExportSectionIndex index, AM_TWLExportType type,
							u64 twl_title_id, u16 content_index)
{
	AM_TWLExportBlockMetadata blockmeta;
	u8 hash[SIZE_OF_SHA_256_HASH];
	u64 nand_save_offset = 0;
	struct SHA256 sha256;
	Result res;
	u32 read;

	if (!onlyverify && R_FAILED(res = processBegin(index, content_index)))
		return res;

	sha_256_init(&sha256, hash);

	READ_BLKMETA(ALIGN(size, 0x10)) /* don't care, just get it read */

	u32 remaining = size;

	u32 section_misalign = size % 0x10;
	remaining   -= section_misalign;   /* remaining   is now aligned to 0x10 */
	bufsize -= bufsize % 0x10; /* bufsize is now aligned to 0x10 */

	u32 to_process = MIN(bufsize, remaining); /* we can now forget about aligning things:tm: */

	while (remaining)
	{
		if (R_FAILED(res = FSFile_Read(&read, file, *off, to_process, buf)) ||
			R_FAILED(res = AM9_DSiWareExportDecryptData(to_process, to_process, 0x10, index, buf, blockmeta.iv, buf, blockmeta.iv)) ||
			(!onlyverify && R_FAILED(res = processWrite(index, twl_title_id, type, buf, to_process, nand_save_offset))))
		{
			if (!onlyverify) processCancel(index);
			return res;
		}
	
		sha_256_write(&sha256, buf, to_process);
	
		*off += to_process;
		nand_save_offset += to_process;
		remaining -= to_process;
	
		to_process = MIN(bufsize, remaining);
	}

	if (section_misalign)
	{
		if (R_FAILED(res = FSFile_Read(&read, file, *off, 0x10, buf)) || /* read a whole aes block */
			R_FAILED(res = AM9_DSiWareExportDecryptData(0x10, 0x10, 0x10, index, buf, blockmeta.iv, buf, blockmeta.iv)) ||
			((!onlyverify) && R_FAILED(res = processWrite(index, twl_title_id, type, buf, section_misalign, nand_save_offset))))
		{
			if (!onlyverify) processCancel(index);
			return res;
		}
	
		*off += 0x10;
	
		sha_256_write(&sha256, buf, 0x10); /* it hashes the padding too... */
	}

	sha_256_close(&sha256);

	if (R_FAILED(res = AM9_DSiWareExportValidateSectionMAC(0x10, 0x20, index, blockmeta.cmac, hash)) ||
		R_FAILED(res = cmp_hash(hash, inhash) ? 0 : AM_FOOTER_SECTION_HASH_MISMATCH) ||
		((!onlyverify) && R_FAILED(res = processFinish(index))))
	{
		if (!onlyverify) processCancel(index);
		return res;
	}

	*off += sizeof(AM_TWLExportBlockMetadata);

	return 0;
}

/* 
	mset calls this twice when importing a backup
	1 - first it uses export type 4, this type doesn't do any content installation, instead, it just verifies:
		- banner (hash, cmac)
		- header (hash, cmac)
		- footer (cmac, sig, ctcert, apcert)
		- dsiware export file size. this is calculated using sizes from the header
	2 - and finally, if the run above succeeds, it proceeds with rerunning the import but with export type 5.
		this will install the content sections, including the following:
		- TMD (first content section)
		- content(s) 1-8 (content section 2-9)
		- public.sav data (content section 10)
		- banner.sav data (content section 11)
		- private.sav data (content section 12) (see note)

	some notes about private.sav:
		- it is exclusive to backups exported using V2_12 export types. this means it is only included if 
		  there are 12 content sections.
		- the size of it is not a part of the payload sizes in the header. rather, it is at a seemingly odd
		  offset in the header.

	note about export type 0xC (V1_4):
		- this export type seems to be bugged within AM9. it will export tmd, content0 and public.sav
		  if it exists, but it will not write the size of public.sav to the header.
		  due to this, it is impossible to import a backup made using 0xC.
		  (0xD, the only other V1_4 type, is unaffected by this.)
*/

Result AM_ImportTWLBackup(u32 buffer_size, AM_TWLExportType type, Handle file, void *buffer)
{
	Result res = 0;
	u64 export_filesize = 0;
	bool requires_conversion = false;

	// allowed values: 11sections: (2, 4, 5), 12sections: (9, 10, 11)
	if (type != V2_11ContentSections2 &&
		type != V2_11ContentSections4 &&
		type != V2_11ContentSections5 &&
		type != V2_12ContentSections9 &&
		type != V2_12ContentSectionsA &&
		type != V2_12ContentSectionsB)
		CLOSEFILE_RET(AM_TWL_EXPORT_INVALID_ENUM_VALUE)
	
	if (buffer_size < 0x20000)
		CLOSEFILE_RET(AM_GENERAL_INVALIDARG)
	
	if (R_FAILED(res = FSFile_GetSize(&export_filesize, file)))
		CLOSEFILE_RET(res)

	const AM_TWLExportTypeInfo *info = getExportTypeInfo(type);

	// we have to use stack here, otherwise content install would only have (buffer_size - header_size - footer_size)
	// as available working buffer space
	u8 header[TWLEXPORT_HDR_MAX];
	u8 footer[TWLEXPORT_FTR_MAX];

	u8 banner_hash[SIZE_OF_SHA_256_HASH];
	u8 header_hash[SIZE_OF_SHA_256_HASH];

	u32 read = 0;
	u64 offset = 0;

	u32 banner_fullsize = alignMeta(BANNER_SIZE);
	u32 header_fullsize = alignMeta(info->header_size);
	u32 footer_fullsize = alignMeta(info->footer_size);

	u32 hbf_fullsize    = banner_fullsize + header_fullsize + footer_fullsize;

	if (export_filesize < hbf_fullsize)
		CLOSEFILE_RET(AM_INVALID_SIZE)
	else if (R_FAILED(res = FSFile_Read(&read, file, 0, hbf_fullsize, buffer)))
		CLOSEFILE_RET(res)

	// let's do this sequentially, like sane humans

	// 1 - banner (this is not even installed, just verified :thonk:)

	if (R_FAILED(res = decryptSectionVerify(buffer, buffer, &banner_hash, BANNER_SIZE, TWLExport_Banner)))
		CLOSEFILE_RET(res)

	offset += banner_fullsize;

	// 2 - header (complete mess)

	void *headerptr = HEADER_OFFSET(buffer);

	if (R_FAILED(res = decryptSectionVerify(headerptr, header, &header_hash, info->header_size,
											TWLExport_Header)))
	{
		// we don't need to reread the file at this point, other export types read more than required anyway

		if (info->section_count == 12) CLOSEFILE_RET(res) // normal fail, with 12 sections this should not fail

		// may be an older export, try old format (while avoiding more stack usage)

		info = &EXPORTINFO_V1_4;
		type = V1_4ContentSectionsC;

		if (R_FAILED(res = decryptSectionVerify(headerptr, headerptr, &header_hash, info->header_size, 
												TWLExport_Header)))
			CLOSEFILE_RET(res)

		// detected an older header, gotta convert to a v2_12 header

		requires_conversion = true; // so we can immediately convert the v1 footer to a v2 one
		header_fullsize = alignMeta(info->header_size);

		convertV1HeaderToV2_12Header(header, headerptr);
	}

	if (R_FAILED(res = verifyExportHeader(header, info, export_filesize)))
		CLOSEFILE_RET(res)

	res = AM9_DSiWareExportVerifyMovableSedHash(0x10, 0x20, HB(header)->empty_cbc, HB(header)->movable_hash);
	
	if (res != AM_MOVABLE_VERIFY_SUCCESS)
		CLOSEFILE_RET(res)

	offset += header_fullsize;

	// 3 - footer (complete mess #2, why the hell do hashes have to be here if already verified using a CMAC?)

	void *footerptr = FOOTER_OFFSET(buffer, info);

	if (R_FAILED(res = decryptSectionVerify(footerptr, footerptr, NULL, info->footer_size, TWLExport_Footer)))
		CLOSEFILE_RET(res)

	if (R_FAILED(res = AM9_DSiWareExportVerifyFooter(
			HB(header)->twl_title_id,
			info->footer_data_verifysize,
			0x3C,  // constant
			0x180, // constant
			0x180, // constant
			TWLExport_Footer,
			footerptr,
			FOOTER_ECDSA_SIG(footerptr, info),
			FOOTER_ECDSA_CTCERT(footerptr, info),
			FOOTER_ECDSA_APCERT(footerptr, info))))
		CLOSEFILE_RET(res)

	if (requires_conversion)
		convertV1FooterToV2_12Footer(footer, footerptr);
	else
		_memcpy32_aligned(footer, footerptr, info->footer_size);

	if (!cmp_hash(FB(footer)->banner_hash, banner_hash) || !cmp_hash(FB(footer)->header_hash, header_hash))
		CLOSEFILE_RET(AM_FOOTER_SECTION_HASH_MISMATCH)

	// mset uses type 4 (although 10 has the same effect) to validate the export file size, the banner,
	// header and footer sections.
	// content installation will be interrupted and a success result will be returned
	if (type == 4 || type == 10)
		CLOSEFILE_RET(0)

	offset += footer_fullsize;

	// types 2, 9 and 5, 11 can pass through here
	// types 2, 9 will have no tmd, content or save installation
	// only types 5 and 11 will have tmd, content and save installation (5 is the one used by mset)

	bool only_verify = type != 5 && type != 11;

	AM_TWLExportHeaderV2_12 *hdr = HF(header);
	AM_TWLExportFooterV2_12 *ftr = FF(footer);

	u64 title_id = hdr->base.twl_title_id;

	if (!only_verify && R_FAILED(res = AM9_DSiWareExportInstallTitleBegin(title_id, type)))
		CLOSEFILE_RET(res)

	// 4 - tmd

	if (R_FAILED(res = processSection(hdr->payload_sizes[0], ftr->content_section_hashes[0], buffer,
									  buffer_size, file, &offset, only_verify, TWLExport_TMD, type, 0, 0)))
	{
		if (!only_verify) AM9_InstallTitleCancel();
		CLOSEFILE_RET(res)
	}

	// 5 - contents 1-8 (payload index 2-9)

	for (u8 i = 0; i < 8; i++)
	{
		if (!hdr->payload_sizes[i + 1])
			continue;

		if (R_FAILED(res = processSection(hdr->payload_sizes[i + 1], ftr->content_section_hashes[i + 1],
										  buffer, buffer_size, file, &offset, only_verify, TWLExport_Content,
										  type, 0, hdr->content_indices[i])))
		{
			if (!only_verify) AM9_InstallTitleCancel();
			CLOSEFILE_RET(res)
		}
	}

	// 6 - save data (public.sav, banner.sav, private.sav)
	// such a mess, can't find a better way

	if ((hdr->payload_sizes[9] && R_FAILED(res = processSection(hdr->payload_sizes[9], 
																ftr->content_section_hashes[9], buffer,
																buffer_size, file, &offset, only_verify,
																TWLExport_PublicSaveData, type,
																hdr->base.twl_title_id, 0))) ||

		(hdr->payload_sizes[10] && R_FAILED(res = processSection(hdr->payload_sizes[10], 
																ftr->content_section_hashes[10], buffer,
																buffer_size, file, &offset, only_verify,
																TWLExport_BannerSaveData, type,
																hdr->base.twl_title_id, 0))) ||

		(hdr->private_save_size && R_FAILED(res = processSection(hdr->private_save_size, 
																ftr->content_section_hashes[11], buffer,
																buffer_size, file, &offset, only_verify,
																TWLExport_PrivateSaveData, type,
																hdr->base.twl_title_id, 0))))
	{
		if (!only_verify) AM9_InstallTitleCancel();
			CLOSEFILE_RET(res)
	}

	if (!only_verify && (R_FAILED(res = AM9_InstallTitleFinish()) ||
		R_FAILED(res = AM9_InstallTitlesCommit(MediaType_NAND, 1, 1 /* temp db? */, &title_id))))
	{
		if (!only_verify) AM9_InstallTitleCancel();
			CLOSEFILE_RET(res)
	}

	CLOSEFILE_RET(0)
}

Result AM_ReadTWLBackupInfo(u32 buffer_size, u32 export_info_size, u32 banner_size, AM_TWLExportType type,
							Handle file, void *buffer, void *out_export_info, void *banner)
{
	const AM_TWLExportTypeInfo *info = getExportTypeInfo(type);
	Result res = 0;
	u32 read;

	u32 banner_fullsize        = alignMeta(BANNER_SIZE);
	u32 header_fullsize        = alignMeta(info->header_size);
	u32 banner_header_fullsize = banner_fullsize + header_fullsize;

	if (buffer_size < banner_header_fullsize) // real am doesn't do this because ???
		CLOSEFILE_RET(AM_GENERAL_INVALIDARG)     // saves us some checks afterwards

	u64 export_filesize;

	if (R_FAILED(res = FSFile_GetSize(&export_filesize, file)))
		CLOSEFILE_RET(res)
	
	if (export_filesize < banner_header_fullsize) // we only need banner + header anyway
		CLOSEFILE_RET(AM_GENERAL_INVALIDARG);

	if (R_FAILED(res = FSFile_Read(&read, file, 0, banner_header_fullsize, buffer)))
		CLOSEFILE_RET(res)

	if (banner_size) // sane code??? how???
	{
		if R_FAILED(res = decryptSectionVerify(buffer, buffer, NULL, BANNER_SIZE, TWLExport_Banner))
			CLOSEFILE_RET(res)

		banner_size = banner_size >= BANNER_SIZE ? BANNER_SIZE : banner_size;

		_memcpy(banner, buffer, banner_size);
	}

	if (export_info_size) // no need for processing anything really if nothing was requested
	{
		// back to header handling, ugh

		AM_TWLExportHeaderV2_12 *hdr = HEADER_OFFSET(buffer);

		// since we already did (or didn't) copy over the banner, the space the banner is in right now is free
		// for us to use now, avoids any additional stack usage
		if (R_FAILED(res = decryptSectionVerify(hdr, buffer, NULL, info->header_size,
												TWLExport_Header)))
		{
			if (info->section_count == 12)
				CLOSEFILE_RET(res) // again here, with 12 content sections should not fail

			// try reading old header, same as import

			info = &EXPORTINFO_V1_4;
			type = V1_4ContentSectionsC;

			if (R_FAILED(res = decryptSectionVerify(hdr, buffer, NULL, info->header_size,
													TWLExport_Header)))
				CLOSEFILE_RET(res)

			convertV1HeaderToV2_12Header(hdr, buffer); // using same buffer, efficiency:tm:
		}
		else
			_memcpy32_aligned(hdr, buffer, info->header_size);

		// now we have a valid v2 header @ hdr
		// fun

		AM_TWLExportInfo export_info;

		export_info.twl_title_id      = hdr->base.twl_title_id;
		export_info.group_id          = hdr->base.group_id;
		export_info.title_version     = hdr->base.title_version;
		export_info.public_save_size  = hdr->extra_data.public_save_size;
		export_info.private_save_size = hdr->extra_data.private_save_size;
		export_info.reserved_pad      = 0;
		export_info.required_size     = hdr->base.required_size;

		// fun fact: real am doesn't even check the copy size here :)
		_memcpy(out_export_info, &export_info, MIN(export_info_size, sizeof(AM_TWLExportInfo)));
	}

	CLOSEFILE_RET(0)
}