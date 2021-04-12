// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

/*
 * This driver API will only be called by 3 clients in sof :-
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

#include <sof/atomic.h>
#include <sof/audio/component.h>
#include <sof/bit.h>
#include <sof/common.h>
#include <sof/drivers/dw-dma.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/wait.h>
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 298873bc-d532-4d93-a540-95ee6bcf3456 */
DECLARE_SOF_UUID("dw-dma", dw_dma_uuid, 0x298873bc, 0xd532, 0x4d93,
		 0xa5, 0x40, 0x95, 0xee, 0x6b, 0xcf, 0x34, 0x56);

DECLARE_TR_CTX(dwdma_tr, SOF_UUID(dw_dma_uuid), LOG_LEVEL_INFO);

/* pointer data for DW DMA buffer */
struct dw_dma_ptr_data {
	uint32_t current_ptr;
	uint32_t start_ptr;
	uint32_t end_ptr;
	uint32_t buffer_bytes;
};

/* data for each DW DMA channel */
struct dw_dma_chan_data {
	struct dw_lli *lli;
	struct dw_lli *lli_current;
	uint32_t cfg_lo;
	uint32_t cfg_hi;
	struct dw_dma_ptr_data ptr_data;	/* pointer data */
};

/* use array to get burst_elems for specific slot number setting.
 * the relation between msize and burst_elems should be
 * 2 ^ msize = burst_elems
 */
static const uint32_t burst_elems[] = {1, 2, 4, 8};

#if CONFIG_HW_LLI
#define DW_DMA_BUFFER_PERIOD_COUNT	4
#else
#define DW_DMA_BUFFER_PERIOD_COUNT	2
#endif

static int dw_dma_stop(struct dma_chan_data *channel);

static void dw_dma_interrupt_mask(struct dma_chan_data *channel)
{
	/* mask block, transfer and error interrupts for channel */
	dma_reg_write(channel->dma, DW_MASK_TFR, DW_CHAN_MASK(channel->index));
	dma_reg_write(channel->dma, DW_MASK_BLOCK,
		      DW_CHAN_MASK(channel->index));
	dma_reg_write(channel->dma, DW_MASK_ERR, DW_CHAN_MASK(channel->index));
}

static void dw_dma_interrupt_unmask(struct dma_chan_data *channel)
{
	/* unmask block, transfer and error interrupts for channel */
#if CONFIG_HW_LLI
	dma_reg_write(channel->dma, DW_MASK_BLOCK,
		      DW_CHAN_UNMASK(channel->index));
#else
	dma_reg_write(channel->dma, DW_MASK_TFR,
		      DW_CHAN_UNMASK(channel->index));
#endif
	dma_reg_write(channel->dma, DW_MASK_ERR,
		      DW_CHAN_UNMASK(channel->index));
}

static void dw_dma_interrupt_clear(struct dma_chan_data *channel)
{
	/* clear transfer, block, src, dst and error interrupts for channel */
	dma_reg_write(channel->dma, DW_CLEAR_TFR, DW_CHAN(channel->index));
	dma_reg_write(channel->dma, DW_CLEAR_BLOCK, DW_CHAN(channel->index));
	dma_reg_write(channel->dma, DW_CLEAR_SRC_TRAN, DW_CHAN(channel->index));
	dma_reg_write(channel->dma, DW_CLEAR_DST_TRAN, DW_CHAN(channel->index));
	dma_reg_write(channel->dma, DW_CLEAR_ERR, DW_CHAN(channel->index));
}

static uint32_t dw_dma_interrupt_status(struct dma_chan_data *channel)
{
	uint32_t status;

#if CONFIG_HW_LLI
	status = dma_reg_read(channel->dma, DW_STATUS_BLOCK);
#else
	status = dma_reg_read(channel->dma, DW_STATUS_TFR);
#endif

	return status & DW_CHAN(channel->index);
}

static void dw_dma_increment_pointer(struct dw_dma_chan_data *chan, int bytes)
{
	chan->ptr_data.current_ptr += bytes;
	if (chan->ptr_data.current_ptr >= chan->ptr_data.end_ptr)
		chan->ptr_data.current_ptr = chan->ptr_data.start_ptr +
			(chan->ptr_data.current_ptr - chan->ptr_data.end_ptr);
}

