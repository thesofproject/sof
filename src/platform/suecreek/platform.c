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
 */

#include <platform/memory.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <platform/dma.h>
#include <platform/clk.h>
#include <platform/timer.h>
#include <platform/pmc.h>
#include <platform/interrupt.h>
#include <uapi/ipc.h>
#include <reef/mailbox.h>
#include <reef/dai.h>
#include <reef/dma.h>
#include <reef/reef.h>
#include <reef/work.h>
#include <reef/clock.h>
#include <reef/ipc.h>
#include <reef/trace.h>
#include <reef/io.h>
#include <reef/spi.h>
#include <reef/audio/component.h>
#include <string.h>
#include <version.h>

static const struct sof_ipc_fw_ready ready = {
	.hdr = {
		.cmd = SOF_IPC_FW_READY,
		.size = sizeof(struct sof_ipc_fw_ready),
	},

	/* for host, we need exchange the naming of inxxx and outxxx */
	.hostbox_offset = 0,
	.dspbox_offset = 0,
	.hostbox_size = 0x1000,
	.dspbox_size = 0x1000,
	.version = {
		.build = REEF_BUILD,
		.minor = REEF_MINOR,
		.major = REEF_MAJOR,
		.date = __DATE__,
		.time = __TIME__,
		.tag = REEF_TAG,
	},
	/* TODO: add capabilities */
};

static struct work_queue_timesource platform_generic_queue = {
	.timer	 = {
		.id = TIMER3,	/* external timer */
		.irq = IRQ_BIT_LVL2_DWCT0,
	},
	.clk		= CLK_SSP,
	.notifier	= NOTIFIER_ID_SSP_FREQ,
	.timer_set	= platform_timer_set,
	.timer_clear	= platform_timer_clear,
	.timer_get	= platform_timer_get,
};

struct timer *platform_timer = &platform_generic_queue.timer;

int platform_boot_complete(uint32_t boot_message)
{
	//uint64_t outbox = MAILBOX_HOST_OFFSET >> 3;

	/* TODO: must be sent over SPI */
	//mailbox_outbox_write(0, &ready, sizeof(ready));
//#define SUE_HOST_IRQ_CTL		SUE_GLOB_CTL_BASEADDR(0x50)
//#define SUE_HOST_WAKE_CTL		SUE_GLOB_CTL_BASEADDR(0x54)

//	io_reg_write(SUE_HOST_IRQ_CTL, 0);
//	io_reg_write(SUE_HOST_IRQ_CTL, 1);
//	io_reg_write(SUE_HOST_IRQ_CTL, 0);
	
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
	{25000000, 32, 48000, 1536, 25000},	/* 1.536MHz */
	{25000000, 64, 48000, 3072, 25000},	/* 3.072MHz */
	{25000000, 400, 48000, 96, 125},	/* 19.2MHz */
	{25000000, 400, 44100, 441, 625},	/* 17.64MHz */
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

#define SUE_DW_ICTL_BASE_ADDR			0x00081800
#define SUE_DW_ICTL_IRQ_INTEN_L			(0x00 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_INTEN_H			(0x04 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_INTMASK_L		(0x08 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_INTMASK_H		(0x0C + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_INTFORCE_L		(0x10 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_INTFORCE_H		(0x14 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_RAWSTATUS_L		(0x18 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_RAWSTATUS_H		(0x1C + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_STATUS_L		(0x20 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_STATUS_H		(0x24 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_MASKSTATUS_L	(0x28 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_MASKSTATUS_H	(0x2C + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_FINALSTATUS_L	(0x30 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_FINALSTATUS_H	(0x34 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR			(0x38 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_0		(0x40 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_1		(0x48 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_2		(0x50 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_3		(0x58 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_4		(0x60 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_5		(0x68 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_6		(0x70 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_7		(0x78 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_8		(0x80 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_9		(0x88 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_10		(0x90 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_11		(0x98 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_12		(0xA0 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_13		(0xA8 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_14		(0xB0 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_VECTOR_15		(0xB8 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_FIQ_INTEN		(0xC0 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_FIQ_INTMASK		(0xC4 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_FIQ_INTFORCE	(0xC8 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_FIQ_RAWSTATUS	(0xCC + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_FIQ_STATUS		(0xD0 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_FIQ_FINALSTATUS	(0xD4 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_IRQ_FIQ_PLEVEL		(0xD8 + SUE_DW_ICTL_BASE_ADDR)
#define SUE_DW_ICTL_PR_N(x)				(0xE8 + x*4 + SUE_DW_ICTL_BASE_ADDR)


static struct timer platform_ext_timer = {
	.id = TIMER3,
	.irq = IRQ_BIT_LVL2_DWCT0,
};


#define SUE_DMA0_OWNSHIP_REG	(0x00071A60)
#define SUE_DMA1_OWNSHIP_REG	(0x00071A62)
#define SUE_DMA2_OWNSHIP_REG	(0x00071A64)

static inline void dma_ownership_enable(void)
{
	// TODO: why are these 16 bit ?
	//io_reg_write16(SUE_DMA0_OWNSHIP_REG, 0x80FF);
	//io_reg_write16(SUE_DMA1_OWNSHIP_REG, 0x80FF);
	//io_reg_write16(SUE_DMA2_OWNSHIP_REG, 0x80FF);
}

#define SUE_SYS_CFG_REG1		(0x00071A68)
#define SUE_SYS_CFG_REG2		(0x00071A6C)
#define SUE_SYS_CFG_REG3		(0x00071A70)


int platform_init(struct reef *reef)
{
	struct dma *dmac0, *dmac1;
	struct dai *ssp0, *ssp1, *ssp2;
	struct sspi *spi;
	
    /* Configure as HOST_IRQ GPIO for IPC communication */
  // TODO  gpio_config(GPIO14, SUE_GPIO_DIR_OUT);

//	io_reg_write(SUE_SYS_CFG_REG2, 0x00000006);
//	io_reg_write(SUE_SYS_CFG_REG1, 0x0010071D);
//	io_reg_write(SUE_SYS_CFG_REG3, 0x00000007);

	
	trace_point(TRACE_BOOT_PLATFORM_MBOX);
// TODO mask all xtensa dn SHIM IRQs
	/* clear mailbox for early trace and debug */
//	bzero((void*)MAILBOX_BASE, IPC_MAX_MAILBOX_BYTES);

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
	ipc_init(reef);

	/* enable DMA0, DMA1, DMA2 ownership */
	dma_ownership_enable();

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


	/* initialize the SPI slave */
	spi = sspi_get(SOF_SPI_INTEL_SLAVE);
	if (spi == NULL)
		return -ENODEV;
	sspi_probe(spi);

	/* mask SSP interrupts */
	//shim_write(SHIM_PIMR, shim_read(SHIM_PIMR) | 0x00000038);

	/* init SSP ports */
	trace_point(TRACE_BOOT_PLATFORM_SSP);
	ssp0 = dai_get(SOF_DAI_INTEL_SSP, 0);
	if (ssp0 == NULL)
		return -ENODEV;
	dai_probe(ssp0);

	ssp1 = dai_get(SOF_DAI_INTEL_SSP, 1);
	if (ssp1 == NULL)
		return -ENODEV;
	dai_probe(ssp1);

	ssp2 = dai_get(SOF_DAI_INTEL_SSP, 2);
	if (ssp2 == NULL)
		return -ENODEV;
	dai_probe(ssp2);
	return 0;
}
