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
 *
 * This driver API will only be called by 3 clients in reef :-
 *
 * 1. Host audio component. This component represents the ALSA PCM device
 *    and involves copying data to/from the host ALSA audio buffer to/from the
 *    the DSP buffer.
 *
 * 2. DAI audio component. This component represents physical DAIs and involves
 *    copying data to/from the DSP buffers to/from the DAI FIFOs.
 *
 * 3. IPC Layer. Some IPC needs DMA to copy audio buffer page table information
 *    from the host DRAM into DSP DRAM. This page table information is then
 *    used to construct the DMA configuration for the host client 1 above. 
 */

#include <reef/debug.h>
#include <reef/reef.h>
#include <reef/dma.h>
#include <reef/dw-dma.h>
#include <reef/io.h>
#include <reef/stream.h>
#include <reef/timer.h>
#include <reef/alloc.h>
#include <reef/interrupt.h>
#include <reef/work.h>
#include <reef/lock.h>
#include <reef/trace.h>
#include <platform/dma.h>
#include <platform/platform.h>
#include <platform/interrupt.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

/* channel registers */
#define DW_MAX_CHAN			8
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
#define DW_STATUS_TFR			0x02E8
#define DW_STATUS_BLOCK			0x02F0
#define DW_STATUS_SRC_TRAN		0x02F8
#define DW_STATUS_DST_TRAN		0x0300
#define DW_STATUS_ERR			0x0308
#define DW_RAW_TFR			0x02C0
#define DW_RAW_BLOCK			0x02C8
#define DW_RAW_SRC_TRAN			0x02D0
#define DW_RAW_DST_TRAN			0x02D8
#define DW_RAW_ERR			0x02E0
#define DW_MASK_TFR			0x0310
#define DW_MASK_BLOCK			0x0318
#define DW_MASK_SRC_TRAN		0x0320
#define DW_MASK_DST_TRAN		0x0328
#define DW_MASK_ERR			0x0330
#define DW_CLEAR_TFR			0x0338
#define DW_CLEAR_BLOCK			0x0340
#define DW_CLEAR_SRC_TRAN		0x0348
#define DW_CLEAR_DST_TRAN		0x0350
#define DW_CLEAR_ERR			0x0358
#define DW_INTR_STATUS			0x0360
#define DW_DMA_CFG			0x0398
#define DW_DMA_CHAN_EN			0x03A0
#define DW_FIFO_PART0_LO		0x0400
#define DW_FIFO_PART0_HI		0x0404
#define DW_FIFO_PART1_LO		0x0408
#define DW_FIFO_PART1_HI		0x040C
#define DW_CH_SAI_ERR			0x0410

/* channel bits */
#define INT_MASK(chan)			(0x100 << chan)
#define INT_UNMASK(chan)		(0x101 << chan)
#define CHAN_ENABLE(chan)		(0x101 << chan)
#define CHAN_DISABLE(chan)		(0x100 << chan)

#define DW_CFG_CH_SUSPEND		0x100
#define DW_CFG_CH_DRAIN			0x400
#define DW_CFG_CH_FIFO_EMPTY		0x200

/* CTL_LO */
#define DW_CTLL_INT_EN			(1 << 0)
#define DW_CTLL_DST_WIDTH(x)		(x << 1)
#define DW_CTLL_SRC_WIDTH(x)		(x << 4)
#define DW_CTLL_DST_INC			(0 << 7)
#define DW_CTLL_DST_DEC			(1 << 7)
#define DW_CTLL_DST_FIX			(2 << 7)
#define DW_CTLL_SRC_INC			(0 << 9)
#define DW_CTLL_SRC_DEC			(1 << 9)
#define DW_CTLL_SRC_FIX			(2 << 9)
#define DW_CTLL_DST_MSIZE(x)		(x << 11)
#define DW_CTLL_SRC_MSIZE(x)		(x << 14)
#define DW_CTLL_S_GATH_EN		(1 << 17)
#define DW_CTLL_D_SCAT_EN		(1 << 18)
#define DW_CTLL_FC(x)			(x << 20)
#define DW_CTLL_FC_M2M			(0 << 20)
#define DW_CTLL_FC_M2P			(1 << 20)
#define DW_CTLL_FC_P2M			(2 << 20)
#define DW_CTLL_FC_P2P			(3 << 20)
#define DW_CTLL_DMS(x)			(x << 23)
#define DW_CTLL_SMS(x)			(x << 25)
#define DW_CTLL_LLP_D_EN		(1 << 27)
#define DW_CTLL_LLP_S_EN		(1 << 28)
#define DW_CTLL_RELOAD_SRC		(1 << 30)
#define DW_CTLL_RELOAD_DST		(1 << 31)

