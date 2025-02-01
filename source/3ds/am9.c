#include <3ds/am9.h>

Handle am9_session;
bool am9_init;

#ifndef REPLACE_AM
static inline Result svcControlServiceSteal(Handle* outHandle, const char* name) {
	register int _op __asm__("r0") = 0;
	register Handle* _outHandle __asm__("r1") = outHandle;
	register const char* _name __asm__("r2") = name;

	register Result res __asm__("r0");

	__asm__ volatile ("svc\t0xB0" : "=r"(res) : "r"(_op), "r"(_outHandle), "r"(_name) : "r3", "r12", "memory");

	return res;
}
#endif

Result am9Init()
{
	if (!am9_init)
	{
		Result res;
#if REPLACE_AM
		static const char am9_srvname[8] = "pxi:am9";
		res = SRV_GetServiceHandle(&am9_session, am9_srvname, 7, 0);
#else
		svcSleepThread(5000000000LL); // gotta wait for stock am to start up
		res = svcControlServiceSteal(&am9_session, "pxi:am9");
#endif

		if (R_SUCCEEDED(res))
			am9_init = true;

		return res;
	}

	return 0;
}

void am9Exit()
{
	#if REPLACE_AM
	if (am9_init)
		Err_FailedThrow(svcCloseHandle(am9_session))
	#endif

	am9_session = 0;
	am9_init = false;
}

Result AM9_GetTitleCount(u32 *count, MediaType media_type)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTitleCount, 1, 0);
	ipc_command[1] = (u32)media_type;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetTitleList(u32 *count, u32 amount, MediaType media_type, u64 *title_ids)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTitleList, 2, 2);
	ipc_command[1] = amount;
	ipc_command[2] = (u32)media_type;
	ipc_command[3] = IPC_Desc_PXIBuffer(amount * sizeof(u64), 0, false);
	ipc_command[4] = (u32)title_ids;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetTitleInfos(MediaType media_type, u32 count, u64 *title_ids, TitleInfo *infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTitleInfos, 2, 4);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = count;
	ipc_command[3] = IPC_Desc_PXIBuffer(count * sizeof(u64), 0, true);
	ipc_command[4] = (u32)title_ids;
	ipc_command[5] = IPC_Desc_PXIBuffer(count * sizeof(TitleInfo), 1, false);
	ipc_command[6] = (u32)infos;

	BASIC_RET(am9_session)
}

Result AM9_DeleteTitle(MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeleteTitle, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	BASIC_RET(am9_session)
}

Result AM9_GetTitleProductCode(char *product_code, MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTitleProductCode, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	CHECK_RET(am9_session)

	if (product_code)
		_memcpy(product_code, &ipc_command[2], 16);

	return res;
}

Result AM9_GetTitleExtDataID(u64 *extdata_id, MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTitleExtDataID, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	CHECK_RET(am9_session)

	if (extdata_id) *extdata_id = *(u64 *)(&ipc_command[2]);

	return res;
}

Result AM9_DeletePendingTitles(MediaType media_type, u8 flags)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeletePendingTitles, 2, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = (u32)flags;

	BASIC_RET(am9_session)
}

Result AM9_InstallFIRM(u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallFIRM, 2, 0);
	ipc_command[1] = LODWORD(title_id);
	ipc_command[2] = HIDWORD(title_id);

	BASIC_RET(am9_session)
}

Result AM9_InstallTicketBegin()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTicketBegin, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_InstallTicketWrite(void *tik_data, u32 tik_data_size)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTicketWrite, 1, 2);
	ipc_command[1] = tik_data_size;
	ipc_command[2] = IPC_Desc_PXIBuffer(tik_data_size, 0, true);
	ipc_command[3] = (u32)tik_data;

	BASIC_RET(am9_session)
}

Result AM9_InstallTicketCancel()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTicketCancel, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_InstallTicketFinish()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTicketFinish, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_DeleteTicket(u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeleteTicket, 2, 0);
	ipc_command[1] = LODWORD(title_id);
	ipc_command[2] = HIDWORD(title_id);

	BASIC_RET(am9_session)
}

