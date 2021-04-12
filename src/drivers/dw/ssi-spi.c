// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>

#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/gpio.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/schedule.h>
#include <sof/drivers/spi.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/clk.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <ipc/header.h>
#include <ipc/topology.h>
#include <stddef.h>
#include <stdint.h>

/* a417b6fb-459d-4cf9-be65-d38dc9057b80 */
DECLARE_SOF_UUID("spi-completion", spi_compl_task_uuid, 0xa417b6fb,
		 0x459d, 0x4cf9,
		 0xbe, 0x65, 0xd3, 0x8d, 0xc9, 0x05, 0x7b, 0x80);

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
	uint32_t buffer_size;
	uint32_t transfer_len;
};

struct spi_reg_list {
	uint32_t ctrlr0;
	uint32_t dmacr;		/* dma control register */
};

struct spi {
	const struct gpio *gpio;
	uint32_t index;
	struct dma_chan_data *chan[2];	/* spi-slave rx/tx dma */
	uint32_t buffer_size;
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
		ret = dma_start(spi->chan[direction]);
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
		dma_stop(spi->chan[direction]);

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
	struct dma_chan_data *chan = spi->chan[spi_cfg->dir];

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

	return dma_set_config(chan, &config);
}

static int spi_set_config(struct spi *spi,
			   const struct spi_dma_config *spi_cfg)
{
	/* spi slave config */
	spi_config(spi, spi_cfg);

	/* dma config */
	return spi_slave_dma_set_config(spi, spi_cfg);
}

static enum task_state spi_completion_work(void *data)
{
	struct spi *spi = data;
	struct sof_ipc_hdr *hdr;
	struct spi_dma_config *config;

	switch (spi->ipc_status) {
	case IPC_READ:
		hdr = (struct sof_ipc_hdr *)spi->rx_buffer;

		dcache_invalidate_region(spi->rx_buffer, SPI_BUFFER_SIZE);
		mailbox_hostbox_write(0, spi->rx_buffer, hdr->size);

		ipc_schedule_process(ipc_get());

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
		config->transfer_len = ALIGN_UP_COMPILE(sizeof(*hdr), 16);
		spi_set_config(spi, config);
		spi_trigger(spi, SPI_TRIGGER_START, SPI_DIR_RX);

		break;
	}

	return SOF_TASK_STATE_RESCHEDULE;
}

int spi_push(struct spi *spi, const void *data, size_t size)
{
	struct spi_dma_config *config = spi->config + SPI_DIR_TX;
	int ret;

	if (size > SPI_BUFFER_SIZE) {
		tr_err(&ipc_tr, "ePs");
		return -ENOBUFS;
	}

	ret = spi_trigger(spi, SPI_TRIGGER_STOP, SPI_DIR_RX);
	if (ret < 0)
		return ret;

	/* configure transmit path of SPI-slave */
	config->transfer_len = ALIGN_UP_COMPILE(size, 16);
	ret = spi_set_config(spi, config);
	if (ret < 0)
		return ret;

	spi->ipc_status = IPC_WRITE;

	/* Actually we have to send IPC messages in one go */
	ret = memcpy_s(config->src_buf, config->buffer_size, data, size);
	assert(!ret);

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
	config->transfer_len = ALIGN_UP_COMPILE(sizeof(struct sof_ipc_hdr), 16);

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
	config->buffer_size = spi->buffer_size;

	spi->completion.priv_data = NULL;

	ret = schedule_task_init_ll(&spi->completion,
				    SOF_UUID(spi_compl_task_uuid),
				    SOF_SCHEDULE_LL_DMA,
				    SOF_TASK_PRI_MED, spi_completion_work,
				    spi, 0, 0);
	if (ret < 0)
		return ret;

	schedule_task(&spi->completion, 0, 100);

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
	if (!spi->chan[SPI_DIR_RX])
		return -ENODEV;

	spi->chan[SPI_DIR_TX] = dma_channel_get(spi->dma[SPI_DIR_TX], 0);
	if (!spi->chan[SPI_DIR_TX])
		return -ENODEV;

	spi->gpio = gpio_get(PLATFORM_SPI_GPIO_ID);
	if (!spi->gpio)
		return -ENODEV;

	ret = gpio_probe(spi->gpio);
	if (ret < 0)
		return ret;

	/* configure the spi clock */
	io_reg_write(SSI_SLAVE_CLOCK_CTL, 0x00000001);

	spi->rx_buffer = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_DMA,
				 SPI_BUFFER_SIZE);
	if (!spi->rx_buffer) {
		tr_err(&ipc_tr, "eSp");
		return -ENOMEM;
	}

	spi->tx_buffer = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_DMA,
				 SPI_BUFFER_SIZE);
	spi->buffer_size = SPI_BUFFER_SIZE;
	if (!spi->tx_buffer) {
		rfree(spi->rx_buffer);
		tr_err(&ipc_tr, "eSp");
		return -ENOMEM;
	}

	spi->ipc_status = IPC_READ;

	return spi_slave_init(spi);
}

/* lock */
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

	spi_devices = rmalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
			      sizeof(*spi) * n);
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
