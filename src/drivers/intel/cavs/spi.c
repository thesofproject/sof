/*
 * Copyright (c) 2017, Intel Corporation
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
 * Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>
 */

#include <errno.h>
#include <stdint.h>

#include <sof/clk.h>
#include <sof/dma.h>
#include <sof/io.h>
#include <sof/ipc.h>
#include <sof/schedule.h>
#include <sof/sof.h>
#include <sof/spi.h>
#include <sof/work.h>

#include <platform/dma.h>
#include <platform/memory.h>
#include <platform/sue-gpio.h>

#define SUE_SPI_BASEADDRESS(x)		((x) + DW_SPI_SLAVE_BASE)
#define	SUE_SPI_REG_CTRLR0		SUE_SPI_BASEADDRESS(0x00)
#define	SUE_SPI_REG_CTRLR1		SUE_SPI_BASEADDRESS(0x04)
#define	SUE_SPI_REG_SSIENR		SUE_SPI_BASEADDRESS(0x08)
#define	SUE_SPI_REG_MWCR		SUE_SPI_BASEADDRESS(0x0C)
#define	SUE_SPI_REG_SER			SUE_SPI_BASEADDRESS(0x10)
#define	SUE_SPI_REG_BAUDR		SUE_SPI_BASEADDRESS(0x14)
#define	SUE_SPI_REG_TXFTLR		SUE_SPI_BASEADDRESS(0x18)
#define	SUE_SPI_REG_RXFTLR		SUE_SPI_BASEADDRESS(0x1C)
#define	SUE_SPI_REG_TXFLR		SUE_SPI_BASEADDRESS(0x20)
#define	SUE_SPI_REG_RXFLR		SUE_SPI_BASEADDRESS(0x24)
#define	SUE_SPI_REG_SR			SUE_SPI_BASEADDRESS(0x28)
#define	SUE_SPI_REG_IMR			SUE_SPI_BASEADDRESS(0x2C)
#define	SUE_SPI_REG_ISR			SUE_SPI_BASEADDRESS(0x30)
#define	SUE_SPI_REG_RISR		SUE_SPI_BASEADDRESS(0x34)
#define	SUE_SPI_REG_TXOICR		SUE_SPI_BASEADDRESS(0x38)
#define	SUE_SPI_REG_RXOICR		SUE_SPI_BASEADDRESS(0x3C)
#define	SUE_SPI_REG_RXUICR		SUE_SPI_BASEADDRESS(0x40)
#define	SUE_SPI_REG_ICR			SUE_SPI_BASEADDRESS(0x48)
#define	SUE_SPI_REG_DMACR		SUE_SPI_BASEADDRESS(0x4C)
#define	SUE_SPI_REG_DMATDLR		SUE_SPI_BASEADDRESS(0x50)
#define	SUE_SPI_REG_DMARDLR		SUE_SPI_BASEADDRESS(0x54)
#define	SUE_SPI_REG_DR			SUE_SPI_BASEADDRESS(0x60)
#define	SUE_SPI_REG_SPICTRLR0		SUE_SPI_BASEADDRESS(0xF4)

#define SPI_BUFFER_SIZE			256

#define DMA_HANDSHAKE_SSPI_TX		26
#define DMA_HANDSHAKE_SSPI_RX		27

#define SSI_SLAVE_CLOCK_CTL		(EXT_CTRL_BASE + 0x60)

/* CTRLR0 */
/* 00-standard spi; 01-dual spi; 10-quad spi */
#define SPI_FRAME_FORMAT(x)		(x << 21)
#define SPI_DATA_FRAME_SIZE(x)		(x << 16)
/* 0-slave tx enabled; 1-slave tx disabled */
#define SPI_SLV_OE(x)			(x << 10)
/* 00-both; 01-transmit only;
 * 10-receive only; 11-eeprom read
 */
#define SPI_TRANSFER_MODE(x)		(x << 8)
/* 0-inactive low; 1-inactive high */
#define SPI_SCPOL(x)			(x << 7)
/* 0-first edge capture; 1-one cycle after cs line */
#define SPI_SCPH(x)			(x << 6)
/* 00-moto spi; 01-ti ssp; 10-ns microwire */
#define SPI_FRAME_TYPE(x)		(x << 4)

/* SSIENR */
#define SPI_SSIEN			1

/* DMACR */
/* 0-transmit DMA disable; 1-transmit DMA enable */
#define	SPI_DMACR_TDMAE(x)		(x << 1)
/* 0-receive DMA disable; 1-receive DMA enable */
#define SPI_DMACR_RDMAE(x)		(x << 0)
/* DMATDLR/DMARDLR */
/* transmit data level: 0~255 */
#define SPI_DMATDLR(x)			(x << 0)
/* receive data level: 0~255 */
#define SPI_DMARDLR(x)			(x << 0)