Result AM9_GetTicketCount(u32 *count)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTicketCount, 1, 0);

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetTicketList(u32 *count, u32 amount, u32 offset, u64 *title_ids)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTicketList, 2, 2);
	ipc_command[1] = amount;
	ipc_command[2] = offset;
	ipc_command[3] = IPC_Desc_PXIBuffer(amount * sizeof(u64), 0, false);
	ipc_command[4] = (u32)title_ids;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_InstallTitleBegin(MediaType media_type, u64 title_id, u8 db_type)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTitleBegin, 4, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);
	ipc_command[4] = (u32)db_type;

	BASIC_RET(am9_session)
}

Result AM9_InstallTitlePause()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTitlePause, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_InstallTitleResume(MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTitleResume, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	BASIC_RET(am9_session)
}

Result AM9_InstallTMDBegin()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTMDBegin, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_InstallTMDWrite(void *tmd_data, u32 tmd_data_size)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTMDWrite, 1, 2);
	ipc_command[1] = tmd_data_size;
	ipc_command[2] = IPC_Desc_PXIBuffer(tmd_data_size, 0, true);
	ipc_command[3] = (u32)tmd_data;

	BASIC_RET(am9_session)
}

Result AM9_InstallTMDCancel()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTMDCancel, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_InstallTMDFinish(bool unknown)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTMDFinish, 1, 0);
	ipc_command[1] = (u32)unknown;

	BASIC_RET(am9_session)
}

Result AM9_InstallContentBegin(u16 content_index)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallContentBegin, 1, 0);
	ipc_command[1] = (u32)content_index;

	BASIC_RET(am9_session)
}

Result AM9_InstallContentWrite(void *content_data, u32 content_data_size)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallContentWrite, 1, 2);
	ipc_command[1] = content_data_size;
	ipc_command[2] = IPC_Desc_PXIBuffer(content_data_size, 0, true);
	ipc_command[3] = (u32)content_data;

	BASIC_RET(am9_session)
}

Result AM9_InstallContentPause()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallContentPause, 0, 0);
	
	BASIC_RET(am9_session)
}

Result AM9_InstallContentCancel()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallContentCancel, 0, 0);
	
	BASIC_RET(am9_session)
}

Result AM9_InstallContentResume(u16 content_index, u64 *resume_offset)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallContentResume, 1, 0);
	ipc_command[1] = (u32)content_index;
	
	CHECK_RET(am9_session)

	if (resume_offset) *resume_offset = *((u64 *)(&ipc_command[2]));

	return res;
}

Result AM9_InstallContentFinish()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallContentFinish, 0, 0);
	
	BASIC_RET(am9_session)
}

Result AM9_GetPendingTitleCount(u32 *count, MediaType media_type, u8 status_mask)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetPendingTitleCount, 2, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = (u32)status_mask;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetPendingTitleList(u32 *count, u32 amount, MediaType media_type, u8 status_mask, u64 *title_ids)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetPendingTitleList, 3, 2);
	ipc_command[1] = amount;
	ipc_command[2] = (u32)media_type;
	ipc_command[3] = (u32)status_mask;
	ipc_command[4] = IPC_Desc_PXIBuffer(amount * sizeof(u64), 0, false);
	ipc_command[5] = (u32)title_ids;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetPendingTitleInfo(u32 count, MediaType media_type, u64 *title_ids, TitleInfo *title_infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetPendingTitleInfo, 2, 4);
	ipc_command[1] = count;
	ipc_command[2] = (u32)media_type;
	ipc_command[3] = IPC_Desc_PXIBuffer(count * sizeof(u64), 0, true);
	ipc_command[4] = (u32)title_ids;
	ipc_command[5] = IPC_Desc_PXIBuffer(count * sizeof(TitleInfo), 1, false);
	ipc_command[6] = (u32)title_infos;

	BASIC_RET(am9_session)
}

Result AM9_DeletePendingTitle(MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeletePendingTitle, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	BASIC_RET(am9_session)
}

