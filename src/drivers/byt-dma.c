/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/dma.h>
#include <reef/byt-dma.h>
#include <reef/io.h>
#include <reef/stream.h>
#include <reef/timer.h>
#include <platform/dma.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>


/* channel registers */
#define BYT_DMA_MAX_CHAN		8
#define BYT_DMA_CH_SIZE			0x58
#define BYT_CHAN_OFFSET(chan) \
	(BYT_DMA_CH_SIZE * chan)

#define BYT_DMA_SAR(chan)	\
	(0x0000 + BYT_CHAN_OFFSET(chan))
#define BYT_DMA_DAR(chan) \
	(0x0008 + BYT_CHAN_OFFSET(chan))
#define BYT_DMA_LLP(chan) \
	(0x0010 + BYT_CHAN_OFFSET(chan))
#define BYT_DMA_CTRL_LOW(chan) \
	(0x0018 + BYT_CHAN_OFFSET(chan))
#define BYT_DMA_CTRL_HIGH(chan) \
	(0x001C + BYT_CHAN_OFFSET(chan))
#define BYT_DMA_CFG_LOW(chan) \
	(0x0040 + BYT_CHAN_OFFSET(chan))
#define BYT_DMA_CFG_HIGH(chan) \
	(0x0044 + BYT_CHAN_OFFSET(chan))

/* registers */
#define BYT_DMA_STATUS_TFR		0x02E8
#define BYT_DMA_STATUS_BLOCK		0x02F0
#define BYT_DMA_STATUS_ERR		0x0308
#define BYT_DMA_RAW_TFR			0x02C0
#define BYT_DMA_RAW_BLOCK		0x02C8
#define BYT_DMA_RAW_ERR			0x02E0
#define BYT_DMA_MASK_TFR		0x0310
#define BYT_DMA_MASK_BLOCK		0x0318
#define BYT_DMA_MASK_SRC_TRAN		0x0320
#define BYT_DMA_MASK_DST_TRAN		0x0328
#define BYT_DMA_MASK_ERR		0x0330
#define BYT_DMA_CLEAR_TFR		0x0338
#define BYT_DMA_CLEAR_BLOCK		0x0340
#define BYT_DMA_CLEAR_SRC_TRAN		0x0348
#define BYT_DMA_CLEAR_DST_TRAN		0x0350
#define BYT_DMA_CLEAR_ERR		0x0358
#define BYT_DMA_INTR_STATUS		0x0360
#define BYT_DMA_DMA_CFG			0x0398
#define BYT_DMA_DMA_CHAN_EN		0x03A0
#define BYT_DMA_FIFO_PARTI0_LO		0x0400
#define BYT_DMA_FIFO_PART0_HI		0x0404
#define BYT_DMA_FIFO_PART1_LO		0x0408
#define BYT_DMA_FIFO_PART1_HI		0x040C
#define BYT_DMA_CH_SAI_ERR		0x0410

/* channel bits */
#define INT_MASK(chan)			(0x100 << chan)
#define INT_UNMASK(chan)		(0x101 << chan)
#define CHAN_ENABLE(chan)		(0x101 << chan)
#define CHAN_DISABLE(chan)		(0x100 << chan)

#define BYT_DMA_CFG_CH_SUSPEND		0x100
#define BYT_DMA_CFG_CH_DRAIN		0x400
#define BYT_DMA_CFG_CH_FIFO_EMPTY	0x200

struct dma_pdata {
	uint8_t chan[BYT_DMA_MAX_CHAN];
};

/* allocate next free DMA channel */
static int byt_dma_channel_get(struct dma *dma)
{
	struct dma_pdata *p = dma->data;
	int i;
	
	/* find first free channel */
	for (i = 0; i < BYT_DMA_MAX_CHAN; i++) {
		if (p->chan[i] == DMA_STATUS_FREE) {
			p->chan[i] = DMA_STATUS_IDLE;
			return i;
		}
	}

	return -ENODEV;
}

static void byt_dma_channel_put(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma->data;

	p->chan[channel] = DMA_STATUS_FREE;
	// TODO: disable/reset any other channel config.
}


static int byt_dma_start(struct dma *dma, int channel)
{
	
	return 0;
}

/* this tasklet is call by the general purpose timer */
static void byt_dma_fifo_work(void *data)
{
	struct dma *dma = (struct dma *)data;
	struct dma_pdata *p = dma->data;
	int i;
	uint32_t cfg;

	/* check any draining channels */
	for (i = 0; i < BYT_DMA_MAX_CHAN; i++) {

		/* is channel draining */
		if (p->chan[i] != DMA_STATUS_DRAINING)
			continue;

		/* check for FIFO empty */
		cfg = io_reg_read(dma->base + BYT_DMA_CFG_LOW(i));
		if (cfg & BYT_DMA_CFG_CH_FIFO_EMPTY) {

			/* disable channel */
			io_reg_update_bits(dma->base + BYT_DMA_DMA_CHAN_EN,
				CHAN_DISABLE(i), CHAN_DISABLE(i));
			p->chan[i] == DMA_STATUS_IDLE;
		}
	}
}

#define REEF_SYS_TIMER 	0

static int byt_dma_stop(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma->data;

	/* suspend the channel */
	io_reg_update_bits(dma->base + BYT_DMA_CFG_LOW(channel),
		BYT_DMA_CFG_CH_SUSPEND, BYT_DMA_CFG_CH_SUSPEND);

	p->chan[channel] = DMA_STATUS_DRAINING;
	
	/* FIFO cleanup done by general purpose timer */
	timer_schedule_work(REEF_SYS_TIMER, byt_dma_fifo_work, dma, 1);
	return 0;
}

static int byt_dma_drain(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma->data;

	/* suspend the channel */
	io_reg_update_bits(dma->base + BYT_DMA_CFG_LOW(channel),
		BYT_DMA_CFG_CH_SUSPEND | BYT_DMA_CFG_CH_DRAIN,
		BYT_DMA_CFG_CH_SUSPEND | BYT_DMA_CFG_CH_DRAIN);

	p->chan[channel] = DMA_STATUS_DRAINING;

	/* FIFO cleanup done by general purpose timer */
	return 0;
}

static int byt_dma_status(struct dma *dma, int channel,
	struct dma_chan_status *status)
{
	
	return 0;
}

static int byt_dma_set_config(struct dma *dma, int channel,
	struct dma_chan_config *config)
{	
	return 0;
}

static int byt_dma_set_desc(struct dma *dma, int channel,
	struct dma_desc *desc)
{
	return 0;
}

static int byt_dma_pm_context_restore(struct dma *dma)
{
	return 0;
}

static int byt_dma_pm_context_store(struct dma *dma)
{
	return 0;
}

const struct dma_ops byt_dma_ops = {
	.channel_get	= byt_dma_channel_get,
	.channel_put	= byt_dma_channel_put,
	.start		= byt_dma_start,
	.stop		= byt_dma_stop,
	.drain		= byt_dma_drain,
	.status		= byt_dma_status,
	.set_config	= byt_dma_set_config,
	.set_desc	= byt_dma_set_desc,
	.pm_context_restore		= byt_dma_pm_context_restore,
	.pm_context_store		= byt_dma_pm_context_store,
};

