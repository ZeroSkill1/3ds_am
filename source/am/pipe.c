#include <am/pipe.h>

static u8 __attribute__((section(".data.cia_cindex"))) cia_content_index[0x2000];

AM_Pipe GLOBAL_PipeManager;

static Result AM_Pipe_CIA_Write(void *data, u64 offset, u32 size, u32 flags, void *indata, u32 *written)
{
	(void)flags;
	(void)offset; // who cares, no stupid restrictions here

	CIAInstallData *c = (CIAInstallData *)data;
	Result res = 0;
	u32 cur_written = 0;
	u8 *chunk = (u8 *)indata;

	DEBUG_PRINTF("start size: ", size);

	if (c->invalidated)
		return AM_INVALIDATED_INSTALLATTION_STATE;

	if (!size)
	{
		DEBUG_PRINT("exiting, no more data available\n");
		*written = 0;
		return 0;
	}

	#define TMD_HDR(x)     ((TMDHeader *)(x))
	#define TIK_HDR(x)     ((TicketHeader *)(x))

	#define CON_ENABLED(x) (cia_content_index[x >> 3] & (0x80 >> (x & 7)))

	#define BASIC_COPY(dst, dsize, off_base, next_state) \
		if (c->offset == (off_base + dsize)) \
		{ \
			c->state = next_state; \
			continue; \
		} \
		\
		u32 write = MIN((off_base + dsize) - c->offset, size); \
		DEBUG_PRINTF("copying size: ", write); \
		DEBUG_PRINTF("          to: ", (u32)&((u8 *)(dst))[c->offset - off_base]); \
		_memcpy(&((u8 *)(dst))[c->offset - off_base], chunk, write); \
		c->offset += write; \
		cur_written += write; \
		size -= write; \
		chunk += write;

	#define SKIP_PADDING(newoff) \
		DEBUG_PRINTF("trying to skip padding from: ", c->offset); \
		DEBUG_PRINTF("                         to: ", newoff); \
		if (c->offset < newoff) \
		{ \
			u32 diff = newoff - c->offset; \
			\
			if (size < diff) \
			{ \
				c->offset += size; \
				cur_written += size; \
				size -= size; \
				continue; \
			} \
			else \
			{ \
				c->offset += diff; \
				cur_written += diff; \
				size -= diff; \
				chunk += diff; \
			} \
		}

	while (true)
	{
		if (!size)
		{
			DEBUG_PRINTF("no more size, wrote total: ", cur_written);
			*written = cur_written;
			return 0;
		}

		switch (c->state)
		{
		case InstallState_Header:
			{
				DEBUG_PRINT("reached install state: header\n");
				BASIC_COPY(&c->header, sizeof(CIAHeader), 0, InstallState_ContentIndex)
			}
			break;
		case InstallState_ContentIndex:
			{
				DEBUG_PRINT("reached install state: content index\n");
				BASIC_COPY(cia_content_index, sizeof(cia_content_index), 0x20, InstallState_CertChainCopy)
			}
			break;
		case InstallState_CertChainCopy:
			{
				DEBUG_PRINT("reached install state: certificate chain copy\n");
				if (!c->header.CertificateChainSize)
				{
					DEBUG_PRINT("no cert chain, skipping to ticket header...\n");
					c->state = InstallState_TicketHeader;
					continue;
				}

				SKIP_PADDING(CRT_START) // skip to 0x2040

				if (!c->buf && !(c->buf = malloc(c->header.CertificateChainSize)))
				{
					res = AM_OUT_OF_MEMORY;
					DEBUG_PRINT("could not allocate buffer for cert chain!\n");
					goto err_exit;
				}

				BASIC_COPY(c->buf, c->header.CertificateChainSize, 0x2040, InstallState_CertChainInstall)
			}
			break;
		case InstallState_CertChainInstall:
			{
				DEBUG_PRINT("reached install state: certificate chain install\n");
				if (R_FAILED(res = AM9_ImportCertificate(c->header.CertificateChainSize, c->buf)))
					goto err_exit;

				DEBUG_PRINT("!! freeing cert chain buffer !!\n");
				free(c->buf);
				c->buf = NULL;
				c->state = InstallState_TicketHeader;
				DEBUG_PRINT("certificate chain imported\n");
			}
			break;
		case InstallState_TicketHeader:
			{
				DEBUG_PRINT("reached install state: ticket header\n");
				if (!c->header.TicketSize)
				{
					DEBUG_PRINT("cia has no ticket, skipping...\n");
					c->state = InstallState_TMD;
					continue;
				}

				SKIP_PADDING(TIK_START(c)) // skip padding leading to ticket

				if (!c->buf && !(c->buf = malloc(sizeof(TicketHeader))))
				{
					res = AM_OUT_OF_MEMORY;
					DEBUG_PRINT("failed allocating ticket header import buffer!\n");
					goto err_exit;
				}

				BASIC_COPY(c->buf, sizeof(TicketHeader), TIK_START(c), InstallState_Ticket)
			}
			break;
		case InstallState_Ticket:
			{
				DEBUG_PRINT("reached install state: ticket\n");
				if (!c->tik_header_imported)
				{
					DEBUG_PRINT("ticket header not imported yet, importing...\n");
					c->ticket_tid = __builtin_bswap64(TIK_HDR(c->buf)->TitleID);

					if ((c->system && !TitleID_IsSystemCTR(c->ticket_tid)) || (c->db_type == TempDB && !TitleID_IsDLPChild(c->ticket_tid)))
					{
						res = AM_TICKET_UNEXPECTED_TITLE_ID;
						DEBUG_PRINT("unexpected ticket title id!\n");
						goto err_exit;
					}

					if (R_FAILED(res = AM9_InstallTicketBegin()))
					{
						DEBUG_PRINTF("install ticket begin failed: ", res);
						goto err_exit;
					}

					c->importing_tik = true;

					DEBUG_PRINTF("install ticket write size: ", sizeof(TicketHeader));

					if (R_FAILED(res = AM9_InstallTicketWrite(c->buf, sizeof(TicketHeader))))
					{
						DEBUG_PRINTF("(header) install ticket write failed: ", res);
						goto err_exit;
					}

					free(c->buf);
					c->buf = NULL;

					// ticket wants a 0x3000 buffer max it seems
					if (!(c->buf = malloc(0x3000)))
					{
						res = AM_OUT_OF_MEMORY;
						DEBUG_PRINT("could not allocate buffer for ticket import!\n");
						goto err_exit;
					}

					c->tik_header_imported = true;
					DEBUG_PRINT("finished ticket header import\n");
				}

				if (c->offset == TIK_END(c))
				{
					DEBUG_PRINT("reached ticket end offset, finishing install\n");
					// if a newer ticket is already installed, it'll error
					// but AM allows it so we must do the same
					if (R_FAILED(res = AM9_InstallTicketFinish()) && res != AM_TRYING_TO_INSTALL_OUTDATED_TICKET)
					{
						DEBUG_PRINTF("install ticket finish failed: ", res);
						goto err_exit;
					}

					DEBUG_PRINT("finished ticket install completely\n");

					free(c->buf);
					c->buf = NULL;
					c->state = InstallState_TMDHeader;
					c->importing_tik = false;
					continue;
				}

				// ticket isn't written crypted, so we don't care about alignments

				u32 write = MIN(MIN(TIK_END(c) - c->offset, size), 0x3000); // stock am never exceeds 0x3000

				DEBUG_PRINTF("install ticket write size: ", write);

				if (R_FAILED(res = AM9_InstallTicketWrite(chunk, write)))
				{
					DEBUG_PRINTF("install ticket write failed: ", res);
					goto err_exit;
				}

				c->offset += write;
				cur_written += write;
				chunk += write;
				size -= write;
			}
			break;
		case InstallState_TMDHeader:
			{
				// tmd must not be skipped

				SKIP_PADDING(TMD_START(c)) // skip padding leading to ticket

				if (!c->buf && !(c->buf = malloc(sizeof(TMDHeader) + 0xC))) // + 0xC for align
				{
					res = AM_OUT_OF_MEMORY;
					DEBUG_PRINT("could not allocate tmd header import buffer!\n");
					goto err_exit;
				}

				BASIC_COPY(c->buf, sizeof(TMDHeader) + 0xC, TMD_START(c), InstallState_TMD)
			}
			break;
		case InstallState_TMD:
			{
				if (!c->tmd_header_imported)
				{
					DEBUG_PRINT("tmd header not imported, importing...\n");
					c->tmd_tid = __builtin_bswap64(TMD_HDR(c->buf)->TitleID);
					c->is_dlc = TitleID_IsDLC(c->tmd_tid);

					if ((c->system && !TitleID_IsSystemCTR(c->tmd_tid)) || (c->db_type == TempDB && !TitleID_IsDLPChild(c->tmd_tid)))
					{
						res = AM_TMD_UNEXPECTED_TITLE_ID;
						DEBUG_PRINT("unexpected tmd title id!\n");
						goto err_exit;
					}

					res = c->overwrite ?
						AM9_InstallTitleBeginOverwrite(c->media, c->tmd_tid) :
						AM9_InstallTitleBegin(c->media, c->tmd_tid, c->db_type);

					if (R_FAILED(res))
					{
						if (c->overwrite)
						{
							DEBUG_PRINTF("install title begin overwrite failed: ", res);
						}
						else
						{
							DEBUG_PRINTF("install title begin failed: ", res);
						}

						goto err_exit;
					}

					c->importing_title = true;

					if (R_FAILED(res = AM9_InstallTMDBegin()))
					{
						DEBUG_PRINTF("install tmd begin failed: ", res);
						goto err_exit;
					}

					c->importing_tmd = true;

					DEBUG_PRINTF("install tmd write size: ", sizeof(TMDHeader) + 0xC);

					if (R_FAILED(res = AM9_InstallTMDWrite(c->buf, sizeof(TMDHeader) + 0xC)))
					{
						DEBUG_PRINTF("(header) install tmd write failed: ", res);
						goto err_exit;
					}

					free(c->buf);
					c->buf = NULL;

					if (!(c->buf = malloc(0x3000)))
					{
						res = AM_OUT_OF_MEMORY;
						DEBUG_PRINT("could not allocate tmd import buffer!\n");
						goto err_exit;
					}

					c->tmd_header_imported = true;
					DEBUG_PRINT("tmd header imported.\n");
				}

				if (c->offset == TMD_END(c))
				{
					DEBUG_PRINT("reached tmd end offset, finalizing import...\n");

					if (R_FAILED(res = AM9_InstallTMDFinish(true))) // no clue wtf the bool is
					{
						DEBUG_PRINTF("install tmd finish failed: ", res);
						goto err_exit;
					}

					free(c->buf);
					c->buf = NULL;
					c->state = InstallState_Content;
					c->cur_content_start_offs = CON_START(c);
					c->importing_tmd = false;
					DEBUG_PRINT("tmd completely imported.\n");
					continue;
				}

				// try depleting the misalign buffer, if we even have one
				if (c->misalign_bufsize)
				{
					DEBUG_PRINTF("[TMD] have a misalign buffer of size: ", (u32)c->misalign_bufsize);
					// enough for a full aes block
					if (c->misalign_bufsize + size >= sizeof(c->misalign_buf))
					{
						DEBUG_PRINT("[TMD] have enough data for full aes block\n");
						u8 diff = 16 - c->misalign_bufsize;
						_memcpy(c->misalign_buf + c->misalign_bufsize, chunk, diff);

						DEBUG_PRINTF("install tmd write (misalign) size: ", 16);

						if (R_FAILED(res = AM9_InstallTMDWrite(c->misalign_buf, 16)))
						{
							DEBUG_PRINTF("install tmd write failed (misalignbuf): ", res);
							goto err_exit;
						}

						c->misalign_bufsize = 0;
						c->offset += diff;
						cur_written += diff;
						size -= diff;
						chunk += diff;
					}
					else // not enough for a full aes block, same ordeal as setting the buf up initially
					{
						_memcpy(c->misalign_buf + c->misalign_bufsize, chunk, size);
						c->misalign_bufsize += size; // increment here, as the buf is not empty here
						c->offset += size;
						cur_written += size;
						size -= size;
						DEBUG_PRINTF("[TMD] not enough data to fill misalign buf, size after push: ", (u32)c->misalign_bufsize);
					}
				}

				u32 remaining_size = TMD_END(c) - c->offset;
				bool last_write = size >= remaining_size;
				u32 write = last_write ? remaining_size : MIN(MIN(remaining_size, size), 0x3000); // stock am never exceeds 0x3000
				
				if (!last_write)
				{
					u8 diff = write % 16; // TMD must align, since it's written to sd crypted
					write -= diff;

					if (diff)
						DEBUG_PRINTF("detected tmd misalign: ", diff);

					// misaligned write, size is not at least 0x10
					if (!write)
					{
						DEBUG_PRINTF("tmd misalign causes misalign buf to be filled with: ", diff);
						_memcpy(c->misalign_buf, chunk, diff);
						c->misalign_bufsize = diff; // set here, misalign buf is empty here
						c->offset += diff;
						cur_written += diff;
						size -= diff;
						// no need for chunk incrementing
						continue;
					}
				}

				DEBUG_PRINTF("install tmd write size: ", write);

				if (R_FAILED(res = AM9_InstallTMDWrite(chunk, write)))
				{
					DEBUG_PRINTF("install tmd write failed: ", res);
					goto err_exit;
				}

				c->offset += write;
				cur_written += write;
				chunk += write;
				size -= write;
			}
			break;
		case InstallState_Content:
			{
				SKIP_PADDING(c->cur_content_start_offs)

				// for DLC, after first content, commit required
				// install title stop, commit, restart title install
				if (c->is_dlc &&
					((!c->batch_size && c->num_contents_imported == 1) || // first content, DLC needs a commit here to continue
					 (c->batch_size && c->num_contents_imported_batch == c->batch_size))) // batch finished
				{
					DEBUG_PRINTF("commit required, current imported: ", c->num_contents_imported);
					DEBUG_PRINTF("                   imported batch: ", c->num_contents_imported_batch);
					if (R_FAILED(res = AM9_InstallTitleFinish()))
						goto err_exit;

					c->importing_title = false;

					if (R_FAILED(res = AM9_InstallTitlesCommit(c->media, 1, c->db_type, &c->tmd_tid)))
					{
						DEBUG_PRINTF("install titles commit failed: ", res);
						goto err_exit;
					}

					if (R_FAILED(res = AM9_InstallTitleBegin(c->media, c->tmd_tid, c->db_type)))
					{
						DEBUG_PRINTF("install title begin failed: ", res);
						goto err_exit;
					}

					c->importing_title = true;
					c->num_contents_imported_batch = 0; // reset batch count
					c->batch_size = 0; // reset batch size
				}

				// we aren't importing anything at the moment, try initializing an import
				if (!c->importing_content)
				{
					DEBUG_PRINT("not importing content, trying to set up an import\n");
					DEBUG_PRINTF("current index: ", c->cindex);
					DEBUG_PRINTF("is dlc: ", (u32)c->is_dlc);
					DEBUG_PRINTF("imported contents: ", c->num_contents_imported);
					DEBUG_PRINTF("batch size: ", c->batch_size);

					// curse you ninty for having this restriction for DLC
					if (c->is_dlc && c->num_contents_imported >= 1 && !c->batch_size)
					{
						u16 batch_indices[64]; // installing 64 contents in a batch is way faster
						// ! do not update the cindex here, we need the old value later
						for (u16 i = c->cindex; i < 0xFFFF && c->batch_size < 64; i++)
						{
							if (CON_ENABLED(i))
							{
								batch_indices[c->batch_size] = i; // can use size as index here
								DEBUG_PRINTF("added to batch buffer: ", i);
								c->batch_size++;
							}
						}

						// essentially once the batch has been set up, content imports can resume
						// normally until all content indices that are used to create the import
						// contexts below have been imported
						// at which point we have to repeat this process...

						if (c->batch_size &&
							R_FAILED(res = AM9_CreateImportContentContexts(c->batch_size, batch_indices)))
						{
							DEBUG_PRINTF("create import content contexts failed: ", res);
							goto err_exit;
						}
					}

					bool content_found = false;

					DEBUG_PRINT("trying to find an enabled content to install\n");
					DEBUG_PRINTF("starting with content index: ", c->cindex);

					// find enabled content
					for (; c->cindex < 0xFFFF; c->cindex++)
					{
						if (CON_ENABLED(c->cindex))
						{
							if (R_FAILED(res = AM9_InstallContentBegin(c->cindex)))
							{
								DEBUG_PRINTF("install content begin failed: ", res);
								goto err_exit;
							}

							c->importing_content = true;

							ImportContentContext ctx;

							if (R_FAILED(res = AM9_GetCurrentImportContentContexts(1, &c->cindex, &ctx)))
							{
								DEBUG_PRINTF("get current import content contexts failed: ", res);
								goto err_exit;
							}

							c->cur_content_start_offs = ALIGN(c->offset, 0x40);
							c->cur_content_end_offs = c->cur_content_start_offs + (u32)ctx.size;
							content_found = true;
							DEBUG_PRINT("content import initialized.\n");
							DEBUG_PRINTF("current offset: ", c->offset);
							DEBUG_PRINTF("content start offset: ", c->cur_content_start_offs);
							DEBUG_PRINTF("content end offset: ", c->cur_content_end_offs);
							break;
						}
					}

					if (content_found)
					{
						DEBUG_PRINT("content was found, relooping...\n");
						continue;
					}

					DEBUG_PRINT("content was NOT found, jumping to finalize...\n");
					// if code reaches here, no more content to import
					c->state = InstallState_Finalize;
					continue;
				}

				// try depleting the misalign buffer, if we even have one
				if (c->misalign_bufsize)
				{
					DEBUG_PRINTF("[content] have a misalign buffer of size: ", c->misalign_bufsize);
					// enough for a full aes block
					if (c->misalign_bufsize + size >= sizeof(c->misalign_buf))
					{
						DEBUG_PRINT("[content] have enough data for depleting misalign buf\n");
						u8 diff = 16 - c->misalign_bufsize;
						_memcpy(c->misalign_buf + c->misalign_bufsize, chunk, diff);

						DEBUG_PRINTF("install content write (misalign_buf) size: ", 16);
						if (R_FAILED(res = AM9_InstallContentWrite(c->misalign_buf, 16)))
						{
							DEBUG_PRINTF("install content write failed (misalign_buf): ", res);
							goto err_exit;
						}

						c->misalign_bufsize = 0;
						c->offset += diff;
						cur_written += diff;
						size -= diff;
						chunk += diff;
					}
					else // not enough for a full aes block, same ordeal as setting the buf up initially
					{
						DEBUG_PRINTF("[content] don't have enough data for misalign buffer of size: ", c->misalign_bufsize);
						_memcpy(c->misalign_buf + c->misalign_bufsize, chunk, size);
						c->misalign_bufsize += size; // increment here, as the buf is not empty here
						c->offset += size;
						cur_written += size;
						size -= size;
						// no need for chunk incrementing
					}
				}

				if (size)
				{
					u32 remaining_size = c->cur_content_end_offs - c->offset;
					bool last_write = size >= remaining_size;
					u32 write = last_write ? remaining_size : MIN(remaining_size, size); // stock am never exceeds 0x3000
					
					if (!last_write)
					{
						u8 diff = write % 16; // content must align, since it's written to sd crypted
						write -= diff;

						if (diff)
							DEBUG_PRINTF("detected content misalign: ", diff);

						// misaligned write, size is not at least 0x10
						if (!write)
						{
							DEBUG_PRINTF("content misalign causes misalign buf to be filled with: ", diff);
							_memcpy(c->misalign_buf, chunk, diff);
							c->misalign_bufsize = diff; // set here, misalign buf is empty here
							c->offset += diff;
							cur_written += diff;
							size -= diff;
							// no need for chunk incrementing
							goto check_finish_content;
						}
					}

					if (!write)
						goto check_finish_content;

					DEBUG_PRINTF("content write size: ", write);
					if (R_FAILED(res = AM9_InstallContentWrite(chunk, write)))
					{
						DEBUG_PRINTF("install content write failed: ", res);
						goto err_exit;
					}

					c->offset += write;
					cur_written += write;
					chunk += write;
					size -= write;
				}

check_finish_content:
				// finish off installing content, if needed
				if (c->offset == c->cur_content_end_offs)
				{
					DEBUG_PRINTF("finalizing installation for content: ", c->cindex);
					DEBUG_PRINTF("                             offset: ", c->offset);

					if (R_FAILED(res = AM9_InstallContentFinish()))
					{
						DEBUG_PRINTF("install content finish failed: ", res);
						goto err_exit;
					}

					// content 0 is NOT part of a batch
					if (c->is_dlc && c->num_contents_imported != 0)
					{
						DEBUG_PRINTF("is dlc, incrementing, before: ", c->num_contents_imported_batch);
						c->num_contents_imported_batch++;
					}

					c->num_contents_imported++;
					c->cindex++;
					c->importing_content = false;
				}
			}
			break;
		case InstallState_Finalize:
			{
				return 0; // eh
			}
			break;
		}
	}

err_exit:
	assertNotAmOrFs(res);

	if (c->buf)
	{
		free(c->buf);
		c->buf = NULL;
	}

	c->invalidated = true;

	return res;
	
	#undef TMD_HDR
	#undef TIK_HDR
	#undef BASIC_COPY
	#undef SKIP_PADDING
}