Result AM9_GetImportContentContextsCount(u32 *count, MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetImportContentContextsCount, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetImportContentContextsList(u32 *count, u32 amount, MediaType media_type, u64 title_id, u16 *indices)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetImportContentContextsList, 4, 2);
	ipc_command[1] = amount;
	ipc_command[2] = (u32)media_type;
	ipc_command[3] = LODWORD(title_id);
	ipc_command[4] = HIDWORD(title_id);
	ipc_command[5] = IPC_Desc_PXIBuffer(amount * sizeof(u16), 0, false);
	ipc_command[6] = (u32)indices;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetImportContentContexts(u32 count, MediaType media_type, u64 title_id, u16 *indices, ImportContentContext *contexts)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetImportContentContexts, 4, 4);
	ipc_command[1] = count;
	ipc_command[2] = (u32)media_type;
	ipc_command[3] = LODWORD(title_id);
	ipc_command[4] = HIDWORD(title_id);
	ipc_command[5] = IPC_Desc_PXIBuffer(count * sizeof(u16), 0, true);
	ipc_command[6] = (u32)indices;
	ipc_command[7] = IPC_Desc_PXIBuffer(count * sizeof(ImportContentContext), 1, false);
	ipc_command[8] = (u32)contexts;

	BASIC_RET(am9_session)
}

Result AM9_DeleteImportContentContexts(u32 count, MediaType media_type, u64 title_id, u16 *indices)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeleteImportContentContexts, 4, 2);
	ipc_command[1] = count;
	ipc_command[2] = (u32)media_type;
	ipc_command[3] = LODWORD(title_id);
	ipc_command[4] = HIDWORD(title_id);
	ipc_command[5] = IPC_Desc_PXIBuffer(count * sizeof(u16), 0, true);
	ipc_command[6] = (u32)indices;

	BASIC_RET(am9_session)
}

Result AM9_GetCurrentImportContentContextsCount(u32 *count)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetCurrentImportContentContextsCount, 0, 0);

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetCurrentImportContentContextsList(u32 *count, u32 amount, u16 *indices)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetCurrentImportContentContextsList, 1, 2);
	ipc_command[1] = amount;
	ipc_command[2] = IPC_Desc_PXIBuffer(amount * sizeof(u16), 0, false);
	ipc_command[3] = (u32)indices;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetCurrentImportContentContexts(u32 count, u16 *indices, ImportContentContext *contexts)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetCurrentImportContentContexts, 1, 4);
	ipc_command[1] = count;
	ipc_command[2] = IPC_Desc_PXIBuffer(count * sizeof(u16), 0, true);
	ipc_command[3] = (u32)indices;
	ipc_command[4] = IPC_Desc_PXIBuffer(count * sizeof(ImportContentContext), 1, false);
	ipc_command[5] = (u32)contexts;

	BASIC_RET(am9_session)
}

Result AM9_InstallTitleCancel()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTitleCancel, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_InstallTitleFinish()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTitleFinish, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_InstallTitlesCommit(MediaType media_type, u32 count, u8 db_type, u64 *title_ids)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTitlesCommit, 3, 2);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = count;
	ipc_command[3] = (u32)db_type;
	ipc_command[4] = IPC_Desc_PXIBuffer(count * sizeof(u64), 0, true);
	ipc_command[5] = (u32)title_ids;

	BASIC_RET(am9_session)
}

Result AM9_Sign(u8 *retval, u32 sig_outsize, u32 cert_outsize, u64 title_id, u32 data_size, void *data, void *sig, void *cert)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0]  = IPC_MakeHeader(ID_AM9_Sign, 5, 6);
	ipc_command[1]  = sig_outsize;
	ipc_command[2]  = cert_outsize;
	ipc_command[3]  = LODWORD(title_id);
	ipc_command[4]  = HIDWORD(title_id);
	ipc_command[5]  = data_size;
	ipc_command[6]  = IPC_Desc_PXIBuffer(data_size, 0, true);
	ipc_command[7]  = (u32)data;
	ipc_command[8]  = IPC_Desc_PXIBuffer(sig_outsize, 1, false);
	ipc_command[9]  = (u32)sig;
	ipc_command[10] = IPC_Desc_PXIBuffer(cert_outsize, 2, false);
	ipc_command[11] = (u32)cert;

	CHECK_RET(am9_session)

	if (retval) *retval = (u8)ipc_command[2];

	return res;
}

Result AM9_GetDeviceCertificate(u32 *retval, u32 out_data_size, void *out_data)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetDeviceCertificate, 1, 2);
	ipc_command[1] = out_data_size;
	ipc_command[2] = IPC_Desc_PXIBuffer(out_data_size, 0, false);
	ipc_command[3] = (u32)out_data;

	CHECK_RET(am9_session)

	if (retval) *retval = ipc_command[2];

	return res;
}

Result AM9_GetDeviceID(u64 *id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetDeviceID, 0, 0);

	CHECK_RET(am9_session)

	*id = *((u64 *)&ipc_command[2]);

	return res;
}

