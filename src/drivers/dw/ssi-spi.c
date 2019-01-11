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
#include <stdbool.h>
#include <stdint.h>

#include <sof/clk.h>
#include <sof/dma.h>
#include <sof/gpio.h>
#include <sof/io.h>
#include <sof/ipc.h>
#include <sof/lock.h>
#include <sof/schedule.h>
#include <sof/sof.h>
#include <sof/spi.h>
#include <sof/work.h>

#include <platform/dma.h>
#include <platform/memory.h>
#include <platform/platform.h>

#define	SPI_REG_CTRLR0		0x00
#define	SPI_REG_CTRLR1		0x04
#define	SPI_REG_SSIENR		0x08
#define	SPI_REG_MWCR		0x0C
#define	SPI_REG_SER		0x10
#define	SPI_REG_BAUDR		0x14
#define	SPI_REG_TXFTLR		0x18
#define	SPI_REG_RXFTLR		0x1C
#define	SPI_REG_TXFLR		0x20
#define	SPI_REG_RXFLR		0x24
#define	SPI_REG_SR		0x28
#define	SPI_REG_IMR		0x2C
#define	SPI_REG_ISR		0x30
#define	SPI_REG_RISR		0x34
#define	SPI_REG_TXOICR		0x38
#define	SPI_REG_RXOICR		0x3C
#define	SPI_REG_RXUICR		0x40
#define	SPI_REG_ICR		0x48
#define	SPI_REG_DMACR		0x4C
#define	SPI_REG_DMATDLR		0x50
#define	SPI_REG_DMARDLR		0x54
#define	SPI_REG_DR		0x60
#define	SPI_REG_SPICTRLR0	0xF4

#define SPI_BUFFER_SIZE		256

#define SSI_SLAVE_CLOCK_CTL	(EXT_CTRL_BASE + 0x60)

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

enum {
	SPI_TRIGGER_START,
	SPI_TRIGGER_STOP,
};

/* SPI-Slave ISR's state machine: from the PoV of the DSP */
enum IPC_STATUS {
	IPC_READ,
	IPC_WRITE,
};

struct spi_dma_config {
	enum spi_xfer_direction dir;
	void *src_buf;
	void *dest_buf;
	uint32_t transfer_len;
};

struct spi_reg_list {
	uint32_t ctrlr0;
	uint32_t ctrlr1;
	uint32_t dmacr;		/* dma control register */
};

struct spi {
	const struct gpio *gpio;
	uint32_t index;
	uint32_t chan[2];	/* spi-slave rx/tx dma */
	uint8_t *rx_buffer;
	uint8_t *tx_buffer;
	struct dma *dma[2];
	struct spi_reg_list reg;
	const struct spi_platform_data *plat_data;
	struct spi_dma_config config[2];
	uint32_t ipc_status;
	struct task completion;
	struct sof_ipc_hdr hdr;
};

#define spi_fifo_handshake(spi, direction) \
			spi->plat_data->fifo[direction].handshake
#define spi_reg_write(spi, reg, val) \
			io_reg_write(spi->plat_data->base + reg, val)

extern struct ipc *_ipc;

static void spi_start(struct spi *spi, int direction)
{
	/* disable SPI first before config */
	spi_reg_write(spi, SPI_REG_SSIENR, 0);

	spi_reg_write(spi, SPI_REG_CTRLR0, spi->reg.ctrlr0);
	spi_reg_write(spi, SPI_REG_IMR, 0);

	/* Trigger interrupt at or above 1 entry in the RX FIFO */
	spi_reg_write(spi, SPI_REG_RXFTLR, 1);
	/* Trigger DMA at or above 1 entry in the RX FIFO */
	spi_reg_write(spi, SPI_REG_DMARDLR, SPI_DMARDLR(1));

	/* Trigger interrupt at or below 1 entry in the TX FIFO */
	spi_reg_write(spi, SPI_REG_TXFTLR, 1);
	/* Trigger DMA at or below 1 entry in the TX FIFO */
	spi_reg_write(spi, SPI_REG_DMATDLR, SPI_DMATDLR(1));

	spi_reg_write(spi, SPI_REG_DMACR, spi->reg.dmacr);
	spi_reg_write(spi, SPI_REG_SSIENR, SPI_SSIEN);
}

static void spi_stop(struct spi *spi)
{
	spi_reg_write(spi, SPI_REG_DMACR,
		     SPI_DMACR_TDMAE(0) | SPI_DMACR_RDMAE(0));
	spi_reg_write(spi, SPI_REG_SSIENR, 0);
}