/* CTL_HI */
#define DW_CTLH_DONE			0x00020000
#define DW_CTLH_BLOCK_TS_MASK		0x0001ffff

/* CFG_HI */
#define DW_CFGH_SRC_PER(x)		(x << 0)
#define DW_CFGH_DST_PER(x)		(x << 4)

/* data for each DMA channel */
struct dma_chan_data {
	uint8_t status;
	uint8_t direction;
	uint8_t reserved[2];
	struct dw_lli2 *lli;
	int desc_count;
	uint32_t cfg_lo;
	uint32_t cfg_hi;

	void (*cb)(void *data, uint32_t type);	/* client callback function */
	void *cb_data;		/* client callback data */
	int cb_type;		/* callback type */
};

/* private data for DW DMA engine */
struct dma_pdata {
	struct dma_chan_data chan[DW_MAX_CHAN];
	struct work work;
	spinlock_t lock;
};

static inline void dw_write(struct dma *dma, uint32_t reg, uint32_t value)
{
	io_reg_write(dma_base(dma) + reg, value);
}

static inline uint32_t dw_read(struct dma *dma, uint32_t reg)
{
	return io_reg_read(dma_base(dma) + reg);
}

static inline void dw_update_bits(struct dma *dma, uint32_t reg, uint32_t mask,
	uint32_t value)
{
	io_reg_update_bits(dma_base(dma) + reg, mask, value);
}

/* allocate next free DMA channel */
static int dw_dma_channel_get(struct dma *dma)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	int i;

	spin_lock_local_irq(&dma->lock, dma_irq(dma));

	/* find first free non draining channel */
	for (i = 0; i < DW_MAX_CHAN; i++) {

		/* use channel if it's free */
		if (p->chan[i].status != DMA_STATUS_FREE)
			continue;

		p->chan[i].status = DMA_STATUS_IDLE;

		/* unmask block, transfer and error interrupts for channel */
		dw_write(dma, DW_MASK_TFR, INT_UNMASK(i));
		dw_write(dma, DW_MASK_BLOCK, INT_UNMASK(i));
		dw_write(dma, DW_MASK_ERR, INT_UNMASK(i));

		/* return channel */
		spin_unlock_local_irq(&dma->lock, dma_irq(dma));
		return i;
	}

	/* DMAC has no free channels */
	spin_unlock_local_irq(&dma->lock, dma_irq(dma));
	return -ENODEV;
}

/* channel must not be running when this is called */
static void dw_dma_channel_put(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	spin_lock_local_irq(&dma->lock, dma_irq(dma));

	/* channel can only be freed if it's not still draining */
	if (p->chan[channel].status == DMA_STATUS_DRAINING ||
		p->chan[channel].status == DMA_STATUS_PAUSED) {

		/* free channel later */
		p->chan[channel].status = DMA_STATUS_CLOSING;
		work_schedule_default(&p->work, 1000);
		goto out;
	}

	/* mask block, transfer and error interrupts for channel */
	dw_write(dma, DW_MASK_TFR, INT_MASK(channel));
	dw_write(dma, DW_MASK_BLOCK, INT_MASK(channel));
	dw_write(dma, DW_MASK_ERR, INT_MASK(channel));

	/* free the lli allocated by set_config*/
	if (p->chan[channel].lli) {
		rfree(RZONE_MODULE, RMOD_SYS, p->chan[channel].lli);
		p->chan[channel].lli = NULL;
	}

	/* set new state */
	p->chan[channel].status = DMA_STATUS_FREE;
	p->chan[channel].cb = NULL;
	p->chan[channel].desc_count = 0;

out:
	spin_unlock_local_irq(&dma->lock, dma_irq(dma));
}