Result AM9_ImportCertificates(u32 certsize_1, u32 certsize_2, u32 certsize_3, u32 certsize_4, void *cert_1, void *cert_2, void *cert_3, void *cert_4)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0]  = IPC_MakeHeader(ID_AM9_ImportCertificates, 4, 8);
	ipc_command[1]  = certsize_1;
	ipc_command[2]  = certsize_2;
	ipc_command[3]  = certsize_3;
	ipc_command[4]  = certsize_4;
	ipc_command[5]  = IPC_Desc_PXIBuffer(certsize_1, 0, true);
	ipc_command[6]  = (u32)cert_1;
	ipc_command[7]  = IPC_Desc_PXIBuffer(certsize_2, 1, true);
	ipc_command[8]  = (u32)cert_2;
	ipc_command[9]  = IPC_Desc_PXIBuffer(certsize_3, 2, true);
	ipc_command[10] = (u32)cert_3;
	ipc_command[11] = IPC_Desc_PXIBuffer(certsize_4, 3, true);
	ipc_command[12] = (u32)cert_4;

	BASIC_RET(am9_session)
}

Result AM9_ImportCertificate(u32 certsize, void *cert)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_ImportCertificate, 1, 2);
	ipc_command[1] = certsize;
	ipc_command[2] = IPC_Desc_PXIBuffer(certsize, 0, true);
	ipc_command[3] = (u32)cert;

	BASIC_RET(am9_session)
}

Result AM9_ImportDatabaseInitialized(u8 *initialized, MediaType media_type)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_ImportDatabaseInitialized, 1, 0);
	ipc_command[1] = (u32)media_type;

	CHECK_RET(am9_session);

	if (initialized) *initialized = (u8)ipc_command[2];

	return res;
}

Result AM9_Cleanup(MediaType media_type)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_Cleanup, 1, 0);
	ipc_command[1] = (u32)media_type;

	BASIC_RET(am9_session)
}

Result AM9_DeleteTemporaryTitles()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeleteTemporaryTitles, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_InstallTitlesFinishAndInstallFIRM(MediaType media_type, u32 count, u64 firm_title_id, u8 db_type, u64 *title_ids)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTitlesFinishAndInstallFIRM, 5, 2);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = count;
	ipc_command[3] = LODWORD(firm_title_id);
	ipc_command[4] = HIDWORD(firm_title_id);
	ipc_command[5] = (u32)db_type;
	ipc_command[6] = IPC_Desc_PXIBuffer(count * sizeof(u64), 0, true);
	ipc_command[7] = (u32)title_ids;

	BASIC_RET(am9_session)
}

Result AM9_DSiWareExportVerifyFooter(u64 twl_title_id, u32 data_size, u32 ecdsa_sigsize, u32 device_cert_size, u32 apcert_size, u8 dsiware_export_section_index, void *data, void *ecdsa_sig, void *device_cert, void *apcert)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0]  = IPC_MakeHeader(ID_AM9_DSiWareExportVerifyFooter, 7, 8);
	ipc_command[1]  = LODWORD(twl_title_id);
	ipc_command[2]  = HIDWORD(twl_title_id);
	ipc_command[3]  = data_size;
	ipc_command[4]  = ecdsa_sigsize;
	ipc_command[5]  = device_cert_size;
	ipc_command[6]  = apcert_size;
	ipc_command[7]  = (u32)dsiware_export_section_index;
	ipc_command[8]  = IPC_Desc_PXIBuffer(data_size, 0, true);
	ipc_command[9]  = (u32)data;
	ipc_command[10] = IPC_Desc_PXIBuffer(ecdsa_sigsize, 1, true);
	ipc_command[11] = (u32)ecdsa_sig;
	ipc_command[12] = IPC_Desc_PXIBuffer(device_cert_size, 2, true);
	ipc_command[13] = (u32)device_cert;
	ipc_command[14] = IPC_Desc_PXIBuffer(apcert_size, 3, true);
	ipc_command[15] = (u32)apcert;

	BASIC_RET(am9_session)
}

