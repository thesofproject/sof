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
#include <sof/ipc.h>
#include <uapi/ipc.h>
#include <sof/spi.h>
#include <sof/dma.h>
#include <sof/io.h>
#include <sof/clk.h>
#include <sof/util.h>
#include <platform/dma.h>
#include <platform/sue-gpio.h>

extern struct ipc *_ipc;

#define SPI_SLAVE_BASE			0x00080000
#define DMA_HANDSHAKE_SSPI_TX		26
#define DMA_HANDSHAKE_SSPI_RX		27

#define SSI_SLAVE_CLOCK_CTL		0x00081C60

/* CTRLR0 */
/* 00-standard spi; 01-dual spi; 10-quad spi */
#define SPI_FRAME_FORMAT(x)		(x << 21)
#define SPI_DATA_FRAME_SIZE(x)		(x << 16)
/* 0-slave txt enabled; 1-slave txt disabled */
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
#define IPC_SPI_MASTER_WRITE		(0 << 27)

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
	case SPI_DIR_RX:
		io_reg_write(SUE_SPI_REG_RXFTLR, spi->reg.rxftlr);
		io_reg_write(SUE_SPI_REG_DMARDLR, spi->reg.dmardlr);
		break;
	case SPI_DIR_TX:
		io_reg_write(SUE_SPI_REG_TXFTLR, spi->reg.txftlr);
		io_reg_write(SUE_SPI_REG_DMATDLR, spi->reg.dmatdlr);
		break;
	default:
		break;
	}
	io_reg_write(SUE_SPI_REG_DMACR, spi->reg.dmacr);
	io_reg_write(SUE_SPI_REG_SSIENR, spi->reg.ssienr);
}

static inline void spi_stop(struct sspi *spi)
{
	io_reg_write(SUE_SPI_REG_DMACR,
		SPI_DMACR_TDMAE(0) | SPI_DMACR_RDMAE(0));
	io_reg_write(SUE_SPI_REG_SSIENR, 0);
}

static inline void spi_config(struct sspi *spi, struct spi_dma_config *spi_cfg)
{
	switch (spi_cfg->type) {
	case SPI_DIR_RX:
		spi->reg.ctrlr0 = SPI_FRAME_FORMAT(0)
						| SPI_DATA_FRAME_SIZE(0x1f)
						| SPI_TRANSFER_MODE(0x2)
						| SPI_SCPOL(1)
						| SPI_SCPH(1)
						| SPI_FRAME_TYPE(0);
		spi->reg.rxftlr  = 1;				/*4-byte FIFO*/
		spi->reg.imr	|= SPI_IMR_RXFIM(1);
		spi->reg.dmardlr = SPI_DMARDLR(0);	/*4-byte FIFO*/
		spi->reg.dmacr	|= SPI_DMACR_RDMAE(1);
		spi->reg.ssienr  = SPI_SSIEN;
		break;
	case SPI_DIR_TX:
		spi->reg.ctrlr0 = SPI_FRAME_FORMAT(0)
						| SPI_DATA_FRAME_SIZE(0x1f)
						| SPI_TRANSFER_MODE(0x1)
						| SPI_SCPOL(1)
						| SPI_SCPH(1)
						| SPI_FRAME_TYPE(0);
		spi->reg.txftlr  = 1;				/*4-byte FIFO*/
		spi->reg.imr	|= SPI_IMR_TXEIM(1);
		spi->reg.dmatdlr = SPI_DMATDLR(0);	/*4-byte FIFO*/
		spi->reg.dmacr	|= SPI_DMACR_TDMAE(1);
		spi->reg.ssienr  = SPI_SSIEN;
		break;
	default:
		break;
	}
}

