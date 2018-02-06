/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
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
#include <reef/ipc.h>
#include <reef/trace.h>
#include <reef/audio/component.h>
#include <config.h>
#include <string.h>

static const struct sst_intel_ipc_fw_ready ready = {
	/* for host, we need exchange the naming of inxxx and outxxx */
	.inbox_offset = MAILBOX_HOST_OFFSET + MAILBOX_OUTBOX_OFFSET,
	.outbox_offset = MAILBOX_HOST_OFFSET + MAILBOX_INBOX_OFFSET,
	.inbox_size = MAILBOX_OUTBOX_SIZE,
	.outbox_size = MAILBOX_INBOX_SIZE,
	.fw_info_size = sizeof(struct fw_info),
	{
		.rsvd = PACKAGE_STRING,
	},
};

static struct work_queue_timesource platform_generic_queue = {
	.timer	 = {
		.id = TIMER3,	/* external timer */
		.irq = IRQ_NUM_EXT_TIMER,
	},
	.clk		= CLK_SSP,
	.notifier	= NOTIFIER_ID_SSP_FREQ,
	.timer_set	= platform_timer_set,
	.timer_clear	= platform_timer_clear,
	.timer_get	= platform_timer_get,
};

int platform_boot_complete(uint32_t boot_message)
{
	uint64_t outbox = MAILBOX_HOST_OFFSET >> 3;

	mailbox_outbox_write(0, &ready, sizeof(ready));

	/* boot now complete so we can relax the CPU */
	clock_set_freq(CLK_CPU, CLK_DEFAULT_CPU_HZ);

	/* now interrupt host to tell it we are done booting */
	shim_write(SHIM_IPCDL, IPC_INTEL_FW_READY | outbox);
	shim_write(SHIM_IPCDH, SHIM_IPCDH_BUSY);

	return 0;
}

/* clear mask in PISR, bits are W1C in docs but some bits need preserved ?? */
void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	switch (irq) {
	case IRQ_NUM_EXT_DMAC0:
		shim_write(SHIM_PISR, mask << 16);
		interrupt_clear(irq);
		break;
	case IRQ_NUM_EXT_DMAC1:
		shim_write(SHIM_PISR, mask << 24);
		interrupt_clear(irq);
		break;
#if defined CONFIG_CHERRYTRAIL
	case IRQ_NUM_EXT_DMAC2:
		shim_write(SHIM_PISRH, mask << 0);
		interrupt_clear(irq);
		break;
#endif
	default:
		break;
	}
}

/* TODO: expand this to 64 bit - should we just return mask of IRQ numbers */
uint32_t platform_interrupt_get_enabled(void)
{
	return shim_read(SHIM_PIMR);
}

void platform_interrupt_mask(uint32_t irq, uint32_t mask)
{

}

void platform_interrupt_unmask(uint32_t irq, uint32_t mask)
{

}

static struct timer platform_ext_timer = {
	.id = TIMER3,
	.irq = IRQ_NUM_EXT_TIMER,
};

int platform_init(void)
{
#if defined CONFIG_BAYTRAIL
	struct dma *dmac0, *dmac1;
	struct dai *ssp0, *ssp1, *ssp2;
#elif defined CONFIG_CHERRYTRAIL
	struct dma *dmac0, *dmac1, *dmac2;
	struct dai *ssp0, *ssp1, *ssp2, *ssp3, *ssp4, *ssp5;
#else
#error Undefined platform
#endif

	trace_point(TRACE_BOOT_PLATFORM_MBOX);

	/* clear mailbox for early trace and debug */
	bzero((void*)MAILBOX_BASE, IPC_MAX_MAILBOX_BYTES);

	trace_point(TRACE_BOOT_PLATFORM_SHIM);

	/* configure the shim */
#if defined CONFIG_BAYTRAIL
	shim_write(SHIM_MISC, shim_read(SHIM_MISC) | 0x0000000e);
#elif defined CONFIG_CHERRYTRAIL
	shim_write(SHIM_MISC, shim_read(SHIM_MISC) | 0x00000e0e);
#endif

	trace_point(TRACE_BOOT_PLATFORM_PMC);

	/* init PMC IPC */
	platform_ipc_pmc_init();

	/* init work queues and clocks */
	trace_point(TRACE_BOOT_PLATFORM_TIMER);
	platform_timer_start(&platform_ext_timer);

	trace_point(TRACE_BOOT_PLATFORM_CLOCK);
	init_platform_clocks();

	trace_point(TRACE_BOOT_SYS_WORK);
	init_system_workq(&platform_generic_queue);

	/* Set CPU to default frequency for booting */
	trace_point(TRACE_BOOT_SYS_CPU_FREQ);
	clock_set_freq(CLK_CPU, CLK_MAX_CPU_HZ);

	trace_point(TRACE_BOOT_PLATFORM_SSP_FREQ);

	/* set SSP clock to 19.2M */
	clock_set_freq(CLK_SSP, 19200000);

	/* initialise the host IPC mechanisms */
	trace_point(TRACE_BOOT_PLATFORM_IPC);
	ipc_init();

	/* init DMACs */
	trace_point(TRACE_BOOT_PLATFORM_DMA);
	dmac0 = dma_get(DMA_ID_DMAC0);
	if (dmac0 == NULL)
		return -ENODEV;
	dma_probe(dmac0);

	dmac1 = dma_get(DMA_ID_DMAC1);
	if (dmac1 == NULL)
		return -ENODEV;
	dma_probe(dmac1);

#if defined CONFIG_CHERRYTRAIL
	dmac2 = dma_get(DMA_ID_DMAC1);
	if (dmac2 == NULL)
		return -ENODEV;
	dma_probe(dmac2);
#endif

	/* mask SSP interrupts */
	shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) | 0x00000038);

	/* init SSP ports */
	trace_point(TRACE_BOOT_PLATFORM_SSP);
	ssp0 = dai_get(COMP_TYPE_DAI_SSP, 0);
	if (ssp0 == NULL)
		return -ENODEV;
	dai_probe(ssp0);

	ssp1 = dai_get(COMP_TYPE_DAI_SSP, 1);
	if (ssp1 == NULL)
		return -ENODEV;
	dai_probe(ssp1);

	ssp2 = dai_get(COMP_TYPE_DAI_SSP, 2);
	if (ssp2 == NULL)
		return -ENODEV;
	dai_probe(ssp2);

#if defined CONFIG_CHERRYTRAIL
	ssp3 = dai_get(COMP_TYPE_DAI_SSP, 3);
	if (ssp3 == NULL)
		return -ENODEV;
	dai_probe(ssp3);

	ssp4 = dai_get(COMP_TYPE_DAI_SSP, 4);
	if (ssp4 == NULL)
		return -ENODEV;
	dai_probe(ssp4);

	ssp5 = dai_get(COMP_TYPE_DAI_SSP, 5);
	if (ssp5 == NULL)
		return -ENODEV;
	dai_probe(ssp5);
#endif

	return 0;
}