static int dw_dma_start(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t mask;
	int ret = 0;

	spin_lock_local_irq(&dma->lock, dma_irq(dma));

	/* channel idle */
	if (p->chan[channel].status != DMA_STATUS_IDLE) {
		ret = -EBUSY;
		goto out;
	}

	/* valid stream ? */
	if (p->chan[channel].lli == NULL) {
		ret = -EINVAL;
		goto out;
	}

	/* write interrupt clear registers for the channel:
	ClearTfr, ClearBlock, ClearSrcTran, ClearDstTran, ClearErr*/
	dw_write(dma, DW_CLEAR_TFR, 0x1 << channel);
	dw_write(dma, DW_CLEAR_BLOCK, 0x1 << channel);
	dw_write(dma, DW_CLEAR_SRC_TRAN, 0x1 << channel);
	dw_write(dma, DW_CLEAR_DST_TRAN, 0x1 << channel);
	dw_write(dma, DW_CLEAR_ERR, 0x1 << channel);

	/* clear platform interrupt */
	if (dma->plat_data.irq == IRQ_NUM_EXT_DMAC0)
		mask = 1 << (16 + channel);
	else
		mask = 1 << (24 + channel);
	platform_interrupt_mask_clear(mask);

	/* channel needs started from scratch, so write SARn, DARn */
	dw_write(dma, DW_SAR(channel), p->chan[channel].lli->sar);
	dw_write(dma, DW_DAR(channel), p->chan[channel].lli->dar);
	dw_write(dma, DW_LLP(channel), p->chan[channel].lli->llp);

	/* program CTLn and CFGn*/
	dw_write(dma, DW_CTRL_LOW(channel), p->chan[channel].lli->ctrl_lo);
	dw_write(dma, DW_CTRL_HIGH(channel), p->chan[channel].lli->ctrl_hi);
	dw_write(dma, DW_CFG_LOW(channel), p->chan[channel].cfg_lo);
	dw_write(dma, DW_CFG_HIGH(channel), p->chan[channel].cfg_hi);

	/* enable the channel */
	dw_write(dma, DW_DMA_CHAN_EN, CHAN_ENABLE(channel));
	p->chan[channel].status = DMA_STATUS_RUNNING;

out:
	spin_unlock_local_irq(&dma->lock, dma_irq(dma));
	return ret;
}

static int dw_dma_release(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	spin_lock_local_irq(&dma->lock, dma_irq(dma));

	/* unpause channel */
	dw_update_bits(dma, DW_CFG_LOW(channel), DW_CFG_CH_SUSPEND, 0);
	p->chan[channel].status = DMA_STATUS_RUNNING;

	spin_unlock_local_irq(&dma->lock, dma_irq(dma));
	return 0;
}

static int dw_dma_pause(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	spin_lock_local_irq(&dma->lock, dma_irq(dma));

	dw_update_bits(dma, DW_CFG_LOW(channel),
			DW_CFG_CH_SUSPEND | DW_CFG_CH_DRAIN,
			DW_CFG_CH_SUSPEND | DW_CFG_CH_DRAIN);
	p->chan[channel].status = DMA_STATUS_PAUSED;

	spin_unlock_local_irq(&dma->lock, dma_irq(dma));
	return 0;
}

/*
 * Wait for DMA drain completion using delayed work. This allows the stream
 * IPC to return immediately without blocking the host. This work is called 
 * by the general system timer.
 */