static Result AM_Pipe_Ticket_Write(void *data, u64 offset, u32 size, u32 flags, void *indata, u32 *written)
{
	(void)flags;
	(void)offset;
	(void)data;

	Result res = AM9_InstallTicketWrite(indata, size);
	assertNotAmOrFs(res);

	if (R_SUCCEEDED(res))
	{
		DEBUG_PRINTF("successfully wrote ticket bytes, size: ", size);
		*written = size;
	}

	return res;
}

static Result AM_Pipe_TMD_Write(void *data, u64 offset, u32 size, u32 flags, void *indata, u32 *written)
{
	(void)flags;
	(void)offset;
	(void)data;

	Result res = AM9_InstallTMDWrite(indata, size);
	assertNotAmOrFs(res);

	if (R_SUCCEEDED(res))
	{
		DEBUG_PRINTF("successfully wrote tmd bytes, size: ", size);
		*written = size;
	}

	return res;
}

static Result AM_Pipe_Content_Write(void *data, u64 offset, u32 size, u32 flags, void *indata, u32 *written)
{
	(void)flags;
	(void)offset;
	(void)data;

	Result res = AM9_InstallContentWrite(indata, size);
	assertNotAmOrFs(res);

	if (R_SUCCEEDED(res))
	{
		DEBUG_PRINTF("successfully wrote content bytes, size: ", size);
		*written = size;
	}

	return res;
}