#if !CONFIG_HW_LLI
/* reload using LLI data from DMA IRQ cb */
static inline void dw_dma_chan_reload_lli_cb(void *arg, enum notify_id type,
					     void *data)
{
	struct dma_chan_data *channel = data;
	struct dma *dma = channel->dma;
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
	struct dw_lli *lli = dw_chan->lli_current;

	/* only need to reload if this is a block transfer */
	if (!lli || !lli->llp) {
		channel->status = COMP_STATE_PREPARE;
		return;
	}

	/* channel not active */
	if (channel->status != COMP_STATE_ACTIVE)
		return;

	/* channel still transferring previous lli */
	if (dma_reg_read(dma, DW_DMA_CHAN_EN) & DW_CHAN(channel->index))
		return;

	/* get current and next block pointers */
	lli = (struct dw_lli *)lli->llp;
	dw_chan->lli_current = lli;

	/* channel needs to start from scratch, so write SAR and DAR */
	dma_reg_write(dma, DW_SAR(channel->index), lli->sar);
	dma_reg_write(dma, DW_DAR(channel->index), lli->dar);

	/* program CTL_LO and CTL_HI */
	dma_reg_write(dma, DW_CTRL_LOW(channel->index), lli->ctrl_lo);
	dma_reg_write(dma, DW_CTRL_HIGH(channel->index), lli->ctrl_hi);

	/* program CFG_LO and CFG_HI */
	dma_reg_write(dma, DW_CFG_LOW(channel->index), dw_chan->cfg_lo);
	dma_reg_write(dma, DW_CFG_HIGH(channel->index), dw_chan->cfg_hi);

	/* enable the channel */
	dma_reg_write(dma, DW_DMA_CHAN_EN, DW_CHAN_UNMASK(channel->index));
}
#endif

/* allocate next free DMA channel */
static struct dma_chan_data *dw_dma_channel_get(struct dma *dma,
						unsigned int req_chan)
{
	uint32_t flags;
	int i;

	tr_info(&dwdma_tr, "dw_dma_channel_get(): dma %d request channel %d",
		dma->plat_data.id, req_chan);

	spin_lock_irq(&dma->lock, flags);

	/* find first free non draining channel */
	for (i = 0; i < dma->plat_data.channels; i++) {

		/* use channel if it's free */
		if (dma->chan[i].status != COMP_STATE_INIT)
			continue;

		dma->chan[i].status = COMP_STATE_READY;

		atomic_add(&dma->num_channels_busy, 1);
#if !CONFIG_HW_LLI
		notifier_register(&dma->chan[i], &dma->chan[i],
			NOTIFIER_ID_DMA_IRQ, dw_dma_chan_reload_lli_cb, 0);
#endif

		/* return channel */
		spin_unlock_irq(&dma->lock, flags);
		return &dma->chan[i];
	}

	/* DMA controller has no free channels */
	spin_unlock_irq(&dma->lock, flags);
	tr_err(&dwdma_tr, "dw_dma_channel_get(): dma %d no free channels",
	       dma->plat_data.id);

	return NULL;
}

/* channel must not be running when this is called */
static void dw_dma_channel_put_unlocked(struct dma_chan_data *channel)
{
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);

	dw_dma_interrupt_mask(channel);

	/* free the lli allocated by set_config*/
	if (dw_chan->lli) {
		rfree(dw_chan->lli);
		dw_chan->lli = NULL;
	}

	notifier_unregister_all(NULL, channel);

	/* set new state */
	channel->status = COMP_STATE_INIT;
	channel->desc_count = 0;
	dw_chan->ptr_data.current_ptr = 0;
	dw_chan->ptr_data.start_ptr = 0;
	dw_chan->ptr_data.end_ptr = 0;
	dw_chan->ptr_data.buffer_bytes = 0;
	atomic_sub(&channel->dma->num_channels_busy, 1);
}

/* channel must not be running when this is called */
static void dw_dma_channel_put(struct dma_chan_data *channel)
{
	uint32_t flags;

	tr_info(&dwdma_tr, "dw_dma_channel_put(): dma %d channel %d put",
		channel->dma->plat_data.id, channel->index);

	spin_lock_irq(&channel->dma->lock, flags);
	dw_dma_channel_put_unlocked(channel);
	spin_unlock_irq(&channel->dma->lock, flags);
}