static void delay(unsigned int ms)
{
	uint64_t tick = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, ms);

	wait_delay(tick);
}

static int spi_trigger(struct spi *spi, int cmd, int direction)
{
	int ret;

	switch (cmd) {
	case SPI_TRIGGER_START:
		/* trigger the SPI-Slave + DMA + INT + Receiving */
		ret = dma_start(spi->dma[direction], spi->chan[direction]);
		if (ret < 0)
			return ret;

		/*
		 * DMA seems to need some time before it can service a data
		 * request from the SPI FIFO
		 */
		wait_delay(1);

		spi_start(spi, direction);

		break;
	case SPI_TRIGGER_STOP:
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
static inline void spi_config(struct spi *spi,
			      const struct spi_dma_config *spi_cfg)
{
	switch (spi_cfg->dir) {
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

static int spi_slave_dma_set_config(struct spi *spi,
				    const struct spi_dma_config *spi_cfg)
{
	struct dma_sg_config config = {
		.src_width  = 4,
		.dest_width = 4,
	};
	struct dma_sg_elem local_sg_elem;
	struct dma *dma = spi->dma[spi_cfg->dir];
	uint32_t chan = spi->chan[spi_cfg->dir];

	/* dma config */

	local_sg_elem.size = spi_cfg->transfer_len;

	/*
	 * Source and destination width is 32 bits, contrary to dw_apb_ssi note
	 * on page 87
	 */
	switch (spi_cfg->dir) {
	case SPI_DIR_RX:	/* HOST -> DSP */
		config.direction	= DMA_DIR_DEV_TO_MEM;
		config.scatter		= true;
		config.src_dev		= spi_fifo_handshake(spi,
							spi_cfg->dir);

		local_sg_elem.dest	= (uint32_t)spi_cfg->dest_buf;
		local_sg_elem.src	= spi->plat_data->base + SPI_REG_DR;
		break;
	case SPI_DIR_TX:	/* DSP -> HOST */
		config.direction	= DMA_DIR_MEM_TO_DEV;
		config.dest_dev		= spi_fifo_handshake(spi,
							spi_cfg->dir);

		local_sg_elem.dest	= spi->plat_data->base + SPI_REG_DR;
		local_sg_elem.src	= (uint32_t)spi_cfg->src_buf;
		break;
	default:
		return -EINVAL;
	}

	/* configure local DMA elem */
	config.elem_array.count = 1;
	config.elem_array.elems = &local_sg_elem;

	return dma_set_config(dma, chan, &config);
}

static int spi_set_config(struct spi *spi,
			   const struct spi_dma_config *spi_cfg)
{
	/* spi slave config */
	spi_config(spi, spi_cfg);

	/* dma config */
	return spi_slave_dma_set_config(spi, spi_cfg);
}

static void spi_completion_work(void *data)
{
	struct spi *spi = data;
	struct sof_ipc_hdr *hdr;
	struct spi_dma_config *config;

	switch (spi->ipc_status) {
	case IPC_READ:
		hdr = (struct sof_ipc_hdr *)spi->rx_buffer;

		dcache_invalidate_region(spi->rx_buffer, SPI_BUFFER_SIZE);
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
		spi_trigger(spi, SPI_TRIGGER_STOP, SPI_DIR_TX);

		/* configure to receive next header */
		spi->ipc_status = IPC_READ;
		config = spi->config + SPI_DIR_RX;
		config->transfer_len = ALIGN(sizeof(*hdr), 16);
		spi_set_config(spi, config);
		spi_trigger(spi, SPI_TRIGGER_START, SPI_DIR_RX);

		break;
	}
}

static void spi_dma_complete(void *data, uint32_t type,
			     struct dma_sg_elem *next)
{
	struct spi *spi = data;

	next->size = DMA_RELOAD_END;

	schedule_task(&spi->completion, 0, 100);
}

int spi_push(struct spi *spi, const void *data, size_t size)
{
	struct spi_dma_config *config = spi->config + SPI_DIR_TX;
	int ret;

	if (size > SPI_BUFFER_SIZE) {
		trace_ipc_error("ePs");
		return -ENOBUFS;
	}

	ret = spi_trigger(spi, SPI_TRIGGER_STOP, SPI_DIR_RX);
	if (ret < 0)
		return ret;

	/* configure transmit path of SPI-slave */
	config->transfer_len = ALIGN(size, 16);
	ret = spi_set_config(spi, config);
	if (ret < 0)
		return ret;

	spi->ipc_status = IPC_WRITE;

	/* Actually we have to send IPC messages in one go */
	rmemcpy(config->src_buf, data, size);
	dcache_writeback_region(config->src_buf, size);

	ret = spi_trigger(spi, SPI_TRIGGER_START, SPI_DIR_TX);
	if (ret < 0)
		return ret;

	/*
	 * Tell the master to pull out the data, we aren't getting DMA
	 * completion until all the prepared data has been transferred
	 * to the SPI controller FIFO
	 */
	gpio_write(spi->gpio, PLATFORM_SPI_GPIO_IRQ, GPIO_LEVEL_HIGH);
	delay(1);
	gpio_write(spi->gpio, PLATFORM_SPI_GPIO_IRQ, GPIO_LEVEL_LOW);

	return 0;
}

static int spi_slave_init(struct spi *spi)
{
	struct spi_dma_config *config = spi->config + SPI_DIR_RX;
	struct gpio_config gpio_cfg = {.direction = GPIO_DIRECTION_OUTPUT};
	int ret;

	/* A GPIO to signal host IPC IRQ */
	gpio_configure(spi->gpio, PLATFORM_SPI_GPIO_IRQ, &gpio_cfg);

	bzero(spi->config, sizeof(spi->config));

	/* configure receive path of SPI-slave */
	config->dir = SPI_DIR_RX;
	config->dest_buf = spi->rx_buffer;
	config->transfer_len = ALIGN(sizeof(struct sof_ipc_hdr), 16);

	ret = spi_set_config(spi, config);
	if (ret < 0)
		return ret;

	dcache_invalidate_region(spi->rx_buffer, SPI_BUFFER_SIZE);
	ret = spi_trigger(spi, SPI_TRIGGER_START, SPI_DIR_RX);
	if (ret < 0)
		return ret;

	/* prepare transmit path of SPI-slave */
	config = spi->config + SPI_DIR_TX;
	config->dir = SPI_DIR_TX;
	config->src_buf = spi->tx_buffer;

	schedule_task_init(&spi->completion, spi_completion_work, spi);
	schedule_task_config(&spi->completion, TASK_PRI_MED, 0);

	return 0;
}

int spi_probe(struct spi *spi)
{
	int ret;

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

	spi->gpio = gpio_get(PLATFORM_SPI_GPIO_ID);
	if (!spi->gpio)
		return -ENODEV;

	ret = gpio_probe(spi->gpio);
	if (ret < 0)
		return ret;

	/* configure the spi clock */
	io_reg_write(SSI_SLAVE_CLOCK_CTL, 0x00000001);

	spi->rx_buffer = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_DMA,
				 SPI_BUFFER_SIZE);
	if (spi->rx_buffer == NULL) {
		trace_ipc_error("eSp");
		return -ENOMEM;
	}

	spi->tx_buffer = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_DMA,
				 SPI_BUFFER_SIZE);
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

	return spi_slave_init(spi);
}

spinlock_t spi_lock;
static struct spi *spi_devices;
static unsigned int n_spi_devices;

struct spi *spi_get(enum spi_type type)
{
	struct spi *spi;
	unsigned int i, flags;

	spin_lock_irq(&spi_lock, flags);

	for (i = 0, spi = spi_devices; i < n_spi_devices; i++, spi++)
		if (spi->plat_data->type == type)
			break;

	spin_unlock_irq(&spi_lock, flags);

	return i < n_spi_devices ? spi : NULL;
}

int spi_install(const struct spi_platform_data *plat, size_t n)
{
	struct spi *spi;
	unsigned int i, flags;
	int ret;

	spin_lock_irq(&spi_lock, flags);

	if (spi_devices) {
		ret = -EBUSY;
		goto unlock;
	}

	spi_devices = rmalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*spi) * n);
	if (!spi_devices) {
		ret = -ENOMEM;
		goto unlock;
	}

	n_spi_devices = n;
	ret = 0;

	for (i = 0, spi = spi_devices; i < n; i++, spi++) {
		spi->index = i;
		spi->plat_data = plat + i;
	}

unlock:
	spin_unlock_irq(&spi_lock, flags);

	return ret;
}

void spi_init(void)
{
	spinlock_init(&spi_lock);
}