bool atomicUpdateState(u8 *src, u8 val, u8 wanted)
{
	u8 cur;

	while (true)
	{
		cur = __ldrexb(src);

		if (cur != wanted)
		{
			__clrex();
			return false;
		}

		cur = val;

		if (__strexb(src, cur) == 0)
			return true;
	}
}

Result AM_Pipe_CreateImportHandle(AM_Pipe *pipe, AM_PipeWriteImpl impl, void *data, Handle *import)
{
	RecursiveLock_Lock(&pipe->lock); // lock so we have exclusive access

	// check if a pipe is already initialized, if so then we return an error
	// expected value: false, value to update to: true
	if (!atomicUpdateState(&pipe->init, true, false))
	{
		DEBUG_PRINT("pipe already initialized!\n");
		RecursiveLock_Unlock(&pipe->lock);
		return AM_PIPE_ALREADY_INITIALIZED;
	}

	Handle session;

	// create out session, this is what the user will utilize to access am:pipe ipcs
	Result res = svcCreateSessionToPort(&session, pipe->port_client);

	if (R_FAILED(res))
	{
		DEBUG_PRINTF("svcCreateSessionToPort failed: ", res);
		RecursiveLock_Unlock(&pipe->lock);
		return res;
	}

	pipe->write = impl;
	pipe->data = data;

	// must wait for the pipe thread to start, else the user could try am:pipe ipcs while we're not 
	// ready yet

	DEBUG_PRINT("waiting for thread to start...\n");

	LightEvent_Wait(&pipe->event);

	// safe to send handle now
	*import = session;

	DEBUG_PRINT("handle sent.\n");

	RecursiveLock_Unlock(&pipe->lock);

	DEBUG_PRINT("successfully created import handle.\n");

	return res;
}