Result AM9_DSiWareExportDecryptData(u32 input_size, u32 output_size, u32 iv_size, u8 dsiware_export_section_index, void *input, void *input_iv, void *output, void *output_iv)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0]  = IPC_MakeHeader(ID_AM9_DSiWareExportDecryptData, 4, 8);
	ipc_command[1]  = input_size;
	ipc_command[2]  = output_size;
	ipc_command[3]  = iv_size;
	ipc_command[4]  = (u32)dsiware_export_section_index;
	ipc_command[5]  = IPC_Desc_PXIBuffer(input_size, 0, true);
	ipc_command[6]  = (u32)input;
	ipc_command[7]  = IPC_Desc_PXIBuffer(iv_size, 1, true);
	ipc_command[8]  = (u32)input_iv;
	ipc_command[9]  = IPC_Desc_PXIBuffer(output_size, 2, false);
	ipc_command[10] = (u32)output;
	ipc_command[11] = IPC_Desc_PXIBuffer(iv_size, 3, false);
	ipc_command[12] = (u32)output_iv;

	BASIC_RET(am9_session)
}


Result AM9_DSiWareWriteSaveData(u64 twl_title_id, u32 data_size, u32 nand_file_offset, u8 section_type, u8 operation, void *data)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DSiWareWriteSaveData, 6, 2);
	ipc_command[1] = LODWORD(twl_title_id);
	ipc_command[2] = HIDWORD(twl_title_id);
	ipc_command[3] = data_size;
	ipc_command[4] = nand_file_offset;
	ipc_command[5] = (u32)section_type;
	ipc_command[6] = (u32)operation;
	ipc_command[7] = IPC_Desc_PXIBuffer(data_size, 0, true);
	ipc_command[8] = (u32)data;

	BASIC_RET(am9_session)
}

Result AM9_InitializeTitleDatabase(MediaType media_type, bool overwrite)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InitializeTitleDatabase, 2, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = (u32)overwrite;

	BASIC_RET(am9_session)
}

Result AM9_ReloadTitleDatabase(u8 *available, MediaType media_type)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_ReloadTitleDatabase, 1, 0);
	ipc_command[1] = (u32)media_type;

	CHECK_RET(am9_session)

	if (available) *available = (u8)ipc_command[2];

	return res;
}

Result AM9_GetTicketIDCount(u32 *count, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTicketIDCount, 2, 0);
	ipc_command[1] = LODWORD(title_id);
	ipc_command[2] = HIDWORD(title_id);

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetTicketIDList(u32 *count, u32 amount, u64 title_id, bool unknown, u64 *ticket_ids)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTicketIDList, 4, 2);
	ipc_command[1] = amount;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);
	ipc_command[4] = (u32)unknown;
	ipc_command[5] = IPC_Desc_PXIBuffer(amount * sizeof(u64), 0, false);
	ipc_command[6] = (u32)ticket_ids;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_DeleteTicketID(u64 title_id, u64 ticket_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeleteTicketID, 4, 0);
	ipc_command[1] = LODWORD(title_id);
	ipc_command[2] = HIDWORD(title_id);
	ipc_command[3] = LODWORD(ticket_id);
	ipc_command[4] = HIDWORD(ticket_id);

	BASIC_RET(am9_session)
}

Result AM9_GetPersonalizedTicketInfos(u32 *count, u32 amount, TicketInfo *infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetPersonalizedTicketInfos, 1, 2);
	ipc_command[1] = amount;
	ipc_command[2] = IPC_Desc_PXIBuffer(amount * sizeof(TicketInfo), 0, false);
	ipc_command[3] = (u32)infos;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_DSiWareExportCreate(u64 twl_title_id, u32 path_size, u32 buffer_size, u8 export_type, void *path, void *buffer)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DSiWareExportCreate, 5, 4);
	ipc_command[1] = LODWORD(twl_title_id);
	ipc_command[2] = HIDWORD(twl_title_id);
	ipc_command[3] = path_size;
	ipc_command[4] = buffer_size;
	ipc_command[5] = (u32)export_type;
	ipc_command[6] = IPC_Desc_PXIBuffer(path_size, 0, true);
	ipc_command[7] = (u32)path;
	ipc_command[8] = IPC_Desc_PXIBuffer(buffer_size, 1, false);
	ipc_command[9] = (u32)buffer;

	BASIC_RET(am9_session)
}