static int dw_dma_start(struct dma_chan_data *channel)
{
	struct dma *dma = channel->dma;
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
	struct dw_lli *lli = dw_chan->lli_current;
	uint32_t flags;
	int ret = 0;
#if CONFIG_HW_LLI
	uint32_t words_per_tfr = 0;
#endif

	tr_dbg(&dwdma_tr, "dw_dma_start(): dma %d channel %d start",
	       channel->dma->plat_data.id, channel->index);

	irq_local_disable(flags);

	/* check if channel idle, disabled and ready */
	if ((channel->status != COMP_STATE_PREPARE &&
	     channel->status != COMP_STATE_PAUSED) ||
	    (dma_reg_read(dma, DW_DMA_CHAN_EN) & DW_CHAN(channel->index))) {
		tr_err(&dwdma_tr, "dw_dma_start(): dma %d channel %d not ready ena 0x%x status 0x%x",
		       dma->plat_data.id, channel->index,
		       dma_reg_read(dma, DW_DMA_CHAN_EN),
		       channel->status);
		ret = -EBUSY;
		goto out;
	}

	/* is valid stream */
	if (!dw_chan->lli) {
		tr_err(&dwdma_tr, "dw_dma_start(): dma %d channel %d invalid stream",
		       dma->plat_data.id, channel->index);
		ret = -EINVAL;
		goto out;
	}

#if CONFIG_HW_LLI
	/* LLP mode - write LLP pointer unless in scatter mode */
	dma_reg_write(dma, DW_LLP(channel->index),
		      (lli->ctrl_lo & (DW_CTLL_LLP_D_EN | DW_CTLL_LLP_S_EN)) ?
		      (uint32_t)lli : 0);
#endif
	/* channel needs to start from scratch, so write SAR and DAR */
	dma_reg_write(dma, DW_SAR(channel->index), lli->sar);
	dma_reg_write(dma, DW_DAR(channel->index), lli->dar);

	/* program CTL_LO and CTL_HI */
	dma_reg_write(dma, DW_CTRL_LOW(channel->index), lli->ctrl_lo);
	dma_reg_write(dma, DW_CTRL_HIGH(channel->index), lli->ctrl_hi);

	/* program CFG_LO and CFG_HI */
	dma_reg_write(dma, DW_CFG_LOW(channel->index), dw_chan->cfg_lo);
	dma_reg_write(dma, DW_CFG_HIGH(channel->index), dw_chan->cfg_hi);

#if CONFIG_HW_LLI
	if (lli->ctrl_lo & DW_CTLL_D_SCAT_EN) {
		words_per_tfr = (lli->ctrl_hi & DW_CTLH_BLOCK_TS_MASK) >>
			((lli->ctrl_lo & DW_CTLL_DST_WIDTH_MASK) >>
			DW_CTLL_DST_WIDTH_SHIFT);
		dma_reg_write(dma, DW_DSR(channel->index),
			      DW_DSR_DSC(words_per_tfr) |
			      DW_DSR_DSI(words_per_tfr));
	}
#endif

	/* assign core */
	channel->core = cpu_get_id();

	/* enable linear link position */
	platform_dw_dma_llp_enable(dma, channel);

	/* enable the channel */
	channel->status = COMP_STATE_ACTIVE;
	dma_reg_write(dma, DW_DMA_CHAN_EN, DW_CHAN_UNMASK(channel->index));

out:
	irq_local_enable(flags);

	return ret;
}

static int dw_dma_release(struct dma_chan_data *channel)
{
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
	uint32_t flags;

	tr_info(&dwdma_tr, "dw_dma_release(): dma %d channel %d release",
		channel->dma->plat_data.id, channel->index);

	irq_local_disable(flags);

	/* get next lli for proper release */
	dw_chan->lli_current = (struct dw_lli *)dw_chan->lli_current->llp;

	/* prepare to start */
	dw_dma_stop(channel);

	irq_local_enable(flags);

	return 0;
}

static int dw_dma_pause(struct dma_chan_data *channel)
{
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
	struct dma *dma = channel->dma;
	uint32_t flags;

	tr_info(&dwdma_tr, "dw_dma_pause(): dma %d channel %d pause",
		channel->dma->plat_data.id, channel->index);

	irq_local_disable(flags);

	if (channel->status != COMP_STATE_ACTIVE)
		goto out;

	dma_reg_write(dma, DW_CFG_LOW(channel->index),
		      dw_chan->cfg_lo | DW_CFGL_SUSPEND);

	/* pause the channel */
	channel->status = COMP_STATE_PAUSED;

out:
	irq_local_enable(flags);

	return 0;
}