void AM_Pipe_CloseImportHandle(AM_Pipe *pipe, Handle import)
{
	svcCloseHandle(import);
	atomicUpdateState(&pipe->init, false, true);
}

void AM_Pipe_EnsureThreadExit(AM_Pipe *pipe)
{
	RecursiveLock_Lock(&pipe->lock);

	if (GLOBAL_PipeManager.init) // just ensuring we don't have a pipe thread running
	{
		Err_FailedThrow(svcSignalEvent(pipe->thread_close_event));
		svcWaitSynchronization(pipe->thread, -1);
		atomicUpdateState(&pipe->init, false, true);
	}

	RecursiveLock_Unlock(&pipe->lock);
}

Result AM_Pipe_CreateCIAImportHandle(AM_Pipe *pipe, MediaType media_type, u8 title_db_type, bool overwrite, bool is_system, Handle *import)
{
	CIAInstallData *data = malloc(sizeof(CIAInstallData));

	if (!data)
		return AM_OUT_OF_MEMORY;

	_memset(data, 0x00, sizeof(CIAInstallData));

	data->media = media_type;
	data->db_type = title_db_type;
	data->overwrite = overwrite;
	data->system = is_system;
	data->state = InstallState_Header;

	Result res = AM_Pipe_CreateImportHandle(pipe, &AM_Pipe_CIA_Write, data, import);

	if (R_SUCCEEDED(res))
	{
		DEBUG_PRINT("created cia pipe successfully\n");
		return res;
	}

	free(data);

	return res;
}