static uint32_t dw_dma_fifo_work(void *data, uint32_t udelay)
{
	struct dma *dma = (struct dma *)data;
	struct dma_pdata *p = dma_get_drvdata(dma);
	int i, schedule = 0;
	uint32_t cfg;

	spin_lock_local_irq(&dma->lock, dma_irq(dma));

	/* check any draining channels */
	for (i = 0; i < DW_MAX_CHAN; i++) {

		/* only check channels that are still draining */
		if (p->chan[i].status != DMA_STATUS_DRAINING ||
			p->chan[i].status != DMA_STATUS_CLOSING)
			continue;

		/* check for FIFO empty */
		cfg = dw_read(dma, DW_CFG_LOW(i));
		if (cfg & DW_CFG_CH_FIFO_EMPTY) {

			/* disable channel */
			dw_write(dma, DW_DMA_CHAN_EN, CHAN_DISABLE(i));

			/* do we need to free it ? */
			if (p->chan[i].status == DMA_STATUS_CLOSING)
				dw_dma_channel_put(dma, i);
			else
				p->chan[i].status = DMA_STATUS_IDLE;
		} else
			schedule = 1;
	}

	spin_unlock_local_irq(&dma->lock, dma_irq(dma));

	/* still waiting on more FIFOs to drain ? */
	if (schedule)
		return 1000;	/* reschedule this work in 1 msec */
	else
		return 0;
}

static int dw_dma_stop(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	spin_lock_local_irq(&dma->lock, dma_irq(dma));

	/* suspend the channel if it's still active */
	if (dw_read(dma, DW_DMA_CHAN_EN) & (0x1 << channel)) {

		dw_update_bits(dma, DW_CFG_LOW(channel),
			DW_CFG_CH_SUSPEND | DW_CFG_CH_DRAIN,
			DW_CFG_CH_SUSPEND | DW_CFG_CH_DRAIN);
	}

	p->chan[channel].status = DMA_STATUS_IDLE;
	spin_unlock_local_irq(&dma->lock, dma_irq(dma));

	return 0;
}