static int dw_dma_stop(struct dma_chan_data *channel)
{
	struct dma *dma = channel->dma;
	uint32_t flags;

#if CONFIG_HW_LLI || CONFIG_DMA_SUSPEND_DRAIN
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
#endif

#if CONFIG_HW_LLI
	struct dw_lli *lli = dw_chan->lli;
	int i;
#endif
#if CONFIG_DMA_SUSPEND_DRAIN
	int ret;
#endif

	tr_info(&dwdma_tr, "dw_dma_stop(): dma %d channel %d stop",
		dma->plat_data.id, channel->index);

	irq_local_disable(flags);

	if (channel->status != COMP_STATE_ACTIVE &&
	    channel->status != COMP_STATE_PAUSED)
		goto out;

#if CONFIG_DMA_SUSPEND_DRAIN
	/* channel cannot be disabled right away, so first we need to
	 * suspend it and drain the FIFO
	 */
	dma_reg_write(dma, DW_CFG_LOW(channel->index),
		      dw_chan->cfg_lo | DW_CFGL_SUSPEND | DW_CFGL_DRAIN);

	/* now we wait for FIFO to be empty */
	ret = poll_for_register_delay(dma_base(dma) +
				      DW_CFG_LOW(channel->index),
				      DW_CFGL_FIFO_EMPTY,
				      DW_CFGL_FIFO_EMPTY,
				      DW_DMA_TIMEOUT);
	if (ret < 0)
		tr_err(&dwdma_tr, "dw_dma_stop(): dma %d channel %d timeout",
		       dma->plat_data.id, channel->index);
#endif

	dma_reg_write(dma, DW_DMA_CHAN_EN, DW_CHAN_MASK(channel->index));

#if CONFIG_HW_LLI
	for (i = 0; i < channel->desc_count; i++) {
		lli->ctrl_hi &= ~DW_CTLH_DONE(1);
		lli++;
	}

	dcache_writeback_region(dw_chan->lli,
				sizeof(struct dw_lli) * channel->desc_count);
#endif

	/* disable linear link position */
	platform_dw_dma_llp_disable(dma, channel);

	channel->status = COMP_STATE_PREPARE;

out:
	irq_local_enable(flags);

	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int dw_dma_status(struct dma_chan_data *channel,
			 struct dma_chan_status *status,
			 uint8_t direction)
{
	status->state = channel->status;
	status->r_pos = dma_reg_read(channel->dma, DW_SAR(channel->index));
	status->w_pos = dma_reg_read(channel->dma, DW_DAR(channel->index));
	status->timestamp = timer_get_system(timer_get());

	return 0;
}

/* mask address for dma to identify memory space.
 * It is requested by BYT, HSW, BDW. For other
 * platforms, the mask is zero.
 */
static void dw_dma_mask_address(struct dma_sg_elem *sg_elem,
				struct dw_lli *lli_desc, uint32_t direction)
{
	lli_desc->sar = sg_elem->src;
	lli_desc->dar = sg_elem->dest;

	switch (direction) {
	case DMA_DIR_LMEM_TO_HMEM:
	case DMA_DIR_MEM_TO_DEV:
		lli_desc->sar |= PLATFORM_DW_DMA_HOST_MASK;
		break;
	case DMA_DIR_HMEM_TO_LMEM:
	case DMA_DIR_DEV_TO_MEM:
		lli_desc->dar |= PLATFORM_DW_DMA_HOST_MASK;
		break;
	case DMA_DIR_MEM_TO_MEM:
		lli_desc->sar |= PLATFORM_DW_DMA_HOST_MASK;
		lli_desc->dar |= PLATFORM_DW_DMA_HOST_MASK;
		break;
	default:
		break;
	}
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int dw_dma_set_config(struct dma_chan_data *channel,
			     struct dma_sg_config *config)
{
	const struct dw_drv_plat_data *dp =
		channel->dma->plat_data.drv_plat_data;
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
	struct dma_sg_elem *sg_elem;
	struct dw_lli *lli_desc;
	struct dw_lli *lli_desc_head;
	struct dw_lli *lli_desc_tail;
	uint16_t chan_class;
	uint32_t msize = 3;/* default msize */
	uint32_t flags;
	int ret = 0;
	int i;

	tr_dbg(&dwdma_tr, "dw_dma_set_config(): dma %d channel %d config",
	       channel->dma->plat_data.id, channel->index);

	irq_local_disable(flags);

	chan_class = dp->chan[channel->index].class;

	/* default channel config */
	channel->direction = config->direction;
	channel->is_scheduling_source = config->is_scheduling_source;
	channel->period = config->period;
	dw_chan->cfg_lo = DW_CFG_LOW_DEF;
	dw_chan->cfg_hi = DW_CFG_HIGH_DEF;

	if (!config->elem_array.count) {
		tr_err(&dwdma_tr, "dw_dma_set_config(): dma %d channel %d no elems",
		       channel->dma->plat_data.id, channel->index);
		ret = -EINVAL;
		goto out;
	}

	if (config->irq_disabled &&
	    config->elem_array.count < DW_DMA_CFG_NO_IRQ_MIN_ELEMS) {
		tr_err(&dwdma_tr, "dw_dma_set_config(): dma %d channel %d not enough elems for config with irq disabled %d",
		       channel->dma->plat_data.id,
		       channel->index, config->elem_array.count);
		ret = -EINVAL;
		goto out;
	}

	/* do we need to realloc descriptors */
	if (config->elem_array.count != channel->desc_count) {

		channel->desc_count = config->elem_array.count;

		/*
		 * Allocate descriptors for channel. They must be cache-line
		 * size aligned to avoid corrupting adjacent memory when
		 * synchronizing caches. Such corruption has been observed with
		 * Zephyr. A generic fix will be implemented for all SOF DMA
		 * allocations on Zephyr to always force cache-line size
		 * alignment.
		 */
		if (dw_chan->lli)
			rfree(dw_chan->lli);

		dw_chan->lli = rballoc_align(0, SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
					sizeof(struct dw_lli) * channel->desc_count,
					PLATFORM_DCACHE_ALIGN);
		if (!dw_chan->lli) {
			tr_err(&dwdma_tr, "dw_dma_set_config(): dma %d channel %d lli alloc failed",
			       channel->dma->plat_data.id,
			       channel->index);
			ret = -ENOMEM;
			goto out;
		}
	}

	/* initialise descriptors */
	bzero(dw_chan->lli, sizeof(struct dw_lli) * channel->desc_count);
	lli_desc = dw_chan->lli;
	lli_desc_head = dw_chan->lli;
	lli_desc_tail = dw_chan->lli + channel->desc_count - 1;

	/* configure msize if burst_elems is set */
	if (config->burst_elems) {
		for (i = 0; i < ARRAY_SIZE(burst_elems); i++) {
			if (burst_elems[i] == config->burst_elems) {
				msize = i;
				break;
			}
		}
	}

	dw_chan->ptr_data.buffer_bytes = 0;

	/* fill in lli for the elems in the list */
	for (i = 0; i < config->elem_array.count; i++) {
		sg_elem = config->elem_array.elems + i;

		/* write CTL_LO for each lli */
		switch (config->src_width) {
		case 2:
			/* non peripheral copies are optimal using words */
			switch (config->direction) {
			case DMA_DIR_LMEM_TO_HMEM:
			case DMA_DIR_HMEM_TO_LMEM:
			case DMA_DIR_MEM_TO_MEM:
				/* config the src tr width for 32 bit words */
				lli_desc->ctrl_lo |= DW_CTLL_SRC_WIDTH(2);
				break;
			default:
				/* config the src width for 16 bit samples */
				lli_desc->ctrl_lo |= DW_CTLL_SRC_WIDTH(1);
				break;
			}
			break;
		case 4:
			/* config the src tr width for 24, 32 bit samples */
			lli_desc->ctrl_lo |= DW_CTLL_SRC_WIDTH(2);
			break;
		default:
			tr_err(&dwdma_tr, "dw_dma_set_config(): dma %d channel %d invalid src width %d",
			       channel->dma->plat_data.id,
			       channel->index, config->src_width);
			ret = -EINVAL;
			goto out;
		}

		switch (config->dest_width) {
		case 2:
			/* non peripheral copies are optimal using words */
			switch (config->direction) {
			case DMA_DIR_LMEM_TO_HMEM:
			case DMA_DIR_HMEM_TO_LMEM:
			case DMA_DIR_MEM_TO_MEM:
				/* config the dest tr width for 32 bit words */
				lli_desc->ctrl_lo |= DW_CTLL_DST_WIDTH(2);
				break;
			default:
				/* config the dest width for 16 bit samples */
				lli_desc->ctrl_lo |= DW_CTLL_DST_WIDTH(1);
				break;
			}
			break;
		case 4:
			/* config the dest tr width for 24, 32 bit samples */
			lli_desc->ctrl_lo |= DW_CTLL_DST_WIDTH(2);
			break;
		default:
			tr_err(&dwdma_tr, "dw_dma_set_config(): dma %d channel %d invalid dest width %d",
			       channel->dma->plat_data.id,
			       channel->index, config->dest_width);
			ret = -EINVAL;
			goto out;
		}

		lli_desc->ctrl_lo |= DW_CTLL_SRC_MSIZE(msize) |
			DW_CTLL_DST_MSIZE(msize) |
			DW_CTLL_INT_EN; /* enable interrupt */

		/* config the SINC and DINC fields of CTL_LO,
		 * SRC/DST_PER fields of CFG_HI
		 */
		switch (config->direction) {
		case DMA_DIR_LMEM_TO_HMEM:
			lli_desc->ctrl_lo |= DW_CTLL_FC_M2M | DW_CTLL_SRC_INC |
				DW_CTLL_DST_INC;
#if CONFIG_HW_LLI
			lli_desc->ctrl_lo |=
				DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;
#endif
			break;
		case DMA_DIR_HMEM_TO_LMEM:
			lli_desc->ctrl_lo |= DW_CTLL_FC_M2M | DW_CTLL_SRC_INC |
				DW_CTLL_DST_INC;
#if CONFIG_HW_LLI
			lli_desc->ctrl_lo |=
				DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;
#endif
			break;
		case DMA_DIR_MEM_TO_MEM:
			lli_desc->ctrl_lo |= DW_CTLL_FC_M2M | DW_CTLL_SRC_INC |
				DW_CTLL_DST_INC;
#if CONFIG_HW_LLI
			lli_desc->ctrl_lo |=
				DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;
#endif
			break;
		case DMA_DIR_MEM_TO_DEV:
			lli_desc->ctrl_lo |= DW_CTLL_FC_M2P | DW_CTLL_SRC_INC |
				DW_CTLL_DST_FIX;
#if CONFIG_HW_LLI
			lli_desc->ctrl_lo |= DW_CTLL_LLP_S_EN;
			dw_chan->cfg_lo |= DW_CFG_RELOAD_DST;
#endif
			dw_chan->cfg_hi |= DW_CFGH_DST(config->dest_dev);
			platform_dw_dma_llp_config(channel->dma, channel,
						   config->dest_dev);
			break;
		case DMA_DIR_DEV_TO_MEM:
			lli_desc->ctrl_lo |= DW_CTLL_FC_P2M | DW_CTLL_SRC_FIX |
				DW_CTLL_DST_INC;
#if CONFIG_HW_LLI
			if (!config->scatter)
				lli_desc->ctrl_lo |= DW_CTLL_LLP_D_EN;
			else
				/* Use contiguous auto-reload. Line 3 in
				 * table 3-3
				 */
				lli_desc->ctrl_lo |= DW_CTLL_D_SCAT_EN;
			dw_chan->cfg_lo |= DW_CFG_RELOAD_SRC;
#endif
			dw_chan->cfg_hi |= DW_CFGH_SRC(config->src_dev);
			platform_dw_dma_llp_config(channel->dma, channel,
						   config->src_dev);
			break;
		case DMA_DIR_DEV_TO_DEV:
			lli_desc->ctrl_lo |= DW_CTLL_FC_P2P | DW_CTLL_SRC_FIX |
				DW_CTLL_DST_FIX;
#if CONFIG_HW_LLI
			lli_desc->ctrl_lo |=
				DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;
#endif
			dw_chan->cfg_hi |= DW_CFGH_SRC(config->src_dev) |
				DW_CFGH_DST(config->dest_dev);
			break;
		default:
			tr_err(&dwdma_tr, "dw_dma_set_config(): dma %d channel %d invalid direction %d",
			       channel->dma->plat_data.id,
			       channel->index, config->direction);
			ret = -EINVAL;
			goto out;
		}

		dw_dma_mask_address(sg_elem, lli_desc, config->direction);

		if (sg_elem->size > DW_CTLH_BLOCK_TS_MASK) {
			tr_err(&dwdma_tr, "dw_dma_set_config(): dma %d channel %d block size too big %d",
			       channel->dma->plat_data.id,
			       channel->index, sg_elem->size);
			ret = -EINVAL;
			goto out;
		}

		/* set channel class */
		platform_dw_dma_set_class(dw_chan, lli_desc, chan_class);

		/* set transfer size of element */
		platform_dw_dma_set_transfer_size(dw_chan, lli_desc,
						  sg_elem->size);

		dw_chan->ptr_data.buffer_bytes += sg_elem->size;

		/* set next descriptor in list */
		lli_desc->llp = (uint32_t)(lli_desc + 1);

		/* next descriptor */
		lli_desc++;
	}

#if CONFIG_HW_LLI
	dw_chan->cfg_lo |= DW_CFG_CTL_HI_UPD_EN;
#endif

	/* end of list or cyclic buffer */
	if (config->cyclic) {
		lli_desc_tail->llp = (uint32_t)lli_desc_head;
	} else {
		lli_desc_tail->llp = 0;
#if CONFIG_HW_LLI
		lli_desc_tail->ctrl_lo &=
			~(DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN);
#endif
	}

	/* write back descriptors so DMA engine can read them directly */
	dcache_writeback_region(dw_chan->lli,
				sizeof(struct dw_lli) * channel->desc_count);

	channel->status = COMP_STATE_PREPARE;
	dw_chan->lli_current = dw_chan->lli;

	/* initialize pointers */
	dw_chan->ptr_data.start_ptr = DW_DMA_LLI_ADDRESS(dw_chan->lli,
							 channel->direction);
	dw_chan->ptr_data.end_ptr = dw_chan->ptr_data.start_ptr +
				    dw_chan->ptr_data.buffer_bytes;
	dw_chan->ptr_data.current_ptr = dw_chan->ptr_data.start_ptr;

out:
	irq_local_enable(flags);

	return ret;
}

/* restore DMA context after leaving D3 */
static int dw_dma_pm_context_restore(struct dma *dma)
{
	return 0;
}

/* store DMA context before leaving D0 */
static int dw_dma_pm_context_store(struct dma *dma)
{
	/* disable the DMA controller */
	dma_reg_write(dma, DW_DMA_CFG, 0);

	return 0;
}

static void dw_dma_verify_transfer(struct dma_chan_data *channel,
				   struct dma_cb_data *next)
{
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
#if CONFIG_HW_LLI
#if defined __ZEPHYR__
	int i;
#else
	struct dw_lli *lli = platform_dw_dma_lli_get(dw_chan->lli_current);
#endif
	switch (next->status) {
	case DMA_CB_STATUS_END:
		channel->status = COMP_STATE_PREPARE;
		dma_reg_write(channel->dma, DW_DMA_CHAN_EN,
			      DW_CHAN_MASK(channel->index));
		/* fallthrough */
	default:
	/* default action is to clear the DONE bit for all LLI making
	 * sure the cache is coherent between DSP and DMAC.
	 */
#if defined __ZEPHYR__
		dcache_invalidate_region(dw_chan->lli,
					 sizeof(struct dw_lli) * channel->desc_count);

		for (i = 0; i < channel->desc_count; i++)
			dw_chan->lli[i].ctrl_hi &= ~DW_CTLH_DONE(1);

		dcache_writeback_region(dw_chan->lli,
					sizeof(struct dw_lli) * channel->desc_count);
#else
		while (lli->ctrl_hi & DW_CTLH_DONE(1)) {
			lli->ctrl_hi &= ~DW_CTLH_DONE(1);
			dw_chan->lli_current =
				(struct dw_lli *)dw_chan->lli_current->llp;
			lli = platform_dw_dma_lli_get(dw_chan->lli_current);
		}
#endif
		break;
	}
#else
	/* check for reload channel:
	 * next->status is DMA_CB_STATUS_END, stop this dma copy.
	 */
	switch (next->status) {
	case DMA_CB_STATUS_END:
		channel->status = COMP_STATE_PREPARE;
		dw_chan->lli_current =
			(struct dw_lli *)dw_chan->lli_current->llp;
		break;
	default:
		break;
	}
#endif
}

static int dw_dma_copy(struct dma_chan_data *channel, int bytes,
		       uint32_t flags)
{
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
	int ret = 0;
	struct dma_cb_data next = {
		.channel = channel,
		.elem = { .size = bytes },
		.status = DMA_CB_STATUS_END,
	};
	uint32_t irq_flags;

	tr_dbg(&dwdma_tr, "dw_dma_copy(): dma %d channel %d copy",
	       channel->dma->plat_data.id, channel->index);

	notifier_event(channel, NOTIFIER_ID_DMA_COPY,
		       NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));

	if (flags & DMA_COPY_ONE_SHOT) {
		/* for one shot copy start the DMA */
		ret = dw_dma_start(channel);
		if (ret < 0)
			return ret;
	}

	if (flags & DMA_COPY_BLOCKING) {
		/* wait for transfer finish */
		ret = poll_for_register_delay(dma_base(channel->dma) +
					      DW_DMA_CHAN_EN,
					      DW_CHAN(channel->index), 0,
					      DW_DMA_TIMEOUT);
		if (ret < 0)
			return ret;
	}

	dw_dma_verify_transfer(channel, &next);

	/* increment current pointer */
	spin_lock_irq(&channel->dma->lock, irq_flags);
	dw_dma_increment_pointer(dw_chan, bytes);
	spin_unlock_irq(&channel->dma->lock, irq_flags);

	return ret;
}

static int dw_dma_setup(struct dma *dma)
{
	int i;

	/* we cannot config DMAC if DMAC has been already enabled by host */
	if (dma_reg_read(dma, DW_DMA_CFG))
		dma_reg_write(dma, DW_DMA_CFG, 0);

	/* now check that it's 0 */
	for (i = DW_DMA_CFG_TRIES; i > 0; i--)
		if (!dma_reg_read(dma, DW_DMA_CFG))
			break;

	if (!i) {
		tr_err(&dwdma_tr, "dw_dma_setup(): dma %d setup failed",
		       dma->plat_data.id);
		return -EIO;
	}

	for (i = 0; i < dma->plat_data.channels; i++)
		dma_reg_read(dma, DW_DMA_CHAN_EN);

	/* enable the DMA controller */
	dma_reg_write(dma, DW_DMA_CFG, 1);

	/* mask all interrupts for all channels */
	dma_reg_write(dma, DW_MASK_TFR, DW_CHAN_MASK_ALL);
	dma_reg_write(dma, DW_MASK_BLOCK, DW_CHAN_MASK_ALL);
	dma_reg_write(dma, DW_MASK_SRC_TRAN, DW_CHAN_MASK_ALL);
	dma_reg_write(dma, DW_MASK_DST_TRAN, DW_CHAN_MASK_ALL);
	dma_reg_write(dma, DW_MASK_ERR, DW_CHAN_MASK_ALL);

#if CONFIG_DMA_FIFO_PARTITION
	/* allocate FIFO partitions for each channel */
	dma_reg_write(dma, DW_FIFO_PART1_HI,
		      DW_FIFO_CHx(DW_FIFO_SIZE) | DW_FIFO_CHy(DW_FIFO_SIZE));
	dma_reg_write(dma, DW_FIFO_PART1_LO,
		      DW_FIFO_CHx(DW_FIFO_SIZE) | DW_FIFO_CHy(DW_FIFO_SIZE));
	dma_reg_write(dma, DW_FIFO_PART0_HI,
		      DW_FIFO_CHx(DW_FIFO_SIZE) | DW_FIFO_CHy(DW_FIFO_SIZE));
	dma_reg_write(dma, DW_FIFO_PART0_LO,
		      DW_FIFO_CHx(DW_FIFO_SIZE) | DW_FIFO_CHy(DW_FIFO_SIZE) |
		      DW_FIFO_UPD);
#endif

	return 0;
}

static int dw_dma_probe(struct dma *dma)
{
	struct dma_chan_data *chan;
	struct dw_dma_chan_data *dw_chan;
	int ret;
	int i;

	if (dma->chan)
		return -EEXIST; /* already created */

	/* disable dynamic clock gating */
	pm_runtime_get_sync(DW_DMAC_CLK, dma->plat_data.id);

	/* allocate dma channels */
	dma->chan = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			    sizeof(struct dma_chan_data) * dma->plat_data.channels);

	if (!dma->chan) {
		tr_err(&dwdma_tr, "dw_dma_probe(): dma %d allocaction of channels failed",
		       dma->plat_data.id);
		goto out;
	}

	ret = dw_dma_setup(dma);
	if (ret < 0)
		return ret;

	/* init work */
	for (i = 0, chan = dma->chan; i < dma->plat_data.channels;
	     i++, chan++) {
		chan->status = COMP_STATE_INIT;
		chan->dma = dma;
		chan->index = i;
		chan->core = DMA_CORE_INVALID;

		dw_chan = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				  sizeof(*dw_chan));

		if (!dw_chan) {
			tr_err(&dwdma_tr, "dw_dma_probe(): dma %d allocaction of channel %d private data failed",
			       dma->plat_data.id, i);
			goto out;
		}

		dma_chan_set_data(chan, dw_chan);
	}

	/* init number of channels draining */
	atomic_init(&dma->num_channels_busy, 0);

	return 0;