Result AM_Pipe_CreateTicketImportHandle(AM_Pipe *pipe, Handle *import)
{
	Result res = AM_Pipe_CreateImportHandle(pipe, &AM_Pipe_Ticket_Write, NULL, import);

	if (R_SUCCEEDED(res))
		DEBUG_PRINT("created ticket pipe successully\n");

	return res;
}

Result AM_Pipe_CreateTMDImportHandle(AM_Pipe *pipe, Handle *import)
{
	Result res = AM_Pipe_CreateImportHandle(pipe, &AM_Pipe_TMD_Write, NULL, import);

	if (R_SUCCEEDED(res))
		DEBUG_PRINT("created tmd pipe successfully\n");

	return res;
}

Result AM_Pipe_CreateContentImportHandle(AM_Pipe *pipe, Handle *import)
{
	Result res = AM_Pipe_CreateImportHandle(pipe, &AM_Pipe_Content_Write, NULL, import);

	if (R_SUCCEEDED(res))
		DEBUG_PRINT("created content pipe successfully\n");

	return res;
}

void AM_Pipe_HandleIPC()
{
	u32 *ipc_command = getThreadLocalStorage()->ipc_command;
	u32 cmd_header = ipc_command[0];
	u16 cmd_id = (cmd_header >> 16) & 0xFFFF;

	switch(cmd_id)
	{
	case 0x0001: // dummy
	case 0x0401: // control
	case 0x0801: // open sub file
	case 0x0802: // read
	case 0x0804: // get size
	case 0x0805: // set size
	case 0x0806: // get attributes
	case 0x0807: // set attributes
	case 0x080A: // set priority
	case 0x080B: // get priority
	case 0x080C: // open link file
		{
			CHECK_HEADER(IPC_MakeHeader(cmd_id, 0, 0))

			ipc_command[0] = IPC_MakeHeader(cmd_id, 1, 0);
			ipc_command[1] = AM_PIPE_UNSUPPORTED_ACTION;
		}
		break;
	case 0x0808: // close
	case 0x0809: // flush
		{
			ipc_command[0] = IPC_MakeHeader(cmd_id, 1, 0);
			ipc_command[1] = 0;
		}
		break;
	case 0x0803: // write
		{
			CHECK_HEADER(IPC_MakeHeader(0x0803, 4, 2))

			u32 written;
			u64 offset = *((u64 *)&ipc_command[1]);
			u32 size = ipc_command[3];
			u32 flags = ipc_command[4];

			CHECK_WRONGARG
			(
				!IPC_VerifyBuffer(ipc_command[5], IPC_BUFFER_R) ||
				IPC_GetBufferSize(ipc_command[5]) != size
			)

			void *buf = (void *)ipc_command[6];
			
			Result res = GLOBAL_PipeManager.write(GLOBAL_PipeManager.data, offset, size, flags, buf, &written);

			ipc_command[0] = IPC_MakeHeader(0x0803, 2, 2);
			ipc_command[1] = res;
			ipc_command[2] = written;
			ipc_command[3] = IPC_Desc_Buffer(size, IPC_BUFFER_R);
			ipc_command[4] = (u32)buf;
		}
		break;
	}
}