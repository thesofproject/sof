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
#include <platform/dma.h>
#include <platform/clk.h>
#include <uapi/intel-ipc.h>
#include <reef/mailbox.h>
#include <reef/dai.h>
#include <reef/dma.h>
#include <reef/reef.h>
#include <reef/audio/component.h>

static const struct sst_hsw_ipc_fw_ready ready = {
	.inbox_offset = MAILBOX_HOST_OFFSET + MAILBOX_INBOX_OFFSET,
	.outbox_offset = MAILBOX_HOST_OFFSET + MAILBOX_OUTBOX_OFFSET,
	.inbox_size = MAILBOX_INBOX_SIZE,
	.outbox_size = MAILBOX_OUTBOX_SIZE,
	.fw_info_size = sizeof(struct fw_info),
	.info = {
		.name = "REEF",
		.date = __DATE__,
		.time = __TIME__,
	},
};

int platform_boot_complete(uint32_t boot_message)
{
	uint32_t outbox = MAILBOX_HOST_OFFSET >> 3;

	mailbox_outbox_write(0, &ready, sizeof(ready));

	/* now interrupt host to tell it we are done booting */
	shim_write(SHIM_IPCD, SHIM_IPCD_BUSY | IPC_INTEL_FW_READY | outbox);

	return 0;
}

int platform_init(int argc, char *argv[])
{
	struct dma *dmac0, *dmac1;
	struct dai *ssp0, *ssp1;

	dmac0 = dma_get(DMA_ID_DMAC0);
	dma_probe(dmac0);

	dmac1 = dma_get(DMA_ID_DMAC1);
	dma_probe(dmac1);

	ssp0 = dai_get(COMP_UUID(COMP_VENDOR_INTEL, DAI_UUID_SSP0));
	dai_probe(ssp0);

	ssp1 = dai_get(COMP_UUID(COMP_VENDOR_INTEL, DAI_UUID_SSP1));
	dai_probe(ssp1);

	init_platform_clocks();

	return 0;
}