Result AM9_DSiWareExportInstallTitleBegin(u64 title_id, u8 export_type)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DSiWareExportInstallTitleBegin, 3, 0);
	ipc_command[1] = LODWORD(title_id);
	ipc_command[2] = HIDWORD(title_id);
	ipc_command[3] = (u32)export_type;

	BASIC_RET(am9_session)
}

Result AM9_DSiWareExportGetSize(u32 *size, u64 twl_title_id, u8 export_type)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DSiWareExportGetSize, 3, 0);
	ipc_command[1] = LODWORD(twl_title_id);
	ipc_command[2] = HIDWORD(twl_title_id);
	ipc_command[3] = (u32)export_type;

	CHECK_RET(am9_session)

	if (size) *size = ipc_command[2];

	return res;
}

Result AM9_GetTWLTitleListForReboot(u32 *count, u32 amount, u64 *title_ids, u32 *content_ids)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTWLTitleListForReboot, 1, 4);
	ipc_command[1] = amount;
	ipc_command[2] = IPC_Desc_PXIBuffer(amount * sizeof(u64), 0, false);
	ipc_command[3] = (u32)title_ids;
	ipc_command[4] = IPC_Desc_PXIBuffer(amount * sizeof(u32), 1, false);
	ipc_command[5] = (u32)content_ids;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_DeleteUserDSiWareTitles()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeleteUserDSiWareTitles, 0, 0);

	BASIC_RET(am9_session)
}

Result AM9_DeleteExpiredUserTitles(MediaType media_type)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeleteExpiredUserTitles, 1, 0);
	ipc_command[1] = (u32)media_type;

	BASIC_RET(am9_session)
}

Result AM9_DSiWareExportVerifyMovableSedHash(u32 buf0_size, u32 buf1_size, void *buf0, void *buf1)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DSiWareExportVerifyMovableSedHash, 2, 4);
	ipc_command[1] = buf0_size;
	ipc_command[2] = buf1_size;
	ipc_command[3] = IPC_Desc_PXIBuffer(buf0_size, 0, true);
	ipc_command[4] = (u32)buf0;
	ipc_command[5] = IPC_Desc_PXIBuffer(buf1_size, 1, true);
	ipc_command[6] = (u32)buf1;

	BASIC_RET(am9_session)
}

Result AM9_GetTWLArchiveResourceInfo(TWLArchiveResourceInfo *info)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTWLArchiveResourceInfo, 0, 0);

	CHECK_RET(am9_session)

	if (info)
		_memcpy(info, &ipc_command[2], sizeof(TWLArchiveResourceInfo));

	return res;
}

Result AM9_DSiWareExportValidateSectionMAC(u32 mac_size, u32 hash_size, u8 dsiware_export_section_index, void *mac, void *hash)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DSiWareExportValidateSectionMAC, 3, 4);
	ipc_command[1] = mac_size;
	ipc_command[2] = hash_size;
	ipc_command[3] = (u32)dsiware_export_section_index;
	ipc_command[4] = IPC_Desc_PXIBuffer(mac_size, 0, true);
	ipc_command[5] = (u32)mac;
	ipc_command[6] = IPC_Desc_PXIBuffer(hash_size, 1, true);
	ipc_command[7] = (u32)hash;

	BASIC_RET(am9_session)
}

Result AM9_CheckContentRight(u8 *has_right, u64 title_id, u16 content_index)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_CheckContentRight, 3, 0);
	ipc_command[1] = LODWORD(title_id);
	ipc_command[2] = HIDWORD(title_id);
	ipc_command[3] = (u32)content_index;

	CHECK_RET(am9_session)

	if (has_right) *has_right = (u8)ipc_command[2];

	return res;
}

Result AM9_CreateImportContentContexts(u32 count, u16 *indices)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_CreateImportContentContexts, 1, 2);
	ipc_command[1] = count;
	ipc_command[2] = IPC_Desc_PXIBuffer(count * sizeof(u16), 0, true);
	ipc_command[3] = (u32)indices;

	BASIC_RET(am9_session)
}

Result AM9_GetContentInfoCount(u32 *count, MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetContentInfoCount, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_FindContentInfos(MediaType media_type, u64 title_id, u32 count, u16 *indices, ContentInfo *infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_FindContentInfos, 4, 4);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);
	ipc_command[4] = count;
	ipc_command[5] = IPC_Desc_PXIBuffer(count * sizeof(u16), 0, true);
	ipc_command[6] = (u32)indices;
	ipc_command[7] = IPC_Desc_PXIBuffer(count * sizeof(ContentInfo), 1, false);
	ipc_command[8] = (u32)infos;

	BASIC_RET(am9_session)
}

