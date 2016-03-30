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
#include <platform/timer.h>
#include <platform/pmc.h>
#include <uapi/intel-ipc.h>
#include <reef/mailbox.h>
#include <reef/dai.h>
#include <reef/dma.h>
#include <reef/reef.h>
#include <reef/work.h>
#include <reef/clock.h>
#include <reef/audio/component.h>
#include <string.h>

static const struct sst_intel_ipc_fw_ready ready = {
	/* for host, we need exchange the naming of inxxx and outxxx */
	.inbox_offset = MAILBOX_HOST_OFFSET + MAILBOX_OUTBOX_OFFSET,
	.outbox_offset = MAILBOX_HOST_OFFSET + MAILBOX_INBOX_OFFSET,
	.inbox_size = MAILBOX_OUTBOX_SIZE,
	.outbox_size = MAILBOX_INBOX_SIZE,
	.fw_info_size = sizeof(struct fw_info),
	.info = {
		.name = "REEF",
		.date = __DATE__,
		.time = __TIME__,
	},
};

static const struct work_queue_timesource platform_generic_queue = {
	.timer 		= TIMER3,	/* external timer */
	.clk		= CLK_SSP2,
	.timer_set	= platform_timer_set,
	.timer_clear	= platform_timer_clear,
	.timer_get	= platform_timer_get,
};

int platform_boot_complete(uint32_t boot_message)
{
	uint64_t outbox = MAILBOX_HOST_OFFSET >> 3;

	mailbox_outbox_write(0, &ready, sizeof(ready));

	/* now interrupt host to tell it we are done booting */
	shim_write(SHIM_IPCDL, IPC_INTEL_FW_READY | outbox);
	shim_write(SHIM_IPCDH, SHIM_IPCDH_BUSY);

	return 0;
}

/* clear mask in PISR, bits are W1C in docs but some bits need preserved ?? */
void platform_interrupt_mask_clear(uint32_t mask)
{
	shim_write(SHIM_PISR, mask);
}

int platform_init(void)
{
	struct dma *dmac0, *dmac1;
	struct dai *ssp0, *ssp1, *ssp2;

	/* clear mailbox */
	bzero((void*)MAILBOX_BASE, IPC_MAX_MAILBOX_BYTES);

	/* init PMC IPC */
	platform_ipc_pmc_init();

	/* init work queues and clocks */
	platform_timer_set(TIMER3, 0xffffffff);
	init_platform_clocks();
	init_system_workq(&platform_generic_queue);

	/* Set CPU to default frequency for booting */
	clock_set_freq(CLK_CPU, 343000000);
	clock_set_freq(CLK_SSP0, 25000000);
	clock_set_freq(CLK_SSP1, 25000000);
	clock_set_freq(CLK_SSP2, 25000000);

	dmac0 = dma_get(DMA_ID_DMAC0);
	dma_probe(dmac0);

	dmac1 = dma_get(DMA_ID_DMAC1);
	dma_probe(dmac1);

	ssp0 = dai_get(COMP_TYPE_DAI_SSP, 0);
	dai_probe(ssp0);

	ssp1 = dai_get(COMP_TYPE_DAI_SSP, 1);
	dai_probe(ssp1);

	ssp2 = dai_get(COMP_TYPE_DAI_SSP, 2);
	dai_probe(ssp2);

	/* set SSP defaults */
	shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) | 0x00000038);
	shim_write(SHIM_MISC, shim_read(SHIM_MISC) | 0x0000000e);

	return 0;
}