static int spi_slave_dma_set_config(struct sspi *spi, struct spi_dma_config *spi_cfg)
{
	struct dma_sg_config config;
	struct dma_sg_elem local_sg_elem;
	struct dma *dma;
	uint32_t chan;

	/*
	 * A note on page 87, claiming that SPI source and destination width are 16
	 * bits is wrong, they are 32 bits in fact
	 */
	switch (spi_cfg->type) {
	case SPI_DIR_RX:	/* HOST -> DSP */
		/* dma config */
		dma = spi->dma[SPI_DIR_RX];
		chan = spi->chan[SPI_DIR_RX];
		config.direction  = DMA_DIR_DEV_TO_MEM;
		config.src_width  = 4;
		config.dest_width = 4;
		config.cyclic     = spi_cfg->cyclic;
		config.src_dev    = spi_fifo_handshake(spi, SPI_DIR_RX);

		local_sg_elem.dest = (uint32_t)spi_cfg->dest_buf;
		local_sg_elem.src  = (uint32_t)spi_fifo_offset(spi, SPI_DIR_RX);
		local_sg_elem.size = spi_cfg->transfer_len;
		break;
	case SPI_DIR_TX:	/* DSP -> HOST */
		/* dma config */
		dma = spi->dma[SPI_DIR_TX];
		chan = spi->chan[SPI_DIR_TX];
		config.direction  = DMA_DIR_MEM_TO_DEV;
		config.src_width  = 4;
		config.dest_width = 4;
		config.cyclic     = spi_cfg->cyclic;
		config.dest_dev   = spi_fifo_handshake(spi, SPI_DIR_TX);

		local_sg_elem.src  = (uint32_t)spi_cfg->src_buf;
		local_sg_elem.dest = (uint32_t)spi_fifo_offset(spi, SPI_DIR_TX);
		local_sg_elem.size = spi_cfg->transfer_len;
		break;
	default:
		return -EINVAL;
	}

	/* configure local DMA elem */
	config.elem_array.count = 1;
	config.elem_array.elems = &local_sg_elem;

	return dma_set_config(dma, chan, &config);
}

#if 1 // TODO
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
			sspi_trigger(spi, SSPI_TRIGGER_STOP, SPI_DIR_RX);
			/* skip current DMA-channel0 */
			next->size = DMA_RELOAD_END;

			spi->ipc_status = IPC_READ;
			spi_config = &spi->config[SPI_DIR_TX];
			spi_config->src_buf = spi->tx_buffer;
			/*
			 * i need add 1 here,
			 * seems the slave always send number bytes sub 1
			 */
			spi_config->transfer_len = hdr->size/*+1*/;
			spi_config->cyclic = 1;
			sspi_set_config(spi, spi_config);
			sspi_trigger(spi, SSPI_TRIGGER_START, SPI_DIR_TX);
		} else { /* HOST -> DSP */
			spi->ipc_status = IPC_WRITE;
			spi_config = &spi->config[SPI_DIR_RX];
			spi_config->dest_buf =
				&spi->rx_buffer[sizeof(struct sof_ipc_hdr)];
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
		spi_config = &spi->config[SPI_DIR_RX];
		spi_config->dest_buf = spi->rx_buffer;
		spi_config->transfer_len = sizeof(*hdr);
		spi_config->cyclic = 1;
		sspi_set_config(spi, spi_config);
		break;

	case IPC_READ:	/* DSP -> HOST */
		/* stop transmit channel */
		sspi_trigger(spi, SSPI_TRIGGER_STOP, SPI_DIR_TX);
		/* skip current DMA-channel1 */
		next->size = DMA_RELOAD_END;

		/* FIXME: why is it necessary to reconfigure RX after completing TX? */

		/* configure to receive next header */
		spi->ipc_status = IPC_IDLE;
		spi_config = &spi->config[SPI_DIR_RX];
		spi_config->dest_buf = spi->rx_buffer;
		spi_config->transfer_len = sizeof(*hdr);
		spi_config->cyclic = 1;
		sspi_set_config(spi, spi_config);
		sspi_trigger(spi, SSPI_TRIGGER_START, SPI_DIR_RX);
		break;
	}

	/*
	 * Do we actually need local buffers or should we use caller's buffers
	 * since they are always DMA-capable anyway and the DMA controller
	 * supports SG, so we should be able to just setup an SG array of the
	 * required length? If we use SG, the completion will only be called
	 * once after the complete transfer has completed.
	 */
	wait_completed(&spi->complete);
}
#endif