out:
	if (dma->chan) {
		for (i = 0; i < dma->plat_data.channels; i++)
			rfree(dma_chan_get_data(&dma->chan[i]));
		rfree(dma->chan);
		dma->chan = NULL;
	}

	return -ENOMEM;
}

static int dw_dma_remove(struct dma *dma)
{
	int i;

	tr_dbg(&dwdma_tr, "dw_dma_remove(): dma %d remove", dma->plat_data.id);

	pm_runtime_put_sync(DW_DMAC_CLK, dma->plat_data.id);

	for (i = 0; i < dma->plat_data.channels; i++)
		rfree(dma_chan_get_data(&dma->chan[i]));

	rfree(dma->chan);
	dma->chan = NULL;

	return 0;
}

/* capture */
static int dw_dma_avail_data_size(struct dma_chan_data *channel)
{
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
	int32_t read_ptr = dw_chan->ptr_data.current_ptr;
	int32_t write_ptr = dma_reg_read(channel->dma, DW_DAR(channel->index));
	int size;

	size = write_ptr - read_ptr;
	if (size < 0)
		size += dw_chan->ptr_data.buffer_bytes;

	if (!size)
		tr_info(&dwdma_tr, "dw_dma_avail_data_size() size is 0!");

	tr_dbg(&dwdma_tr, "DAR %x reader 0x%x free 0x%x avail 0x%x", write_ptr,
	       read_ptr, dw_chan->ptr_data.buffer_bytes - size, size);

	return size;
}