static int dw_dma_drain(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	spin_lock_local_irq(&dma->lock, dma_irq(dma));

	// TODO: this should drain the current avail in the *buffer*.
	// TODO: in llp mode we would NULL terminate the last valid desc.
	p->chan[channel].status = DMA_STATUS_DRAINING;

	spin_unlock_local_irq(&dma->lock, dma_irq(dma));
	/* FIFO cleanup done by general purpose timer */
	work_schedule_default(&p->work, 1000);
	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int dw_dma_status(struct dma *dma, int channel,
	struct dma_chan_status *status, uint8_t direction)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	status->state = p->chan[channel].status;
	status->r_pos = dw_read(dma, DW_SAR(channel));
	status->w_pos = dw_read(dma, DW_DAR(channel));
	status->timestamp = timer_get_system();

	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int dw_dma_set_config(struct dma *dma, int channel,
	struct dma_sg_config *config)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct list_head *plist;
	struct dma_sg_elem *sg_elem;
	struct dw_lli2 *lli_desc, *lli_desc_head, *lli_desc_tail;
	uint32_t desc_count = 0;

	spin_lock_local_irq(&dma->lock, dma_irq(dma));

	/* default channel config */
	p->chan[channel].direction = config->direction;
	p->chan[channel].cfg_lo = 0x00000203;
	p->chan[channel].cfg_hi = 0x0;

	/* get number of SG elems */
	list_for_each(plist, &config->elem_list)
		desc_count++;

	if (desc_count == 0)
		return -EINVAL;

	/* do we need to realloc descriptors */
	if (desc_count != p->chan[channel].desc_count) {

		p->chan[channel].desc_count = desc_count;

		/* allocate descriptors for channel */
		if (p->chan[channel].lli)
			rfree(RZONE_MODULE, RMOD_SYS, p->chan[channel].lli);
		p->chan[channel].lli = rmalloc(RZONE_MODULE, RMOD_SYS,
			sizeof(struct dw_lli2) * p->chan[channel].desc_count);
		if (p->chan[channel].lli == NULL)
			return -ENOMEM;
	}

	/* initialise descriptors */
	bzero(p->chan[channel].lli, sizeof(struct dw_lli2) *
		p->chan[channel].desc_count);
	lli_desc = lli_desc_head = p->chan[channel].lli;
	lli_desc_tail = p->chan[channel].lli + p->chan[channel].desc_count - 1;

	/* fill in lli for the elem in the list */
	list_for_each(plist, &config->elem_list) {

		sg_elem = container_of(plist, struct dma_sg_elem, list);

		lli_desc->ctrl_hi &= ~DW_CTLH_DONE; /* clear the done bit */

		/* write CTL_LOn for each lli */
		lli_desc->ctrl_lo |= DW_CTLL_FC(config->direction); /* config the transfer type */
		lli_desc->ctrl_lo |= DW_CTLL_SRC_WIDTH(2); /* config the src tr width */
		lli_desc->ctrl_lo |= DW_CTLL_DST_WIDTH(2); /* config the dest tr width */
		lli_desc->ctrl_lo |= DW_CTLL_SRC_MSIZE(3); /* config the src msize length 2^2 */
		lli_desc->ctrl_lo |= DW_CTLL_DST_MSIZE(3); /* config the dest msize length 2^2 */
		lli_desc->ctrl_lo |= DW_CTLL_INT_EN; /* enable interrupt */

		/* config the SINC and DINC field of CTL_LOn, SRC/DST_PER filed of CFGn */
		switch (config->direction) {
		case DMA_DIR_MEM_TO_MEM:
			lli_desc->ctrl_lo |= DW_CTLL_SRC_INC | DW_CTLL_DST_INC;
			break;
		case DMA_DIR_MEM_TO_DEV:
			lli_desc->ctrl_lo |= DW_CTLL_SRC_INC | DW_CTLL_DST_FIX;
			p->chan[channel].cfg_hi |=
				DW_CFGH_DST_PER(config->dest_dev);
			break;
		case DMA_DIR_DEV_TO_MEM:
			lli_desc->ctrl_lo |= DW_CTLL_SRC_FIX | DW_CTLL_DST_INC;
			p->chan[channel].cfg_hi |=
				DW_CFGH_SRC_PER(config->src_dev);
			break;
		case DMA_DIR_DEV_TO_DEV:
			lli_desc->ctrl_lo |= DW_CTLL_SRC_FIX | DW_CTLL_DST_FIX;
			p->chan[channel].cfg_hi |=
				DW_CFGH_SRC_PER(config->src_dev) |
				DW_CFGH_DST_PER(config->dest_dev);
			break;
		default:
			break;
		}

		/* set source and destination adddresses */
		lli_desc->sar = (uint32_t)sg_elem->src;
		lli_desc->dar = (uint32_t)sg_elem->dest;

		/* set transfer size of element */
		lli_desc->ctrl_hi |= sg_elem->size & DW_CTLH_BLOCK_TS_MASK;

		/* set next descriptor in list */
		lli_desc->llp = (uint32_t)(lli_desc + 1);
		lli_desc->ctrl_lo |= DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;

		/* next descriptor */
		lli_desc++;
	}

	/* end of list or cyclic buffer ? */
	if (config->cyclic) {
		lli_desc_tail->llp = (uint32_t)lli_desc_head;
		p->chan[channel].cfg_lo |=
			(DW_CTLL_RELOAD_DST | DW_CTLL_RELOAD_SRC);
	} else {
		lli_desc_tail->llp = 0x0;
		lli_desc_tail->ctrl_lo &=
			~(DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN);
	}

	spin_unlock_local_irq(&dma->lock, dma_irq(dma));

	return 0;
}

/* restore DMA conext after leaving D3 */
static int dw_dma_pm_context_restore(struct dma *dma)
{
	return 0;
}

/* store DMA conext after leaving D3 */
static int dw_dma_pm_context_store(struct dma *dma)
{
	return 0;
}

static void dw_dma_set_cb(struct dma *dma, int channel, int type,
		void (*cb)(void *data, uint32_t type), void *data)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	spin_lock_local_irq(&dma->lock, dma_irq(dma));
	p->chan[channel].cb = cb;
	p->chan[channel].cb_data = data;
	p->chan[channel].cb_type = type;
	spin_unlock_local_irq(&dma->lock, dma_irq(dma));
}