static int spi_slave_probe(struct sspi *spi)
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

	spi->ipc_status = IPC_IDLE;

	dma_set_cb(spi->dma[SPI_DIR_RX],
		spi->chan[SPI_DIR_RX],
		DMA_IRQ_TYPE_LLIST, spi_dma_complete, spi);
	dma_set_cb(spi->dma[SPI_DIR_TX],
		spi->chan[SPI_DIR_TX],
		DMA_IRQ_TYPE_LLIST, spi_dma_complete, spi);

	return 0;
}

static void delay(unsigned int ms)
{
	uint64_t tick = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, ms);
	wait_delay(tick);
}

static int spi_slave_trigger(struct sspi *spi, int cmd, int direction)
{
	int ret;

	switch (cmd) {
	case SSPI_TRIGGER_START:
		/* trigger the SPI-Slave + DMA + INT + Receiving */
		ret = dma_start(spi->dma[direction], spi->chan[direction]);
		if (ret < 0) {
			trace_point(0x6000 - ret);
			return ret;
		}
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

static int spi_slave_set_config(struct sspi *spi, struct spi_dma_config *spi_cfg)
{
	/* spi slave config */
	spi_config(spi, spi_cfg);

	/* dma config */
	return spi_slave_dma_set_config(spi, spi_cfg);
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
	.ops = &spi_ops,
},
};

struct sspi *sspi_get(uint32_t type)
{
	if (type == SOF_SPI_INTEL_SLAVE)
		return &spi_slave[0];

	return NULL;
}

void spi_push(void *data, size_t size)
{
	struct sspi *spi = sspi_get(SOF_SPI_INTEL_SLAVE);
	struct spi_dma_config *config = spi->config + SPI_DIR_TX;

	spi->ipc_status = IPC_WRITE;

	do {
		size_t chunk_size = min(size, SPI_BUFFER_SIZE);

		rmemcpy(config->src_buf, data, chunk_size);
		dcache_writeback_region(config->src_buf, chunk_size);
		sspi_trigger(spi, SSPI_TRIGGER_START, SPI_DIR_TX);
		data += chunk_size;
		size -= chunk_size;
		gpio_write(GPIO14, 1);
		delay(1);
		gpio_write(GPIO14, 0);
		/* 0.5s */
		spi->complete.timeout = 500000;
		wait_for_completion_timeout(&spi->complete);
		wait_clear(&spi->complete);
		if (spi->complete.timeout)
			break;
	} while (size);
}

int spi_slave_init(struct sspi *spi, uint32_t type)
{
	int ret;

	if (type != SOF_SPI_INTEL_SLAVE)
		return -EINVAL;

	/* configure receive path of SPI-slave */
	bzero((void *)&spi->config[SPI_DIR_RX],
			sizeof(spi->config[SPI_DIR_RX]));
	spi->config[SPI_DIR_RX].type = SPI_DIR_RX;
	spi->config[SPI_DIR_RX].src_buf = NULL/*spi->tx_buffer*/;
	spi->config[SPI_DIR_RX].dest_buf = spi->rx_buffer;
	spi->config[SPI_DIR_RX].transfer_len = 8;
	spi->config[SPI_DIR_RX].cyclic = 1;
	ret = sspi_set_config(spi, &spi->config[SPI_DIR_RX]);
	if (ret < 0)
		return ret;

	ret = sspi_trigger(spi, SSPI_TRIGGER_START, SPI_DIR_RX);
	if (ret < 0)
		return ret;

	/* configure transmit path of SPI-slave */
	bzero((void *)&spi->config[SPI_DIR_TX],
			sizeof(spi->config[SPI_DIR_TX]));
	spi->config[SPI_DIR_TX].type = SPI_DIR_TX;
	spi->config[SPI_DIR_TX].src_buf = spi->tx_buffer;
	spi->config[SPI_DIR_TX].dest_buf = NULL/*spi->rx_buffer*/;
	spi->config[SPI_DIR_TX].transfer_len = 8;
	sspi_set_config(spi, &spi->config[SPI_DIR_TX]);

	wait_init(&spi->complete);

	return 0;
}