enum sspi_type {
	SPI_DIR_RX,
	SPI_DIR_TX,
};

enum {
	SSPI_TRIGGER_START,
	SSPI_TRIGGER_STOP,
};

/* SPI-Slave ISR's state machine: from the PoV of the DSP */
enum IPC_STATUS {
	IPC_READ,
	IPC_WRITE,
};

struct spi_dma_config {
	enum sspi_type type;
	void *src_buf;
	void *dest_buf;
	uint32_t transfer_len;
	uint32_t cyclic;
};

struct sspi_plat_fifo_data {
	uint32_t offset;
	uint32_t handshake;
};

struct sspi_plat_data {
	uint32_t base;
	uint32_t irq;
	struct sspi_plat_fifo_data fifo[2];
};

struct spi_reg_list {
	uint32_t ctrlr0;
	uint32_t ctrlr1;
	uint32_t dmacr;		/* dma control register */
};

struct sspi {
	uint32_t type;
	uint32_t index;
	uint32_t chan[2];	/* spi-slave rx/tx dma */
	uint32_t rx_size;
	uint32_t tx_size;
	uint8_t *rx_buffer;
	uint8_t *tx_buffer;
	struct dma *dma[2];
	struct spi_reg_list reg;
	struct sspi_plat_data plat_data;
	struct spi_dma_config config[2];
	uint32_t ipc_status;
	struct task completion;

	struct sof_ipc_hdr hdr;
};

#define spi_fifo_offset(spi, direction) \
			spi->plat_data.fifo[direction].offset
#define spi_fifo_handshake(spi, direction) \
			spi->plat_data.fifo[direction].handshake

extern struct ipc *_ipc;

static void spi_start(struct sspi *spi, int direction)
{
	/* disable SPI first before config */
	io_reg_write(SUE_SPI_REG_SSIENR, 0);

	io_reg_write(SUE_SPI_REG_CTRLR0, spi->reg.ctrlr0);
	io_reg_write(SUE_SPI_REG_IMR, 0);

	/* Trigger interrupt at or above 1 entry in the RX FIFO */
	io_reg_write(SUE_SPI_REG_RXFTLR, 1);
	/* Trigger DMA at or above 1 entry in the RX FIFO */
	io_reg_write(SUE_SPI_REG_DMARDLR, SPI_DMARDLR(1));

	/* Trigger interrupt at or below 1 entry in the TX FIFO */
	io_reg_write(SUE_SPI_REG_TXFTLR, 1);
	/* Trigger DMA at or below 1 entry in the TX FIFO */
	io_reg_write(SUE_SPI_REG_DMATDLR, SPI_DMATDLR(1));

	io_reg_write(SUE_SPI_REG_DMACR, spi->reg.dmacr);
	io_reg_write(SUE_SPI_REG_SSIENR, SPI_SSIEN);
}

static void spi_stop(struct sspi *spi)
{
	io_reg_write(SUE_SPI_REG_DMACR,
		     SPI_DMACR_TDMAE(0) | SPI_DMACR_RDMAE(0));
	io_reg_write(SUE_SPI_REG_SSIENR, 0);
}