/* playback */
static int dw_dma_free_data_size(struct dma_chan_data *channel)
{
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
	int32_t read_ptr = dma_reg_read(channel->dma, DW_SAR(channel->index));
	int32_t write_ptr = dw_chan->ptr_data.current_ptr;
	int size;

	size = read_ptr - write_ptr;
	if (size < 0)
		size += dw_chan->ptr_data.buffer_bytes;

	if (!size)
		tr_info(&dwdma_tr, "dw_dma_free_data_size() size is 0!");

	tr_dbg(&dwdma_tr, "SAR %x writer 0x%x free 0x%x avail 0x%x", read_ptr,
	       write_ptr, size, dw_chan->ptr_data.buffer_bytes - size);

	return size;
}

static int dw_dma_get_data_size(struct dma_chan_data *channel,
				uint32_t *avail, uint32_t *free)
{
	struct dw_dma_chan_data *dw_chan = dma_chan_get_data(channel);
	uint32_t flags;
	int ret = 0;

	tr_dbg(&dwdma_tr, "dw_dma_get_data_size(): dma %d channel %d get data size",
	       channel->dma->plat_data.id, channel->index);

	spin_lock_irq(&channel->dma->lock, flags);

	if (channel->direction == DMA_DIR_HMEM_TO_LMEM ||
	    channel->direction == DMA_DIR_DEV_TO_MEM) {
		*avail = dw_dma_avail_data_size(channel);
		*free = dw_chan->ptr_data.buffer_bytes - *avail;
	} else {
		*free = dw_dma_free_data_size(channel);
		*avail = dw_chan->ptr_data.buffer_bytes - *free;
	}
	spin_unlock_irq(&channel->dma->lock, flags);

#if CONFIG_HW_LLI
	if (!(dma_reg_read(channel->dma, DW_DMA_CHAN_EN) &
	      DW_CHAN(channel->index))) {
		tr_err(&dwdma_tr, "dw_dma_get_data_size(): xrun detected");
		return -ENODATA;
	}
#endif

	return ret;
}