Result AM9_ListContentInfos(u32 *count, u32 amount, MediaType media_type, u64 title_id, u32 offset, ContentInfo *infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_ListContentInfos, 5, 2);
	ipc_command[1] = amount;
	ipc_command[2] = (u32)media_type;
	ipc_command[3] = LODWORD(title_id);
	ipc_command[4] = HIDWORD(title_id);
	ipc_command[5] = offset;
	ipc_command[6] = IPC_Desc_PXIBuffer(amount * sizeof(ContentInfo), 0, false);
	ipc_command[7] = (u32)infos;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_GetCurrentContentInfoCount(u32 *count)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetCurrentContentInfoCount, 0, 0);

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_FindCurrentContentInfos(u32 count, u16 *indices, ContentInfo *infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_FindCurrentContentInfos, 1, 4);
	ipc_command[1] = count;
	ipc_command[2] = IPC_Desc_PXIBuffer(count * sizeof(u16), 0, true);
	ipc_command[3] = (u32)indices;
	ipc_command[4] = IPC_Desc_PXIBuffer(count * sizeof(ContentInfo), 1, false);
	ipc_command[5] = (u32)infos;

	BASIC_RET(am9_session)
}

Result AM9_ListCurrentContentInfos(u32 *count, u32 amount, u32 offset, ContentInfo *infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_ListCurrentContentInfos, 2, 2);
	ipc_command[1] = amount;
	ipc_command[2] = offset;
	ipc_command[3] = IPC_Desc_PXIBuffer(amount * sizeof(ContentInfo), 0, false);
	ipc_command[4] = (u32)infos;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_DeleteContents(MediaType media_type, u64 title_id, u32 count, u16 *indices)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeleteContents, 4, 2);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);
	ipc_command[4] = count;
	ipc_command[5] = IPC_Desc_PXIBuffer(count * sizeof(u16), 0, true);
	ipc_command[6] = (u32)indices;

	BASIC_RET(am9_session)
}

Result AM9_GetTitleInstalledTicketsCount(u32 *count, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTitleInstalledTicketsCount, 2, 0);
	ipc_command[1] = LODWORD(title_id);
	ipc_command[2] = HIDWORD(title_id);

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_ListTicketInfos(u32 *count, u32 amount, u64 title_id, u32 offset, TicketInfo *infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_ListTicketInfos, 4, 2);
	ipc_command[1] = amount;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);
	ipc_command[4] = offset;
	ipc_command[5] = IPC_Desc_PXIBuffer(amount * sizeof(TicketInfo), 0, false);
	ipc_command[6] = (u32)infos;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_ExportLicenseTicket(u32 *actual_size, u32 data_size, u64 title_id, u64 ticket_id, void *data)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_ExportLicenseTicket, 5, 2);
	ipc_command[1] = data_size;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);
	ipc_command[4] = LODWORD(ticket_id);
	ipc_command[5] = HIDWORD(ticket_id);
	ipc_command[6] = IPC_Desc_PXIBuffer(data_size, 0, false);
	ipc_command[7] = (u32)data;

	CHECK_RET(am9_session)

	if (actual_size) *actual_size = ipc_command[2];

	return res;
}

Result AM9_GetTicketLimitInfos(u32 count, u64 *ticket_ids, TicketLimitInfo *infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetTicketLimitInfos, 1, 4);
	ipc_command[1] = count;
	ipc_command[2] = IPC_Desc_PXIBuffer(count * sizeof(u64), 0, true);
	ipc_command[3] = (u32)ticket_ids;
	ipc_command[4] = IPC_Desc_PXIBuffer(count * sizeof(TicketLimitInfo), 1, false);
	ipc_command[5] = (u32)infos;

	BASIC_RET(am9_session)
}

Result AM9_UpdateImportContentContexts(u32 count, u16 *indices)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_UpdateImportContentContexts, 1, 2);
	ipc_command[1] = count;
	ipc_command[2] = IPC_Desc_PXIBuffer(count * sizeof(u16), 0, true);
	ipc_command[3] = (u32)indices;

	BASIC_RET(am9_session)
}

