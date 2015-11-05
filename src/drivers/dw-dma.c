/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * DW DMA driver.
 *
 * DW DMA IP comes in several flavours each with different capabilities and
 * with register and bit changes between falvours.
 */

#include <reef/dma.h>
#include <reef/dw-dma.h>
#include <reef/io.h>
#include <reef/stream.h>
#include <reef/timer.h>
#include <platform/dma.h>
#include <platform/platform.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>


/* channel registers */
#define DW_MAX_CHAN		8
#define DW_CH_SIZE			0x58
#define BYT_CHAN_OFFSET(chan) \
	(DW_CH_SIZE * chan)

#define DW_SAR(chan)	\
	(0x0000 + BYT_CHAN_OFFSET(chan))
#define DW_DAR(chan) \
	(0x0008 + BYT_CHAN_OFFSET(chan))
#define DW_LLP(chan) \
	(0x0010 + BYT_CHAN_OFFSET(chan))
#define DW_CTRL_LOW(chan) \
	(0x0018 + BYT_CHAN_OFFSET(chan))
#define DW_CTRL_HIGH(chan) \
	(0x001C + BYT_CHAN_OFFSET(chan))
#define DW_CFG_LOW(chan) \
	(0x0040 + BYT_CHAN_OFFSET(chan))
#define DW_CFG_HIGH(chan) \
	(0x0044 + BYT_CHAN_OFFSET(chan))

/* registers */
#define DW_STATUS_TFR		0x02E8
#define DW_STATUS_BLOCK		0x02F0
#define DW_STATUS_ERR		0x0308
#define DW_RAW_TFR			0x02C0
#define DW_RAW_BLOCK		0x02C8
#define DW_RAW_ERR			0x02E0
#define DW_MASK_TFR		0x0310
#define DW_MASK_BLOCK		0x0318
#define DW_MASK_SRC_TRAN		0x0320
#define DW_MASK_DST_TRAN		0x0328
#define DW_MASK_ERR		0x0330
#define DW_CLEAR_TFR		0x0338
#define DW_CLEAR_BLOCK		0x0340
#define DW_CLEAR_SRC_TRAN		0x0348
#define DW_CLEAR_DST_TRAN		0x0350
#define DW_CLEAR_ERR		0x0358
#define DW_INTR_STATUS		0x0360
#define DW_DMA_CFG			0x0398
#define DW_DMA_CHAN_EN		0x03A0
#define DW_FIFO_PARTI0_LO		0x0400
#define DW_FIFO_PART0_HI		0x0404
#define DW_FIFO_PART1_LO		0x0408
#define DW_FIFO_PART1_HI		0x040C
#define DW_CH_SAI_ERR		0x0410

/* channel bits */
#define INT_MASK(chan)			(0x100 << chan)
#define INT_UNMASK(chan)		(0x101 << chan)
#define CHAN_ENABLE(chan)		(0x101 << chan)
#define CHAN_DISABLE(chan)		(0x100 << chan)

#define DW_CFG_CH_SUSPEND		0x100
#define DW_CFG_CH_DRAIN		0x400
#define DW_CFG_CH_FIFO_EMPTY	0x200

struct dma_pdata {
	uint8_t chan[DW_MAX_CHAN];
};

/* allocate next free DMA channel */
static int dw_dma_channel_get(struct dma *dma)
{
	struct dma_pdata *p = dma->data;
	int i;
	
	/* find first free non draining channel */
	for (i = 0; i < DW_MAX_CHAN; i++) {

		/* dont use any channels that are still draining */
		if (p->chan[i] & DMA_STATUS_DRAINING)
			continue;

		/* use channel if it's free */
		if (p->chan[i] & DMA_STATUS_FREE) {
			p->chan[i] = DMA_STATUS_IDLE;
			return i;
		}
	}

	/* DMAC has no free channels */
	return -ENODEV;
}

static void dw_dma_channel_put(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma->data;

	p->chan[channel] |= DMA_STATUS_FREE;
	// TODO: disable/reset any other channel config.
}


static int dw_dma_start(struct dma *dma, int channel)
{
	
	return 0;
}

/* this tasklet is call by the general purpose timer */
static void dw_dma_fifo_work(void *data)
{
	struct dma *dma = (struct dma *)data;
	struct dma_pdata *p = dma->data;
	int i, schedule;
	uint32_t cfg;

	/* check any draining channels */
	for (i = 0; i < DW_MAX_CHAN; i++) {

		/* only check channels that are still draining */
		if (!(p->chan[i] & DMA_STATUS_DRAINING))
			continue;

		/* check for FIFO empty */
		cfg = io_reg_read(dma->base + DW_CFG_LOW(i));
		if (cfg & DW_CFG_CH_FIFO_EMPTY) {

			/* disable channel */
			io_reg_update_bits(dma->base + DW_DMA_CHAN_EN,
				CHAN_DISABLE(i), CHAN_DISABLE(i));
			p->chan[i] &= ~DMA_STATUS_DRAINING;
		} else
			schedule = 1;
	}

	/* still waiting on more FIFOs to drain ? */
	if (schedule)
		timer_schedule_work(REEF_SYS_TIMER, dw_dma_fifo_work, dma, 1);
}

static int dw_dma_stop(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma->data;

	/* suspend the channel */
	io_reg_update_bits(dma->base + DW_CFG_LOW(channel),
		DW_CFG_CH_SUSPEND, DW_CFG_CH_SUSPEND);

	p->chan[channel] = DMA_STATUS_DRAINING;
	
	/* FIFO cleanup done by general purpose timer */
	timer_schedule_work(REEF_SYS_TIMER, dw_dma_fifo_work, dma, 1);
	return 0;
}

static int dw_dma_drain(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma->data;

	/* suspend the channel */
	io_reg_update_bits(dma->base + DW_CFG_LOW(channel),
		DW_CFG_CH_SUSPEND | DW_CFG_CH_DRAIN,
		DW_CFG_CH_SUSPEND | DW_CFG_CH_DRAIN);

	p->chan[channel] = DMA_STATUS_DRAINING;

	/* FIFO cleanup done by general purpose timer */
	timer_schedule_work(REEF_SYS_TIMER, dw_dma_fifo_work, dma, 1);
	return 0;
}

static int dw_dma_status(struct dma *dma, int channel,
	struct dma_chan_status *status)
{
	
	return 0;
}

static int dw_dma_set_config(struct dma *dma, int channel,
	struct dma_chan_config *config)
{	
	return 0;
}

static int dw_dma_set_desc(struct dma *dma, int channel,
	struct dma_desc *desc)
{
	return 0;
}

static int dw_dma_pm_context_restore(struct dma *dma)
{
	return 0;
}

static int dw_dma_pm_context_store(struct dma *dma)
{
	return 0;
}

const struct dma_ops dw_dma_ops = {
	.channel_get	= dw_dma_channel_get,
	.channel_put	= dw_dma_channel_put,
	.start		= dw_dma_start,
	.stop		= dw_dma_stop,
	.drain		= dw_dma_drain,
	.status		= dw_dma_status,
	.set_config	= dw_dma_set_config,
	.set_desc	= dw_dma_set_desc,
	.pm_context_restore		= dw_dma_pm_context_restore,
	.pm_context_store		= dw_dma_pm_context_store,
};

