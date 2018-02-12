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
 * Author: Zhigang Wu <zhigang.wu@intel.com>
 */

#include <stdint.h>
#include <errno.h>
#include <reef/ipc.h>
#include <uapi/ipc.h>
#include <reef/spi.h>
#include <reef/dma.h>
#include <reef/io.h>
#include <reef/clock.h>
#include <platform/dma.h>

extern struct ipc *_ipc;

#define SPI_SLAVE_BASE	0x00080000
#define DMA_HANDSHAKE_SSPI_TX		26
#define DMA_HANDSHAKE_SSPI_RX		27

#define SSI_SLAVE_CLOCK_CTL		0x00081C60


/* CTRLR0 */
/* 00-standard spi; 01-dual spi; 10-quad spi */
#define SPI_FRAME_FORMAT(x)		(x << 21)
#define SPI_DATA_FRAME_SIZE(x)	(x << 16)
/* 0-slave txt enabled; 1-slave txt disabled */
#define SPI_SLV_OE(x)			(x << 10)
/* 00-both; 01-transmit only;
 * 10-receive only; 11-eeprom read
 */
#define SPI_TRANSFER_MODE(x)	(x << 8)
/* 0-inactive low; 1-inactive high */
#define SPI_SCPOL(x)			(x << 7)
/* 0-first edge capture; 1-one cycle after cs line */
#define SPI_SCPH(x)				(x << 6)
/* 00-moto spi; 01-ti ssp; 10-ns microwire */
#define SPI_FRAME_TYPE(x)		(x << 4)

/* SSIENR */
#define SPI_SSIEN				1

/* IMR */
/* 0-masked; 1-unmasked;
 * receive FIFO full interrupt mask/unmask
 */
#define SPI_IMR_RXFIM(x)		(x << 4)
/* 0-masked; 1-unmasked;
 * receive FIFO overflow interrupt mask/unmask
 */
#define SPI_IMR_RXOIM(x)		(x << 3)
/* 0-masked; 1-unmasked;
 * transmit FIFO overflow interrupt mask/unmask
 */
#define SPI_IMR_TXOIM(x)		(x << 1)
/* 0-masked; 1-unmasked;
 * transmit FIFO empty interrupt mask/unmask
 */
#define SPI_IMR_TXEIM(x)		(x << 0)

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


/* SPI Master will get value from DSP */
#define	IPC_SPI_MASTER_READ		(1 << 27)
/* SPI Master will send value to DSP */
#define IPC_SPI_MASTER_WRITE	(0 << 27)


#define spi_irq(spi) \
			spi->plat_data.irq
#define spi_fifo_offset(spi, direction) \
			spi->plat_data.fifo[direction].offset
#define spi_fifo_handshake(spi, direction) \
			spi->plat_data.fifo[direction].handshake

/* SPI-Slave ISR's state machine */
enum IPC_STATUS {
	IPC_IDLE = 0,
	IPC_READ,
	IPC_WRITE,
};


static inline void spi_start(struct sspi *spi, int direction)
{
	/* disable SPI first before config */
	io_reg_write(SUE_SPI_REG_SSIENR, 0);

	io_reg_write(SUE_SPI_REG_CTRLR0, spi->reg.ctrlr0);
	io_reg_write(SUE_SPI_REG_IMR, spi->reg.imr);

	switch (direction) {
	case SPI_TYPE_INTEL_RECEIVE:
		io_reg_write(SUE_SPI_REG_RXFTLR, spi->reg.rxftlr);
		io_reg_write(SUE_SPI_REG_DMARDLR, spi->reg.dmardlr);
		break;
	case SPI_TYPE_INTEL_TRANSMIT:
		io_reg_write(SUE_SPI_REG_TXFTLR, spi->reg.txftlr);
		io_reg_write(SUE_SPI_REG_DMATDLR, spi->reg.dmatdlr);
		break;
	default:
		break;
	}
	io_reg_write(SUE_SPI_REG_DMACR, spi->reg.dmacr);
	io_reg_write(SUE_SPI_REG_SSIENR, 1);
}


