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
#include <reef/ipc.h>
#include <reef/trace.h>
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

static struct work_queue_timesource platform_generic_queue = {
	.timer 	 = {
		.id = TIMER3,	/* external timer */
		.irq = IRQ_BIT_LVL2_WALL_CLK0,
	},
	.clk		= CLK_SSP,
	.notifier	= NOTIFIER_ID_SSP_FREQ,
	.timer_set	= platform_timer_set,
	.timer_clear	= platform_timer_clear,
	.timer_get	= platform_timer_get,
};

int platform_boot_complete(uint32_t boot_message)
{
	//uint64_t outbox = MAILBOX_HOST_OFFSET >> 3;

	mailbox_outbox_write(0, &ready, sizeof(ready));

	/* boot now complete so we can relax the CPU */
	clock_set_freq(CLK_CPU, CLK_DEFAULT_CPU_HZ);
#if 0
	/* now interrupt host to tell it we are done booting */
	shim_write(SHIM_IPCDL, IPC_INTEL_FW_READY | outbox);
	shim_write(SHIM_IPCDH, SHIM_IPCDH_BUSY);
#endif
	return 0;
}

struct ssp_mn {
	uint32_t source;
	uint32_t bclk_fs;
	uint32_t rate;
	uint32_t m;
	uint32_t n;
};

#if 0
static const struct ssp_mn ssp_mn_conf[] = {
	{25000000, 24, 48000, 1152, 25000},     /* 1.152MHz */
	{25000000, 32, 48000, 1536, 25000}, 	/* 1.536MHz */
	{25000000, 64, 48000, 3072, 25000}, 	/* 3.072MHz */
	{25000000, 400, 48000, 96, 125}, 	/* 19.2MHz */
	{25000000, 400, 44100, 441, 625}, 	/* 17.64MHz */
};
#endif

/* set the SSP M/N clock dividers */
int platform_ssp_set_mn(uint32_t ssp_port, uint32_t source, uint32_t rate,
	uint32_t bclk_fs)
{
#if 0
	int i;

	/* check for matching config in the table */
	for (i = 0; i < ARRAY_SIZE(ssp_mn_conf); i++) {

		if (ssp_mn_conf[i].source != source)
			continue;

		if (ssp_mn_conf[i].rate != rate)
			continue;

		if (ssp_mn_conf[i].bclk_fs != bclk_fs)
			continue;

		/* match */
		switch (ssp_port) {
		case 0:
			shim_write(SHIM_SSP0_DIVL, ssp_mn_conf[i].n);
			shim_write(SHIM_SSP0_DIVH, SHIM_SSP_DIV_ENA |
				SHIM_SSP_DIV_UPD | ssp_mn_conf[i].m);
			break;
		case 1:
			shim_write(SHIM_SSP1_DIVL, ssp_mn_conf[i].n);
			shim_write(SHIM_SSP1_DIVH, SHIM_SSP_DIV_ENA |
				SHIM_SSP_DIV_UPD | ssp_mn_conf[i].m);
			break;
		case 2:
			shim_write(SHIM_SSP2_DIVL, ssp_mn_conf[i].n);
			shim_write(SHIM_SSP2_DIVH, SHIM_SSP_DIV_ENA |
				SHIM_SSP_DIV_UPD | ssp_mn_conf[i].m);
			break;
		default:
			return -ENODEV;
		}

		return 0;
	}
#endif
	return -EINVAL;
}

void platform_ssp_disable_mn(uint32_t ssp_port)
{
#if 0
	switch (ssp_port) {
	case 0:
		shim_write(SHIM_SSP0_DIVH, SHIM_SSP_DIV_BYP |
			SHIM_SSP_DIV_UPD);
		break;
	case 1:
		shim_write(SHIM_SSP1_DIVH, SHIM_SSP_DIV_BYP |
			SHIM_SSP_DIV_UPD);
		break;
	case 2:
		shim_write(SHIM_SSP2_DIVH, SHIM_SSP_DIV_BYP |
			SHIM_SSP_DIV_UPD);
		break;
	}
#endif
}

/* clear mask in PISR, bits are W1C in docs but some bits need preserved ?? */
void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	/* get the external IRQ */
	switch (REEF_IRQ_NUMBER(irq)) {
	case IRQ_NUM_EXT_LEVEL5:
		if (irq_read(REG_IRQ_IL5RSD) == 0)
			interrupt_clear(REEF_IRQ_NUMBER(irq));
		break;
	case IRQ_NUM_EXT_LEVEL4:
		if (irq_read(REG_IRQ_IL4RSD) == 0)
			interrupt_clear(REEF_IRQ_NUMBER(irq));
		break;
	case IRQ_NUM_EXT_LEVEL3:
		if (irq_read(REG_IRQ_IL3RSD) == 0)
			interrupt_clear(REEF_IRQ_NUMBER(irq));
		break;
	case IRQ_NUM_EXT_LEVEL2:
		if (irq_read(REG_IRQ_IL2RSD) == 0)
			interrupt_clear(REEF_IRQ_NUMBER(irq));
		break;
	default:
		break;
	}
}

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;//shim_read(SHIM_PIMR);
}

void platform_interrupt_mask(uint32_t irq, uint32_t mask)
{

}

void platform_interrupt_unmask(uint32_t irq, uint32_t mask)
{

}

static struct timer platform_ext_timer = {
	.id = TIMER3,
	.irq = IRQ_BIT_LVL2_WALL_CLK0,
};

int platform_init(void)
{
	struct dma *dmac0, *dmac1;
	struct dai *ssp0, *ssp1, *ssp2;

	trace_point(TRACE_BOOT_PLATFORM_MBOX);
// TODO mask all xtensa dn SHIM IRQs
	/* clear mailbox for early trace and debug */
	bzero((void*)MAILBOX_BASE, IPC_MAX_MAILBOX_BYTES);

	trace_point(TRACE_BOOT_PLATFORM_SHIM);

	/* configure the shim */
	//shim_write(SHIM_MISC, shim_read(SHIM_MISC) | 0x0000000e);

	trace_point(TRACE_BOOT_PLATFORM_PMC);

	/* init PMC IPC */
	//platform_ipc_pmc_init();

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

	/* set SSP clock to 25M */
	trace_point(TRACE_BOOT_PLATFORM_SSP_FREQ);
	clock_set_freq(CLK_SSP, 25000000);

	/* initialise the host IPC mechanisms */
	trace_point(TRACE_BOOT_PLATFORM_IPC);
	ipc_init();

	/* init DMACs */
	trace_point(TRACE_BOOT_PLATFORM_DMA);
	dmac0 = dma_get(DMA_ID_DMAC0);
	dma_probe(dmac0);

	dmac1 = dma_get(DMA_ID_DMAC1);
	dma_probe(dmac1);

	/* mask SSP interrupts */
	//shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) | 0x00000038);

	/* init SSP ports */
	trace_point(TRACE_BOOT_PLATFORM_SSP);
	ssp0 = dai_get(COMP_TYPE_DAI_SSP, 0);
	dai_probe(ssp0);

	ssp1 = dai_get(COMP_TYPE_DAI_SSP, 1);
	dai_probe(ssp1);

	ssp2 = dai_get(COMP_TYPE_DAI_SSP, 2);
	dai_probe(ssp2);
	return 0;
}
