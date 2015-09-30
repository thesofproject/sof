/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <platform/memory.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <uapi/intel-ipc.h>
#include <reef/mailbox.h>
#include <reef/reef.h>

static const struct sst_hsw_ipc_fw_ready ready = {
	.inbox_offset = MAILBOX_HOST_OFFSET + MAILBOX_INBOX_OFFSET,
	.outbox_offset = MAILBOX_HOST_OFFSET + MAILBOX_OUTBOX_OFFSET,
	.inbox_size = MAILBOX_INBOX_SIZE,
	.outbox_size = MAILBOX_OUTBOX_SIZE,
	.fw_info_size = 5,
	.fw_info = {'R','E','E','F',0},
};

int platform_boot_complete(uint32_t boot_message)
{
	uint32_t outbox = MAILBOX_HOST_OFFSET >> 3;

	mailbox_outbox_write(0, &ready, sizeof(ready));

	/* now interrupt host to tell it we are done booting */
	shim_write(SHIM_IPCD, SHIM_IPCD_BUSY | IPC_INTEL_FW_READY | outbox);

	return 0;
}