static inline void spi_stop(struct sspi *spi)
{
	io_reg_write(SUE_SPI_REG_DMACR,
		SPI_DMACR_TDMAE(0) | SPI_DMACR_RDMAE(0));
	io_reg_write(SUE_SPI_REG_SSIENR, 0);
}

static inline void spi_config(struct sspi *spi,
						struct spi_dma_config *spi_cfg)
{
	switch (spi_cfg->type) {
	case SPI_TYPE_INTEL_RECEIVE:
		spi->reg.ctrlr0 = SPI_FRAME_FORMAT(0)
						| SPI_DATA_FRAME_SIZE(0x7)
						| SPI_TRANSFER_MODE(0x2)
						| SPI_SCPOL(1)
						| SPI_SCPH(1)
						| SPI_FRAME_TYPE(0);
		spi->reg.rxftlr  = 1;				/*4-byte FIFO*/
		spi->reg.imr	 = SPI_IMR_RXFIM(1);
		spi->reg.dmardlr = SPI_DMARDLR(0);	/*4-byte FIFO*/
		spi->reg.dmacr	 = SPI_DMACR_RDMAE(1);
		spi->reg.ssienr  = SPI_SSIEN;
		break;
	case SPI_TYPE_INTEL_TRANSMIT:
		spi->reg.ctrlr0 = SPI_FRAME_FORMAT(0)
						| SPI_DATA_FRAME_SIZE(0x7)
						| SPI_TRANSFER_MODE(0x1)
						| SPI_SCPOL(1)
						| SPI_SCPH(1)
						| SPI_FRAME_TYPE(0);
		spi->reg.txftlr  = 1;				/*4-byte FIFO*/
		spi->reg.imr	 = SPI_IMR_TXEIM(1);
		spi->reg.dmatdlr = SPI_DMATDLR(0);	/*4-byte FIFO*/
		spi->reg.dmacr	 = SPI_DMACR_TDMAE(1);
		spi->reg.ssienr  = SPI_SSIEN;
		break;
	default:
		break;
	}
}

static int spi_slave_dma_set_config(struct sspi *spi,
							struct spi_dma_config *spi_cfg)
{
	struct dma_sg_config config;
	struct dma_sg_elem local_sg_elem;
	struct dma *dma;
	uint32_t chan;

	dma = spi->dma;
	switch (spi_cfg->type) {
	case SPI_TYPE_INTEL_RECEIVE:	/* HOST -> DSP */
		/* dma config */
		chan = spi->chan[SPI_TYPE_INTEL_RECEIVE];
		config.direction  = DMA_DIR_DEV_TO_MEM;
		config.src_width  = spi_cfg->src_width;
		config.dest_width = spi_cfg->dest_width;
		config.src_msize  = spi_cfg->src_msize;
		config.dest_msize = spi_cfg->dest_msize;
		config.cyclic     = spi_cfg->cyclic;
		config.src_dev    =
			spi_fifo_handshake(spi, SPI_TYPE_INTEL_RECEIVE);

		local_sg_elem.dest = (uint32_t)spi_cfg->dest_buf;
		local_sg_elem.src  =
			(uint32_t)spi_fifo_offset(spi, SPI_TYPE_INTEL_RECEIVE);
		local_sg_elem.size = spi_cfg->transfer_len;
		break;
	case SPI_TYPE_INTEL_TRANSMIT:	/* DSP -> HOST */
		/* dma config */
		chan = spi->chan[SPI_TYPE_INTEL_TRANSMIT];
		config.direction  = DMA_DIR_MEM_TO_DEV;
		config.src_width  = spi_cfg->src_width;
		config.dest_width = spi_cfg->dest_width;
		config.src_msize  = spi_cfg->src_msize;
		config.dest_msize = spi_cfg->dest_msize;
		config.cyclic     = spi_cfg->cyclic;
		config.dest_dev   =
			spi_fifo_handshake(spi, SPI_TYPE_INTEL_TRANSMIT);

		local_sg_elem.src  = (uint32_t)spi_cfg->src_buf;
		local_sg_elem.dest =
			(uint32_t)spi_fifo_offset(spi, SPI_TYPE_INTEL_TRANSMIT);
		local_sg_elem.size = spi_cfg->transfer_len;
		break;
	default:
		return -EINVAL;
	}
	list_init(&config.elem_list);