Result AM9_GetInternalTitleLocationInfo(InternalTitleLocationInfo *info, MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetInternalTitleLocationInfo, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	CHECK_RET(am9_session)

	if (info)
		_memcpy(info, &ipc_command[2], sizeof(InternalTitleLocationInfo));

	return res;
}

Result AM9_MigrateAGBToSAV(MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_MigrateAGBToSAV, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	BASIC_RET(am9_session)
}

Result AM9_DeleteTitles(MediaType media_type, u32 count, u64 *title_ids)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_DeleteTitles, 2, 2);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = count;
	ipc_command[3] = IPC_Desc_PXIBuffer(count * sizeof(u64), 0, true);
	ipc_command[4] = (u32)title_ids;

	BASIC_RET(am9_session)
}

Result AM9_GetItemRights(u32 *outval1, u32 *outval2, u32 data_size, u32 unk_enumval, u64 title_id, u64 ticket_id, u32 offset, void *data)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetItemRights, 7, 2);
	ipc_command[1] = data_size;
	ipc_command[2] = unk_enumval;
	ipc_command[3] = LODWORD(title_id);
	ipc_command[4] = HIDWORD(title_id);
	ipc_command[5] = LODWORD(ticket_id);
	ipc_command[6] = HIDWORD(ticket_id);
	ipc_command[7] = offset;
	ipc_command[8] = IPC_Desc_PXIBuffer(data_size, 0, false);
	ipc_command[9] = (u32)data;

	CHECK_RET(am9_session)

	if (outval1) *outval1 = ipc_command[2];
	if (outval2) *outval2 = ipc_command[3];

	return res;
}

Result AM9_TitleInUse(u8 *in_use, MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_TitleInUse, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	CHECK_RET(am9_session)

	if (in_use) *in_use = (u8)ipc_command[2];

	return res;
}

Result AM9_GetInstalledContentInfoCount(u32 *count, MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_GetInstalledContentInfoCount, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_ListInstalledContentInfos(u32 *count, u32 amount, MediaType media_type, u64 title_id, u32 offset, ContentInfo *infos)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_ListInstalledContentInfos, 5, 2);
	ipc_command[1] = amount;
	ipc_command[2] = (u32)media_type;
	ipc_command[3] = LODWORD(title_id);
	ipc_command[4] = HIDWORD(title_id);
	ipc_command[5] = offset;
	ipc_command[6] = IPC_Desc_PXIBuffer(amount * sizeof(ContentInfo), 0, false);
	ipc_command[7] = (u32)infos;

	CHECK_RET(am9_session)

	if (count) *count = ipc_command[2];

	return res;
}

Result AM9_InstallTitleBeginOverwrite(MediaType media_type, u64 title_id)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0] = IPC_MakeHeader(ID_AM9_InstallTitleBeginOverwrite, 3, 0);
	ipc_command[1] = (u32)media_type;
	ipc_command[2] = LODWORD(title_id);
	ipc_command[3] = HIDWORD(title_id);

	BASIC_RET(am9_session)
}

Result AM9_ExportTicketWrapped(u32 *crypted_ticket_size, u32 *crypted_key_iv_size, u32 crypted_ticket_buffer_size, u32 crypted_key_iv_buffer_size, u64 title_id, u64 ticket_id, void *crypted_ticket, void *crypted_key_iv)
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;

	ipc_command[0]  = IPC_MakeHeader(ID_AM9_ExportTicketWrapped, 6, 4);
	ipc_command[1]  = crypted_ticket_buffer_size;
	ipc_command[2]  = crypted_key_iv_buffer_size;
	ipc_command[3]  = LODWORD(title_id);
	ipc_command[4]  = HIDWORD(title_id);
	ipc_command[5]  = LODWORD(ticket_id);
	ipc_command[6]  = HIDWORD(ticket_id);
	ipc_command[7]  = IPC_Desc_PXIBuffer(crypted_ticket_buffer_size, 0, false);
	ipc_command[8]  = (u32)crypted_ticket;
	ipc_command[9]  = IPC_Desc_PXIBuffer(crypted_key_iv_buffer_size, 1, false);
	ipc_command[10] = (u32)crypted_key_iv;

	CHECK_RET(am9_session)

	if (crypted_ticket_size) *crypted_ticket_size = ipc_command[2];
	if (crypted_key_iv_size) *crypted_key_iv_size = ipc_command[3];

	return res;
}