/* this will probably be called at the end of every period copied */
static void dw_dma_irq_handler(void *data)
{
	struct dma *dma = (struct dma *)data;
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t status_tfr, status_block, status_err, status_intr, mask;
	int i;

	interrupt_disable(dma_irq(dma));
	
	status_intr = dw_read(dma, DW_INTR_STATUS);
	if (!status_intr)
		goto out;

	/* get the source of our IRQ. */
	status_block = dw_read(dma, DW_STATUS_BLOCK);
	status_tfr = dw_read(dma, DW_STATUS_TFR);

	/* TODO: handle errors, just clear them atm */
	status_err = dw_read(dma, DW_STATUS_ERR);
	dw_write(dma, DW_CLEAR_ERR, status_err);

	for (i = 0; i < DW_MAX_CHAN; i++) {

		if (p->chan[i].cb == NULL)
			continue;

		mask = 0x1 << i;

		/* end of a transfer */
		if (status_tfr & mask &&
			p->chan[i].cb_type & DMA_IRQ_TYPE_LLIST) {
			p->chan[i].cb(p->chan[i].cb_data,
					DMA_IRQ_TYPE_LLIST);
			p->chan[i].status = DMA_STATUS_IDLE;
		}

		/* end of a block */
		if (status_block & mask &&
			p->chan[i].cb_type & DMA_IRQ_TYPE_BLOCK) {
			dbg_val_at(p->chan[i].lli->sar, 20 + i); 	
			p->chan[i].cb(p->chan[i].cb_data,
					DMA_IRQ_TYPE_BLOCK);
		}
	}

	/* clear interrupts */
	dw_write(dma, DW_CLEAR_BLOCK, status_block);
	dw_write(dma, DW_CLEAR_TFR, status_tfr);

out:
	/* we dont use the DSP IRQ clear as we only need to clear the ISR */
	mask = (dma->plat_data.irq == IRQ_NUM_EXT_DMAC0) ? 0x00ff0000 : 0xff000000;
	platform_interrupt_mask_clear(mask);

	interrupt_enable(dma_irq(dma));
}

static void dw_dma_setup(struct dma *dma)
{
	/* enable the DMA controller */
	dw_write(dma, DW_DMA_CFG, 1);

	/* mask all interrupts for all 8 channels */
	dw_write(dma, DW_MASK_TFR, 0x0000ff00);
	dw_write(dma, DW_MASK_BLOCK, 0x0000ff00);
	dw_write(dma, DW_MASK_SRC_TRAN, 0x0000ff00);
	dw_write(dma, DW_MASK_DST_TRAN, 0x0000ff00);
	dw_write(dma, DW_MASK_ERR, 0x0000ff00);

	/* allocate FIFO partitions, 128 bytes for each ch */
	dw_write(dma, DW_FIFO_PART1_LO, 0x100080);
	dw_write(dma, DW_FIFO_PART1_HI, 0x100080);
	dw_write(dma, DW_FIFO_PART0_HI, 0x100080);
	dw_write(dma, DW_FIFO_PART0_LO, 0x100080 | (1 << 26));
	dw_write(dma, DW_FIFO_PART0_LO, 0x100080);
}

static int dw_dma_probe(struct dma *dma)
{
	struct dma_pdata *dw_pdata;

	/* allocate private data */
	dw_pdata = rmalloc(RZONE_DEV, RMOD_SYS, sizeof(*dw_pdata));
	bzero(dw_pdata, sizeof(*dw_pdata));
	dma_set_drvdata(dma, dw_pdata);

	spinlock_init(dw_pdata->lock);
	dw_dma_setup(dma);

	/* init work */
	work_init(&dw_pdata->work, dw_dma_fifo_work, dma, WORK_ASYNC);

	/* register our IRQ handler */
	interrupt_register(dma_irq(dma), dw_dma_irq_handler, dma);
	interrupt_enable(dma_irq(dma));

	return 0;
}

const struct dma_ops dw_dma_ops = {
	.channel_get	= dw_dma_channel_get,
	.channel_put	= dw_dma_channel_put,
	.start		= dw_dma_start,
	.stop		= dw_dma_stop,
	.pause		= dw_dma_pause,
	.release	= dw_dma_release,
	.drain		= dw_dma_drain,
	.status		= dw_dma_status,
	.set_config	= dw_dma_set_config,
	.set_cb		= dw_dma_set_cb,
	.pm_context_restore		= dw_dma_pm_context_restore,
	.pm_context_store		= dw_dma_pm_context_store,
	.probe		= dw_dma_probe,
};