	/* configure local DMA elem */
	list_item_prepend(&local_sg_elem.list, &config.elem_list);

	dma_set_config(dma, chan, &config);
	return 0;
}

#if 0 // TODO
static void spi_dma_complete(void *data,
				uint32_t type, struct dma_sg_elem *next)
{
	struct sspi *spi = (struct sspi *)data;
	struct sof_ipc_hdr *hdr;
	struct spi_dma_config *spi_config;

	dcache_invalidate_region(spi->rx_buffer, SPI_BUFFER_SIZE);
	dcache_invalidate_region(spi->tx_buffer, SPI_BUFFER_SIZE);

	switch (spi->ipc_status) {
	case IPC_IDLE:
		hdr = (struct sof_ipc_hdr *)spi->rx_buffer;
		trace_value(hdr->cmd);
		trace_value(hdr->size);
		if (hdr->cmd & IPC_SPI_MASTER_READ) { /* DSP -> HOST */
			/* stop receive channel */
			sspi_trigger(spi,
				SSPI_TRIGGER_STOP, SPI_TYPE_INTEL_RECEIVE);
			/* skip current DMA-channel0 */
			next->size = DMA_RELOAD_END;

			spi->ipc_status = IPC_READ;
			spi_config = &spi->config[SPI_TYPE_INTEL_TRANSMIT];
			spi_config->src_buf = (uint32_t)spi->tx_buffer;
			/* i need add 1 here,
			 * seems the slave always send number bytes sub 1
			*/
			spi_config->transfer_len = hdr->size+1;
			spi_config->cyclic = 1;
			sspi_set_config(spi, spi_config);
			sspi_trigger(spi,
				SSPI_TRIGGER_START, SPI_TYPE_INTEL_TRANSMIT);
		} else { /* HOST -> DSP */
			spi->ipc_status = IPC_WRITE;
			spi_config = &spi->config[SPI_TYPE_INTEL_RECEIVE];
			spi_config->dest_buf =
				(uint32_t)&spi->rx_buffer[sizeof(struct sof_ipc_hdr)];
			spi_config->transfer_len = hdr->size;
			spi_config->cyclic = 1;
			sspi_set_config(spi, spi_config);
		}
		break;
	case IPC_WRITE:	/* HOST -> DSP */
		hdr = (struct sof_ipc_hdr *)spi->rx_buffer;
		spi->rx_size = hdr->size + sizeof(struct sof_ipc_hdr);
		/* info the main thread new command comes */
		_ipc->host_pending = 1;

		/* ready to receive the header */
		spi->ipc_status = IPC_IDLE;
		spi_config = &spi->config[SPI_TYPE_INTEL_RECEIVE];
		spi_config->dest_buf = (uint32_t)spi->rx_buffer;
		spi_config->transfer_len = 8;
		spi_config->cyclic = 1;
		sspi_set_config(spi, spi_config);
		break;

	case IPC_READ:	/* DSP -> HOST */
		/* stop transmit channel */
		sspi_trigger(spi,
			SSPI_TRIGGER_STOP, SPI_TYPE_INTEL_TRANSMIT);
		/* skip current DMA-channel1 */
		next->size = DMA_RELOAD_END;

		/* configure to receive next header */
		spi->ipc_status = IPC_IDLE;
		spi_config = &spi->config[SPI_TYPE_INTEL_RECEIVE];
		spi_config->dest_buf = (uint32_t)spi->rx_buffer;
		spi_config->transfer_len = 8;
		spi_config->cyclic = 1;
		sspi_set_config(spi, spi_config);
		sspi_trigger(spi,
			SSPI_TRIGGER_START, SPI_TYPE_INTEL_RECEIVE);
		break;
	}
}
#endif