static int sspi_trigger(struct sspi *spi, int cmd, int direction)
{
	int ret;

	switch (cmd) {
	case SSPI_TRIGGER_START:
		/* trigger the SPI-Slave + DMA + INT + Receiving */
		ret = dma_start(spi->dma[direction], spi->chan[direction]);
		if (ret < 0)
			return ret;

		spi_start(spi, direction);

		break;
	case SSPI_TRIGGER_STOP:
		/* Stop the SPI-Slave */
		spi_stop(spi);
		dma_stop(spi->dma[direction], spi->chan[direction]);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* Only enable one direction at a time: Rx or Tx */
static inline void spi_config(struct sspi *spi, struct spi_dma_config *spi_cfg)
{
	switch (spi_cfg->type) {
	case SPI_DIR_RX:
		spi->reg.ctrlr0 = SPI_FRAME_FORMAT(0)
						| SPI_DATA_FRAME_SIZE(0x1f)
						| SPI_TRANSFER_MODE(0x2)
						| SPI_SCPOL(1)
						| SPI_SLV_OE(1)
						| SPI_SCPH(1)
						| SPI_FRAME_TYPE(0);
		spi->reg.dmacr	 = SPI_DMACR_RDMAE(1);
		break;
	case SPI_DIR_TX:
		spi->reg.ctrlr0 = SPI_FRAME_FORMAT(0)
						| SPI_DATA_FRAME_SIZE(0x1f)
						| SPI_TRANSFER_MODE(0x1)
						| SPI_SCPOL(1)
						| SPI_SLV_OE(0)
						| SPI_SCPH(1)
						| SPI_FRAME_TYPE(0);
		spi->reg.dmacr	 = SPI_DMACR_TDMAE(1);
		break;
	default:
		break;
	}
}

static int spi_slave_dma_set_config(struct sspi *spi,
				    struct spi_dma_config *spi_cfg)
{
	struct dma_sg_config config = {
		.src_width  = 4,
		.dest_width = 4,
	};
	struct dma_sg_elem local_sg_elem;
	struct dma *dma = spi->dma[spi_cfg->type];
	uint32_t chan = spi->chan[spi_cfg->type];

	/* dma config */
	config.cyclic     = spi_cfg->cyclic;

	local_sg_elem.size = spi_cfg->transfer_len;

	/*
	 * Source and destination width is 32 bits, contrary to dw_apb_ssi note
	 * on page 87
	 */
	switch (spi_cfg->type) {
	case SPI_DIR_RX:	/* HOST -> DSP */
		config.direction  = DMA_DIR_DEV_TO_MEM;
		config.src_dev    = spi_fifo_handshake(spi, spi_cfg->type);

		local_sg_elem.dest = (uint32_t)spi_cfg->dest_buf;
		local_sg_elem.src  = (uint32_t)spi_fifo_offset(spi,
							       spi_cfg->type);
		break;
	case SPI_DIR_TX:	/* DSP -> HOST */
		config.direction  = DMA_DIR_MEM_TO_DEV;
		config.dest_dev   = spi_fifo_handshake(spi, spi_cfg->type);

		local_sg_elem.dest = (uint32_t)spi_fifo_offset(spi,
							       spi_cfg->type);
		local_sg_elem.src  = (uint32_t)spi_cfg->src_buf;
		break;
	default:
		return -EINVAL;
	}

	/* configure local DMA elem */
	config.elem_array.count = 1;
	config.elem_array.elems = &local_sg_elem;

	return dma_set_config(dma, chan, &config);
}

static int sspi_set_config(struct sspi *spi, struct spi_dma_config *spi_cfg)
{
	/* spi slave config */
	spi_config(spi, spi_cfg);

	/* dma config */
	return spi_slave_dma_set_config(spi, spi_cfg);
}

static void delay(unsigned int ms)
{
	uint64_t tick = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, ms);

	wait_delay(tick);
}

static void spi_completion_work(void *data)
{
	struct sspi *spi = data;
	struct sof_ipc_hdr *hdr;
	struct spi_dma_config *spi_config;

	dcache_invalidate_region(spi->rx_buffer, SPI_BUFFER_SIZE);

	switch (spi->ipc_status) {
	case IPC_READ:
		hdr = (struct sof_ipc_hdr *)spi->rx_buffer;

		mailbox_hostbox_write(0, spi->rx_buffer, hdr->size);

		_ipc->host_pending = 1;
		ipc_schedule_process(_ipc);

		break;
	case IPC_WRITE:	/* DSP -> HOST */
		/*
		 * Data has been transferred to the SPI FIFO, but we don't know
		 * whether the master has read it all out yet. We might have to
		 * wait here before reconfiguring the SPI controller
		 */
		sspi_trigger(spi, SSPI_TRIGGER_STOP, SPI_DIR_TX);

		/* configure to receive next header */
		spi->ipc_status = IPC_READ;
		spi_config = &spi->config[SPI_DIR_RX];
		spi_config->dest_buf = spi->rx_buffer;
		spi_config->transfer_len = ALIGN(sizeof(*hdr), 16);
		sspi_set_config(spi, spi_config);
		sspi_trigger(spi, SSPI_TRIGGER_START, SPI_DIR_RX);

		break;
	}
}

static void spi_dma_complete(void *data, uint32_t type,
			     struct dma_sg_elem *next)
{
	struct sspi *spi = data;

	next->size = DMA_RELOAD_END;

	schedule_task(&spi->completion, 0, 100);
}

void sspi_push(struct sspi *spi, void *data, size_t size)
{
	struct spi_dma_config *config = spi->config + SPI_DIR_TX;

	if (size > SPI_BUFFER_SIZE) {
		trace_ipc_error("ePs");
		return;
	}

	sspi_trigger(spi, SSPI_TRIGGER_STOP, SPI_DIR_RX);

	/* configure transmit path of SPI-slave */
	bzero((void *)&spi->config[SPI_DIR_TX],
			sizeof(spi->config[SPI_DIR_TX]));
	config->type = SPI_DIR_TX;
	config->src_buf = spi->tx_buffer;
	config->dest_buf = NULL;
	config->transfer_len = ALIGN(size, 16);
	sspi_set_config(spi, config);

	spi->ipc_status = IPC_WRITE;

	/* Actually we have to send IPC messages in one go */
	rmemcpy(config->src_buf, data, size);
	dcache_writeback_region(config->src_buf, size);

	sspi_trigger(spi, SSPI_TRIGGER_START, SPI_DIR_TX);

	/*
	 * Tell the master to pull out the data, we aren't getting DMA
	 * completion until all the prepared data has been transferred
	 * to the SPI controller FIFO
	 */
	gpio_write(GPIO14, 1);
	delay(1);
	gpio_write(GPIO14, 0);
}

int sspi_slave_init(struct sspi *spi, uint32_t type)
{
	struct spi_dma_config *config = spi->config + SPI_DIR_RX;
	int ret;

	if (type != SOF_SPI_INTEL_SLAVE)
		return -EINVAL;

	/* GPIO14 to signal host IPC IRQ */
	gpio_config(GPIO14, SUE_GPIO_DIR_OUT);

	/* configure receive path of SPI-slave */
	bzero((void *)config, sizeof(*config));
	config->type = SPI_DIR_RX;
	config->src_buf = NULL/*spi->tx_buffer*/;
	config->dest_buf = spi->rx_buffer;
	config->transfer_len = ALIGN(sizeof(struct sof_ipc_hdr), 16);

	ret = sspi_set_config(spi, config);
	if (ret < 0)
		return ret;

	dcache_invalidate_region(spi->rx_buffer, SPI_BUFFER_SIZE);
	ret = sspi_trigger(spi, SSPI_TRIGGER_START, SPI_DIR_RX);
	if (ret < 0)
		return ret;

	schedule_task_init(&spi->completion, spi_completion_work, spi);
	schedule_task_config(&spi->completion, TASK_PRI_MED, 0);

	return 0;
}

int sspi_probe(struct sspi *spi)
{
	spi->dma[SPI_DIR_RX] = dma_get(DMA_DIR_DEV_TO_MEM, DMA_CAP_GP_LP,
				       DMA_DEV_SSI, DMA_ACCESS_SHARED);
	if (!spi->dma[SPI_DIR_RX])
		return -ENODEV;

	spi->dma[SPI_DIR_TX] = dma_get(DMA_DIR_MEM_TO_DEV, DMA_CAP_GP_LP,
				       DMA_DEV_SSI, DMA_ACCESS_SHARED);
	if (!spi->dma[SPI_DIR_TX])
		return -ENODEV;

	spi->chan[SPI_DIR_RX] = dma_channel_get(spi->dma[SPI_DIR_RX], 0);
	if (spi->chan[SPI_DIR_RX] < 0)
		return spi->chan[SPI_DIR_RX];

	spi->chan[SPI_DIR_TX] = dma_channel_get(spi->dma[SPI_DIR_TX], 0);
	if (spi->chan[SPI_DIR_TX] < 0)
		return spi->chan[SPI_DIR_TX];

	/* configure the spi clock */
	io_reg_write(SSI_SLAVE_CLOCK_CTL, 0x00000001);

	spi->rx_buffer = rzalloc(RZONE_SYS, SOF_MEM_CAPS_DMA, SPI_BUFFER_SIZE);
	if (spi->rx_buffer == NULL) {
		trace_ipc_error("eSp");
		return -ENOMEM;
	}

	spi->tx_buffer = rzalloc(RZONE_SYS, SOF_MEM_CAPS_DMA, SPI_BUFFER_SIZE);
	if (spi->tx_buffer == NULL) {
		rfree(spi->rx_buffer);
		trace_ipc_error("eSp");
		return -ENOMEM;
	}

	spi->ipc_status = IPC_READ;

	dma_set_cb(spi->dma[SPI_DIR_RX], spi->chan[SPI_DIR_RX],
		   DMA_IRQ_TYPE_BLOCK, spi_dma_complete, spi);
	dma_set_cb(spi->dma[SPI_DIR_TX], spi->chan[SPI_DIR_TX],
		   DMA_IRQ_TYPE_BLOCK, spi_dma_complete, spi);

	return 0;
}

static struct sspi spi_slave[1] = {
{
	.type  = SOF_SPI_INTEL_SLAVE,
	.index = 0,
	.plat_data = {
		.base		= DW_SPI_SLAVE_BASE,
		.irq		= IRQ_EXT_LP_GPDMA0_LVL5(0, 0),
		.fifo[SPI_DIR_RX] = {
			.offset		= SUE_SPI_REG_DR,
			.handshake	= DMA_HANDSHAKE_SSPI_RX,
		},
		.fifo[SPI_DIR_TX] = {
			.offset		= SUE_SPI_REG_DR,
			.handshake	= DMA_HANDSHAKE_SSPI_TX,
		}
	},
},
};

struct sspi *sspi_get(uint32_t type)
{
	if (type == SOF_SPI_INTEL_SLAVE)
		return &spi_slave[0];

	return NULL;
}