static int dw_dma_get_attribute(struct dma *dma, uint32_t type,
				uint32_t *value)
{
	int ret = 0;

	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
		*value = DW_DMA_BUFFER_ALIGNMENT;
		break;
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = DW_DMA_COPY_ALIGNMENT;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = PLATFORM_DCACHE_ALIGN;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = DW_DMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int dw_dma_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	int ret = 0;

	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		ret = dw_dma_interrupt_status(channel);
		break;
	case DMA_IRQ_CLEAR:
		dw_dma_interrupt_clear(channel);
		break;
	case DMA_IRQ_MASK:
		dw_dma_interrupt_mask(channel);
		break;
	case DMA_IRQ_UNMASK:
		dw_dma_interrupt_unmask(channel);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

const struct dma_ops dw_dma_ops = {
	.channel_get		= dw_dma_channel_get,
	.channel_put		= dw_dma_channel_put,
	.start			= dw_dma_start,
	.stop			= dw_dma_stop,
	.pause			= dw_dma_pause,
	.release		= dw_dma_release,
	.copy			= dw_dma_copy,
	.status			= dw_dma_status,
	.set_config		= dw_dma_set_config,
	.pm_context_restore	= dw_dma_pm_context_restore,
	.pm_context_store	= dw_dma_pm_context_store,
	.probe			= dw_dma_probe,
	.remove			= dw_dma_remove,
	.get_data_size		= dw_dma_get_data_size,
	.get_attribute		= dw_dma_get_attribute,
	.interrupt		= dw_dma_interrupt,
};