static int spi_slave_probe(struct sspi *spi)
{
	// TODO
	//_ipc->spi = spi;

	spi->dma = dma_get(DMA_ID_DMAC0);
	spi->chan[SPI_TYPE_INTEL_RECEIVE]  = dma_channel_get(spi->dma);
	spi->chan[SPI_TYPE_INTEL_TRANSMIT] = dma_channel_get(spi->dma);

	/* configure the spi clock */
	io_reg_write(SSI_SLAVE_CLOCK_CTL, 0x00000001);

	spi->rx_buffer = rzalloc(RZONE_SYS, RFLAGS_DMA, SPI_BUFFER_SIZE);
	if (spi->rx_buffer == NULL) {
		trace_ipc_error("eSp");
		return -ENOMEM;
	}

	spi->tx_buffer = rzalloc(RZONE_SYS, RFLAGS_DMA, SPI_BUFFER_SIZE);
	if (spi->tx_buffer == NULL) {
		rfree(spi->rx_buffer);
		trace_ipc_error("eSp");
		return -ENOMEM;
	}

	spi->ipc_status = IPC_IDLE;
#if 0
	dma_set_cb(spi->dma,
		spi->chan[SPI_TYPE_INTEL_RECEIVE],
		DMA_IRQ_TYPE_LLIST, spi_dma_complete, _ipc->spi);
	dma_set_cb(spi->dma,
		spi->chan[SPI_TYPE_INTEL_TRANSMIT],
		DMA_IRQ_TYPE_LLIST, spi_dma_complete, _ipc->spi);
#endif
	return 0;
}

static int spi_slave_trigger(struct sspi *spi, int cmd, int direction)
{
	switch (cmd) {
	case SSPI_TRIGGER_START:
		/* trigger the SPI-Slave + DMA + INT + Receiving */
		dma_start(spi->dma, spi->chan[direction]);
		spi_start(spi, direction);
		break;
	case SSPI_TRIGGER_STOP:
		/* Stop the SPI-Slave */
		spi_stop(spi);
		dma_stop(spi->dma, spi->chan[direction]);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int spi_slave_set_config(struct sspi *spi,
						struct spi_dma_config *spi_cfg)
{
	/* spi slave config */
	spi_config(spi, spi_cfg);

	/* dma config */
	spi_slave_dma_set_config(spi, spi_cfg);

	return 0;
}

static int spi_slave_set_loopback_mode(struct sspi *spi, uint32_t lbm)
{
	return 0;
}

const struct sspi_ops spi_ops = {
	.trigger		= spi_slave_trigger,
	.set_config		= spi_slave_set_config,
	.probe			= spi_slave_probe,
	.set_loopback_mode	= spi_slave_set_loopback_mode,
};

static struct sspi spi_slave[1] = {
{
	.type  = SOF_SPI_INTEL_SLAVE,
	.index = 0,
	.plat_data = {
		.base		= SPI_SLAVE_BASE,
		.irq		= IRQ_EXT_LP_GPDMA0_LVL5(0),
		.fifo[SPI_TYPE_INTEL_RECEIVE] = {
			.offset		= SUE_SPI_REG_DR,
			.handshake	= DMA_HANDSHAKE_SSPI_RX,
		},
		.fifo[SPI_TYPE_INTEL_TRANSMIT] = {
			.offset		= SUE_SPI_REG_DR,
			.handshake	= DMA_HANDSHAKE_SSPI_TX,
		}
	},
	.ops = &spi_ops,
},
};

struct sspi *sspi_get(uint32_t type)
{
	if (type == SOF_SPI_INTEL_SLAVE)
		return &spi_slave[0];

	return NULL;
}

