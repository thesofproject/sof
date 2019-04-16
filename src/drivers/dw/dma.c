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
 *         Keyon Jie <yang.jie@linux.intel.com>
 *
 *
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

#include <config.h>
#include <errno.h>
#include <platform/dma.h>
#include <platform/dw-dma.h>
#include <platform/interrupt.h>
#include <platform/platform.h>
#include <sof/alloc.h>
#include <sof/audio/component.h>
#include <sof/atomic.h>
#include <sof/cpu.h>
#include <sof/debug.h>
#include <sof/dma.h>
#include <sof/dw-dma.h>
#include <sof/interrupt.h>
#include <sof/io.h>
#include <sof/lock.h>
#include <sof/pm_runtime.h>
#include <sof/schedule.h>
#include <sof/sof.h>
#include <sof/stream.h>
#include <sof/timer.h>
#include <sof/trace.h>
#include <sof/wait.h>
#include <sof/string.h>
#include <stdint.h>

/* pointer data for DW DMA buffer */
struct dw_dma_ptr_data {
	uint32_t current_ptr;
	uint32_t start_ptr;
	uint32_t end_ptr;
	uint32_t buffer_bytes;
};

/* data for each DW DMA channel */
struct dw_dma_chan_data {
	uint32_t status;
	uint32_t direction;
	struct dw_lli *lli;
	struct dw_lli *lli_current;
	uint32_t desc_count;
	uint32_t cfg_lo;
	uint32_t cfg_hi;
	bool irq_disabled;

	/* pointer data */
	struct dw_dma_ptr_data ptr_data;

	/* client callback function */
	void (*cb)(void *data, uint32_t type, struct dma_sg_elem *next);
	/* client callback data */
	void *cb_data;
	/* callback type */
	int cb_type;
};

/* private data for DW DMA engine */
struct dma_pdata {
	struct dw_dma_chan_data chan[DW_MAX_CHAN];
};

/* use array to get burst_elems for specific slot number setting.
 * the relation between msize and burst_elems should be
 * 2 ^ msize = burst_elems
 */
static const uint32_t burst_elems[] = {1, 2, 4, 8};

#if !CONFIG_HW_LLI
static inline void dw_dma_chan_reload_lli(struct dma *dma, int channel);
static inline void dw_dma_chan_reload_next(struct dma *dma, int channel,
					   struct dma_sg_elem *next,
					   int direction);
#endif
static inline int dw_dma_interrupt_register(struct dma *dma, int channel);
static inline void dw_dma_interrupt_unregister(struct dma *dma, int channel);
static int dw_dma_copy(struct dma *dma, int channel, int bytes, uint32_t flags);

static inline void dw_write(struct dma *dma, uint32_t reg, uint32_t value)
{
	io_reg_write(dma_base(dma) + reg, value);
}

static inline uint32_t dw_read(struct dma *dma, uint32_t reg)
{
	return io_reg_read(dma_base(dma) + reg);
}

static void dw_dma_interrupt_mask(struct dma *dma, int channel)
{
	const struct dma_pdata *p = dma_get_drvdata(dma);

	if (p->chan[channel].irq_disabled) {
		tracev_dwdma("dw_dma_interrupt_mask(): dma %d channel %d "
			     "not working in irq mode", dma->plat_data.id,
			     channel);
		return;
	}

	/* mask block, transfer and error interrupts for channel */
	dw_write(dma, DW_MASK_TFR, DW_CHAN_MASK(channel));
	dw_write(dma, DW_MASK_BLOCK, DW_CHAN_MASK(channel));
	dw_write(dma, DW_MASK_ERR, DW_CHAN_MASK(channel));
}

static void dw_dma_interrupt_unmask(struct dma *dma, int channel)
{
	const struct dma_pdata *p = dma_get_drvdata(dma);

	if (p->chan[channel].irq_disabled) {
		tracev_dwdma("dw_dma_interrupt_mask(): dma %d channel %d "
			     "not working in irq mode", dma->plat_data.id,
			     channel);
		return;
	}

	/* unmask block, transfer and error interrupts for channel */
#if CONFIG_HW_LLI
	dw_write(dma, DW_MASK_BLOCK, DW_CHAN_UNMASK(channel));
#else
	dw_write(dma, DW_MASK_TFR, DW_CHAN_UNMASK(channel));
#endif
	dw_write(dma, DW_MASK_ERR, DW_CHAN_UNMASK(channel));
}

static void dw_dma_interrupt_clear(struct dma *dma, int channel)
{
	const struct dma_pdata *p = dma_get_drvdata(dma);

	if (p->chan[channel].irq_disabled) {
		tracev_dwdma("dw_dma_interrupt_mask(): dma %d channel %d "
			     "not working in irq mode", dma->plat_data.id,
			     channel);
		return;
	}

	/* clear transfer, block, src, dst and error interrupts for channel */
	dw_write(dma, DW_CLEAR_TFR, DW_CHAN(channel));
	dw_write(dma, DW_CLEAR_BLOCK, DW_CHAN(channel));
	dw_write(dma, DW_CLEAR_SRC_TRAN, DW_CHAN(channel));
	dw_write(dma, DW_CLEAR_DST_TRAN, DW_CHAN(channel));
	dw_write(dma, DW_CLEAR_ERR, DW_CHAN(channel));

	/* clear platform interrupt */
	platform_interrupt_clear(dma_irq(dma, cpu_get_id()),
				 DW_CHAN(channel));
}

/* allocate next free DMA channel */
static int dw_dma_channel_get(struct dma *dma, int req_chan)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;
	int i;

	trace_dwdma("dw_dma_channel_get(): dma %d request channel %d",
		    dma->plat_data.id, req_chan);

	spin_lock_irq(&dma->lock, flags);

	/* find first free non draining channel */
	for (i = 0; i < DW_MAX_CHAN; i++) {

		/* use channel if it's free */
		if (p->chan[i].status != COMP_STATE_INIT)
			continue;

		p->chan[i].status = COMP_STATE_READY;

		atomic_add(&dma->num_channels_busy, 1);

		/* return channel */
		spin_unlock_irq(&dma->lock, flags);
		return i;
	}

	/* DMA controller has no free channels */
	spin_unlock_irq(&dma->lock, flags);
	trace_dwdma_error("dw_dma_channel_get() error: dma %d "
			  "no free channels", dma->plat_data.id);

	return -ENODEV;
}

/* channel must not be running when this is called */
static void dw_dma_channel_put_unlocked(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;

	if (channel >= dma->plat_data.channels ||
	    channel == DMA_CHAN_INVALID) {
		trace_dwdma_error("dw_dma_channel_put_unlocked() error: "
				  "dma %d invalid channel %d",
				  dma->plat_data.id, channel);
		return;
	}

	dw_dma_interrupt_mask(dma, channel);

	/* free the lli allocated by set_config*/
	if (chan->lli) {
		rfree(chan->lli);
		chan->lli = NULL;
	}

	/* set new state */
	chan->status = COMP_STATE_INIT;
	chan->cb = NULL;
	chan->desc_count = 0;
	chan->ptr_data.current_ptr = 0;
	chan->ptr_data.start_ptr = 0;
	chan->ptr_data.end_ptr = 0;
	chan->ptr_data.buffer_bytes = 0;
	atomic_sub(&dma->num_channels_busy, 1);
}

/* channel must not be running when this is called */
static void dw_dma_channel_put(struct dma *dma, int channel)
{
	uint32_t flags;

	trace_dwdma("dw_dma_channel_put(): dma %d channel %d put",
		    dma->plat_data.id, channel);

	spin_lock_irq(&dma->lock, flags);
	dw_dma_channel_put_unlocked(dma, channel);
	spin_unlock_irq(&dma->lock, flags);
}

static int dw_dma_start(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	struct dw_lli *lli = chan->lli_current;
	uint32_t flags;
	int ret = 0;
#if CONFIG_HW_LLI
	uint32_t words_per_tfr = 0;
#endif

	if (channel >= dma->plat_data.channels ||
	    channel == DMA_CHAN_INVALID) {
		trace_dwdma_error("dw_dma_start() error: dma %d invalid "
				  "channel %d", dma->plat_data.id, channel);
		return -EINVAL;
	}

	tracev_dwdma("dw_dma_start(): dma %d channel %d start",
		     dma->plat_data.id, channel);

	spin_lock_irq(&dma->lock, flags);

	/* check if channel idle, disabled and ready */
	if (chan->status != COMP_STATE_PREPARE ||
	    (dw_read(dma, DW_DMA_CHAN_EN) & DW_CHAN(channel))) {
		trace_dwdma_error("dw_dma_start() error: dma %d channel %d "
				  "not ready ena 0x%x status 0x%x",
				  dma->plat_data.id, channel,
				  dw_read(dma, DW_DMA_CHAN_EN),
				  chan->status);
		ret = -EBUSY;
		goto out;
	}

	/* is valid stream */
	if (!chan->lli) {
		trace_dwdma_error("dw_dma_start() error: dma %d channel %d "
				  "invalid stream", dma->plat_data.id,
				  channel);
		ret = -EINVAL;
		goto out;
	}

	dw_dma_interrupt_clear(dma, channel);

#if CONFIG_HW_LLI
	/* LLP mode - write LLP pointer unless in scatter mode */
	dw_write(dma, DW_LLP(channel), lli->ctrl_lo &
		 (DW_CTLL_LLP_D_EN | DW_CTLL_LLP_S_EN) ?
		 (uint32_t)lli : 0);
#endif
	/* channel needs to start from scratch, so write SAR and DAR */
	dw_write(dma, DW_SAR(channel), lli->sar);
	dw_write(dma, DW_DAR(channel), lli->dar);

	/* program CTL_LO and CTL_HI */
	dw_write(dma, DW_CTRL_LOW(channel), lli->ctrl_lo);
	dw_write(dma, DW_CTRL_HIGH(channel), lli->ctrl_hi);

	/* program CFG_LO and CFG_HI */
	dw_write(dma, DW_CFG_LOW(channel), chan->cfg_lo);
	dw_write(dma, DW_CFG_HIGH(channel), chan->cfg_hi);

#if CONFIG_HW_LLI
	if (lli->ctrl_lo & DW_CTLL_D_SCAT_EN) {
		words_per_tfr = (lli->ctrl_hi & DW_CTLH_BLOCK_TS_MASK) >>
			((lli->ctrl_lo & DW_CTLL_DST_WIDTH_MASK) >>
			DW_CTLL_DST_WIDTH_SHIFT);
		dw_write(dma, DW_DSR(channel), DW_DSR_DSC(words_per_tfr) |
			 DW_DSR_DSI(words_per_tfr));
	}
#endif

	/* enable interrupt only for the first start */
	if (chan->status == COMP_STATE_PREPARE)
		ret = dw_dma_interrupt_register(dma, channel);

	if (!ret) {
		/* enable the channel */
		chan->status = COMP_STATE_ACTIVE;
		dw_write(dma, DW_DMA_CHAN_EN, DW_CHAN_UNMASK(channel));
	}

out:
	spin_unlock_irq(&dma->lock, flags);

	return ret;
}

static int dw_dma_release(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	uint32_t next_ptr;
	uint32_t bytes_left;
	uint32_t flags;

	if (channel >= dma->plat_data.channels ||
	    channel == DMA_CHAN_INVALID) {
		trace_dwdma_error("dw_dma_release() error: dma %d invalid "
				  "channel %d", dma->plat_data.id, channel);
		return -EINVAL;
	}

	trace_dwdma("dw_dma_release(): dma %d channel %d release",
		    dma->plat_data.id, channel);

	spin_lock_irq(&dma->lock, flags);

	/* get next lli for proper release */
	chan->lli_current = (struct dw_lli *)chan->lli_current->llp;

	/* copy leftover data between current and last lli */
	next_ptr = DW_DMA_LLI_ADDRESS(chan->lli_current, chan->direction);
	if (next_ptr >= chan->ptr_data.current_ptr)
		bytes_left = next_ptr - chan->ptr_data.current_ptr;
	else
		/* pointer wrap */
		bytes_left =
			(chan->ptr_data.end_ptr - chan->ptr_data.current_ptr) +
			(next_ptr - chan->ptr_data.start_ptr);

	dw_dma_copy(dma, channel, bytes_left, 0);

	spin_unlock_irq(&dma->lock, flags);

	return 0;
}

static int dw_dma_pause(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	uint32_t flags;

	if (channel >= dma->plat_data.channels ||
	    channel == DMA_CHAN_INVALID) {
		trace_dwdma_error("dw_dma_pause() error: dma %d invalid "
				  "channel %d", dma->plat_data.id, channel);
		return -EINVAL;
	}

	trace_dwdma("dw_dma_pause(): dma %d channel %d pause",
		    dma->plat_data.id, channel);

	spin_lock_irq(&dma->lock, flags);

	if (chan->status != COMP_STATE_ACTIVE)
		goto out;

	/* pause the channel */
	chan->status = COMP_STATE_PAUSED;

out:
	spin_unlock_irq(&dma->lock, flags);

	return 0;
}

static int dw_dma_stop(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	uint32_t flags;
#if CONFIG_HW_LLI
	struct dw_lli *lli = chan->lli;
	int i;
#endif
#if CONFIG_DMA_SUSPEND_DRAIN
	int ret;
#endif

	if (channel >= dma->plat_data.channels ||
	    channel == DMA_CHAN_INVALID) {
		trace_dwdma_error("dw_dma_stop() error: dma %d invalid "
				  "channel %d", dma->plat_data.id, channel);
		return -EINVAL;
	}

	trace_dwdma("dw_dma_stop(): dma %d channel %d stop",
		    dma->plat_data.id, channel);

	spin_lock_irq(&dma->lock, flags);

#if CONFIG_DMA_SUSPEND_DRAIN
	/* channel cannot be disabled right away, so first we need to
	 * suspend it and drain the FIFO
	 */
	dw_write(dma, DW_CFG_LOW(channel), chan->cfg_lo | DW_CFGL_SUSPEND |
		 DW_CFGL_DRAIN);

	/* now we wait for FIFO to be empty */
	ret = poll_for_register_delay(dma_base(dma) + DW_CFG_LOW(channel),
				      DW_CFGL_FIFO_EMPTY,
				      DW_CFGL_FIFO_EMPTY,
				      PLATFORM_DMA_TIMEOUT);
	if (ret < 0)
		trace_dwdma_error("dw_dma_stop() error: dma %d channel %d "
				  "timeout", dma->plat_data.id, channel);
#endif

	dw_write(dma, DW_DMA_CHAN_EN, DW_CHAN_MASK(channel));

	/* disable interrupt */
	dw_dma_interrupt_unregister(dma, channel);

#if CONFIG_HW_LLI
	for (i = 0; i < chan->desc_count; i++) {
		lli->ctrl_hi &= ~DW_CTLH_DONE(1);
		lli++;
	}

	dcache_writeback_region(chan->lli,
				sizeof(struct dw_lli) * chan->desc_count);
#endif

	chan->status = COMP_STATE_PREPARE;

	spin_unlock_irq(&dma->lock, flags);

	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int dw_dma_status(struct dma *dma, int channel,
			 struct dma_chan_status *status,
			 uint8_t direction)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;

	if (channel >= dma->plat_data.channels ||
	    channel == DMA_CHAN_INVALID) {
		trace_dwdma_error("dw_dma_status() error: dma %d invalid "
				  "channel %d", dma->plat_data.id, channel);
		return -EINVAL;
	}

	status->state = chan->status;
	status->r_pos = dw_read(dma, DW_SAR(channel));
	status->w_pos = dw_read(dma, DW_DAR(channel));
	status->timestamp = timer_get_system(platform_timer);

	return 0;
}

/* mask address for dma to identify memory space.
 * It is requested by BYT, HSW, BDW. For other
 * platforms, the mask is zero.
 */
static void dw_dma_mask_address(struct dma_sg_elem *sg_elem, uint32_t *sar,
				uint32_t *dar, uint32_t direction)
{
	*sar = sg_elem->src;
	*dar = sg_elem->dest;

	switch (direction) {
	case DMA_DIR_LMEM_TO_HMEM:
	case DMA_DIR_MEM_TO_DEV:
		*sar |= PLATFORM_HOST_DMA_MASK;
		break;
	case DMA_DIR_HMEM_TO_LMEM:
	case DMA_DIR_DEV_TO_MEM:
		*dar |= PLATFORM_HOST_DMA_MASK;
		break;
	case DMA_DIR_MEM_TO_MEM:
		*sar |= PLATFORM_HOST_DMA_MASK;
		*dar |= PLATFORM_HOST_DMA_MASK;
		break;
	default:
		break;
	}
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int dw_dma_set_config(struct dma *dma, int channel,
			     struct dma_sg_config *config)
{
	struct dw_drv_plat_data *dp = dma->plat_data.drv_plat_data;
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	struct dma_sg_elem *sg_elem;
	struct dw_lli *lli_desc;
	struct dw_lli *lli_desc_head;
	struct dw_lli *lli_desc_tail;
	uint16_t chan_class;
	uint32_t msize = 3;/* default msize */
	uint32_t flags;
	int ret = 0;
	int i;

	if (channel >= dma->plat_data.channels ||
	    channel == DMA_CHAN_INVALID) {
		trace_dwdma_error("dw_dma_set_config() error: dma %d invalid "
				  "channel %d", dma->plat_data.id, channel);
		return -EINVAL;
	}

	tracev_dwdma("dw_dma_set_config(): dma %d channel %d config",
		     dma->plat_data.id, channel);

	spin_lock_irq(&dma->lock, flags);

	chan_class = dp->chan[channel].class;

	/* default channel config */
	chan->direction = config->direction;
	chan->irq_disabled = config->irq_disabled;
	chan->cfg_lo = DW_CFG_LOW_DEF;
	chan->cfg_hi = DW_CFG_HIGH_DEF;

	if (!config->elem_array.count) {
		trace_dwdma_error("dw_dma_set_config() error: dma %d "
				  "channel %d no elems", dma->plat_data.id,
				  channel);
		ret = -EINVAL;
		goto out;
	}

	if (config->irq_disabled &&
	    config->elem_array.count < DW_DMA_CFG_NO_IRQ_MIN_ELEMS) {
		trace_dwdma_error("dw_dma_set_config() error: dma %d channel "
				  "%d not enough elems for config with irq "
				  "disabled %d", dma->plat_data.id, channel,
				  config->elem_array.count);
		ret = -EINVAL;
		goto out;
	}

	/* do we need to realloc descriptors */
	if (config->elem_array.count != chan->desc_count) {

		chan->desc_count = config->elem_array.count;

		/* allocate descriptors for channel */
		if (chan->lli)
			rfree(chan->lli);

		chan->lli = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM |
				    SOF_MEM_CAPS_DMA, sizeof(struct dw_lli) *
				    chan->desc_count);
		if (!chan->lli) {
			trace_dwdma_error("dw_dma_set_config() error: dma %d "
					  "channel %d lli alloc failed",
					  dma->plat_data.id, channel);
			ret = -ENOMEM;
			goto out;
		}
	}

	/* initialise descriptors */
	bzero(chan->lli, sizeof(struct dw_lli) * chan->desc_count);
	lli_desc = chan->lli;
	lli_desc_head = chan->lli;
	lli_desc_tail = chan->lli + chan->desc_count - 1;

	/* configure msize if burst_elems is set */
	if (config->burst_elems) {
		for (i = 0; i < ARRAY_SIZE(burst_elems); i++) {
			if (burst_elems[i] == config->burst_elems) {
				msize = i;
				break;
			}
		}
	}

	dw_dma_interrupt_unmask(dma, channel);

	chan->ptr_data.buffer_bytes = 0;

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
			trace_dwdma_error("dw_dma_set_config() error: dma %d "
					  "channel %d invalid src width %d",
					  dma->plat_data.id, channel,
					  config->src_width);
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
			trace_dwdma_error("dw_dma_set_config() error: dma %d "
					  "channel %d invalid dest width %d",
					  dma->plat_data.id, channel,
					  config->dest_width);
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
			chan->cfg_lo |= DW_CFG_RELOAD_DST;
#endif
			chan->cfg_hi |= DW_CFGH_DST(config->dest_dev);
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
			chan->cfg_lo |= DW_CFG_RELOAD_SRC;
#endif
			chan->cfg_hi |= DW_CFGH_SRC(config->src_dev);
			break;
		case DMA_DIR_DEV_TO_DEV:
			lli_desc->ctrl_lo |= DW_CTLL_FC_P2P | DW_CTLL_SRC_FIX |
				DW_CTLL_DST_FIX;
#if CONFIG_HW_LLI
			lli_desc->ctrl_lo |=
				DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;
#endif
			chan->cfg_hi |= DW_CFGH_SRC(config->src_dev) |
				DW_CFGH_DST(config->dest_dev);
			break;
		default:
			trace_dwdma_error("dw_dma_set_config() error: dma %d "
					  "channel %d invalid direction %d",
					  dma->plat_data.id, channel,
					  config->direction);
			ret = -EINVAL;
			goto out;
		}

		dw_dma_mask_address(sg_elem, &lli_desc->sar, &lli_desc->dar,
				    config->direction);

		if (sg_elem->size > DW_CTLH_BLOCK_TS_MASK) {
			trace_dwdma_error("dw_dma_set_config() error: dma %d "
					  "channel %d block size too big %d",
					  dma->plat_data.id, channel,
					  sg_elem->size);
			ret = -EINVAL;
			goto out;
		}

		/* set channel class */
		platform_dw_dma_set_class(chan, lli_desc, chan_class);

		/* set transfer size of element */
		platform_dw_dma_set_transfer_size(chan, lli_desc,
						  sg_elem->size);

		chan->ptr_data.buffer_bytes += sg_elem->size;

		/* set next descriptor in list */
		lli_desc->llp = (uint32_t)(lli_desc + 1);

		/* next descriptor */
		lli_desc++;
	}

#if CONFIG_HW_LLI
	chan->cfg_lo |= DW_CFG_CTL_HI_UPD_EN;
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
	dcache_writeback_region(chan->lli,
				sizeof(struct dw_lli) * chan->desc_count);

	chan->status = COMP_STATE_PREPARE;
	chan->lli_current = chan->lli;

	/* initialize pointers */
	chan->ptr_data.start_ptr = DW_DMA_LLI_ADDRESS(chan->lli,
						      chan->direction);
	chan->ptr_data.end_ptr = chan->ptr_data.start_ptr +
		chan->ptr_data.buffer_bytes;
	chan->ptr_data.current_ptr = chan->ptr_data.start_ptr;

out:
	spin_unlock_irq(&dma->lock, flags);

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
	dw_write(dma, DW_DMA_CFG, 0);

	return 0;
}

static int dw_dma_set_cb(struct dma *dma, int channel, int type,
			 void (*cb)(void *data, uint32_t type,
				    struct dma_sg_elem *next),
			 void *data)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);
	chan->cb = cb;
	chan->cb_data = data;
	chan->cb_type = type;
	spin_unlock_irq(&dma->lock, flags);

	return 0;
}

#if !CONFIG_HW_LLI
/* reload using LLI data */
static inline void dw_dma_chan_reload_lli(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	struct dw_lli *lli = chan->lli_current;

	/* only need to reload if this is a block transfer */
	if (!lli || !lli->llp) {
		chan->status = COMP_STATE_PREPARE;
		return;
	}

	/* get current and next block pointers */
	lli = (struct dw_lli *)lli->llp;
	chan->lli_current = lli;

	/* channel needs to start from scratch, so write SAR and DAR */
	dw_write(dma, DW_SAR(channel), lli->sar);
	dw_write(dma, DW_DAR(channel), lli->dar);

	/* program CTL_LO and CTL_HI */
	dw_write(dma, DW_CTRL_LOW(channel), lli->ctrl_lo);
	dw_write(dma, DW_CTRL_HIGH(channel), lli->ctrl_hi);

	/* program CFG_LO and CFG_HI */
	dw_write(dma, DW_CFG_LOW(channel), chan->cfg_lo);
	dw_write(dma, DW_CFG_HIGH(channel), chan->cfg_hi);

	/* enable the channel */
	dw_write(dma, DW_DMA_CHAN_EN, DW_CHAN_UNMASK(channel));
}

/* reload using callback data */
static inline void dw_dma_chan_reload_next(struct dma *dma, int channel,
					   struct dma_sg_elem *next,
					   int direction)
{
	struct dw_drv_plat_data *dp = dma->plat_data.drv_plat_data;
	uint16_t class = dp->chan[channel].class;
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	struct dw_lli *lli = chan->lli_current;
	uint32_t sar;
	uint32_t dar;

	dw_dma_mask_address(next, &sar, &dar, direction);

	/* channel needs to start from scratch, so write SAR and DAR */
	dw_write(dma, DW_SAR(channel), sar);
	dw_write(dma, DW_DAR(channel), dar);

	/* set channel class */
	platform_dw_dma_set_class(chan, lli, class);

	/* set transfer size of element */
	platform_dw_dma_set_transfer_size(chan, lli, next->size);

	/* program CTL_LO and CTL_HI */
	dw_write(dma, DW_CTRL_LOW(channel), lli->ctrl_lo);
	dw_write(dma, DW_CTRL_HIGH(channel), lli->ctrl_hi);

	/* program CFG_LO and CFG_HI */
	dw_write(dma, DW_CFG_LOW(channel), chan->cfg_lo);
	dw_write(dma, DW_CFG_HIGH(channel), chan->cfg_hi);

	/* enable the channel */
	dw_write(dma, DW_DMA_CHAN_EN, DW_CHAN_UNMASK(channel));
}
#endif

static void dw_dma_verify_transfer(struct dma *dma, int channel,
				   struct dma_sg_elem *next)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
#if CONFIG_HW_LLI
	struct dw_lli *ll_uncached = cache_to_uncache(chan->lli_current);

	switch (next->size) {
	case DMA_RELOAD_END:
		chan->status = COMP_STATE_PREPARE;
		dw_write(dma, DW_DMA_CHAN_EN, DW_CHAN_MASK(channel));
		/* fallthrough */
	default:
		if (ll_uncached->ctrl_hi & DW_CTLH_DONE(1)) {
			ll_uncached->ctrl_hi &= ~DW_CTLH_DONE(1);
			chan->lli_current =
				(struct dw_lli *)chan->lli_current->llp;
		}
		break;
	}
#else
	/* check for reload channel:
	 * next.size is DMA_RELOAD_END, stop this dma copy;
	 * next.size > 0 but not DMA_RELOAD_LLI, use next
	 * element for next copy;
	 * if we are waiting for pause, pause it;
	 * otherwise, reload lli
	 */
	switch (next->size) {
	case DMA_RELOAD_END:
		chan->status = COMP_STATE_PREPARE;
		chan->lli_current = (struct dw_lli *)chan->lli_current->llp;
		break;
	case DMA_RELOAD_LLI:
		dw_dma_chan_reload_lli(dma, channel);
		break;
	default:
		dw_dma_chan_reload_next(dma, channel, next, chan->direction);
		break;
	}
#endif
}

static void dw_dma_irq_callback(struct dma *dma, int channel,
				struct dma_sg_elem *next, uint32_t type)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;

	if (channel >= dma->plat_data.channels ||
	    channel == DMA_CHAN_INVALID) {
		trace_dwdma_error("dw_dma_irq_callback() error: dma %d invalid"
				  " channel %d", dma->plat_data.id, channel);
		return;
	}

	if (chan->cb && chan->cb_type & type)
		chan->cb(chan->cb_data, type, next);

	if (next->size != DMA_RELOAD_IGNORE)
		dw_dma_verify_transfer(dma, channel, next);
}

static int dw_dma_copy(struct dma *dma, int channel, int bytes, uint32_t flags)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	struct dma_sg_elem next = {
		.src = DMA_RELOAD_LLI,
		.dest = DMA_RELOAD_LLI,
		.size = bytes
	};

	tracev_dwdma("dw_dma_copy(): dma %d channel %d copy",
		     dma->plat_data.id, channel);

	dw_dma_irq_callback(dma, channel, &next, DMA_CB_TYPE_COPY);

	/* change current pointer */
	chan->ptr_data.current_ptr += bytes;
	if (chan->ptr_data.current_ptr >= chan->ptr_data.end_ptr)
		chan->ptr_data.current_ptr = chan->ptr_data.start_ptr +
			(chan->ptr_data.current_ptr - chan->ptr_data.end_ptr);

	return 0;
}

/* interrupt handler for DMA */
static void dw_dma_irq_handler(void *data)
{
	struct dma *dma = data;
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t status_intr;
	uint32_t status_err;
	uint32_t status_src;
	uint32_t mask;
	int i;
	struct dma_sg_elem next = {
		.src = DMA_RELOAD_LLI,
		.dest = DMA_RELOAD_LLI,
		.size = DMA_RELOAD_LLI
	};

	status_intr = dw_read(dma, DW_INTR_STATUS);
	if (!status_intr)
		return;

	tracev_dwdma("dw_dma_irq_handler(): dma %d IRQ status 0x%x",
		     dma->plat_data.id, status_intr);

	/* get the source of our IRQ and clear it */
#if CONFIG_HW_LLI
	status_src = dw_read(dma, DW_STATUS_BLOCK);
	dw_write(dma, DW_CLEAR_BLOCK, status_src);
#else
	status_src = dw_read(dma, DW_STATUS_TFR);
	dw_write(dma, DW_CLEAR_TFR, status_src);
#endif

	/* TODO: handle errors, just clear them atm */
	status_err = dw_read(dma, DW_STATUS_ERR);
	if (status_err) {
		trace_dwdma_error("dw_dma_irq_handler() error: dma %d status "
				  "error 0x%x", dma->plat_data.id, status_err);
		dw_write(dma, DW_CLEAR_ERR, status_err);
	}

	/* clear platform and DSP interrupt */
	platform_interrupt_clear(dma_irq(dma, cpu_get_id()),
				 status_src | status_err);

	for (i = 0; i < dma->plat_data.channels; i++) {
		/* skip if channel is not running */
		if (p->chan[i].status != COMP_STATE_ACTIVE)
			continue;

		mask = 0x1 << i;

		if (status_src & mask)
			dw_dma_irq_callback(dma, i, &next, DMA_CB_TYPE_IRQ);
	}
}

static inline int dw_dma_interrupt_register(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t irq = dma_chan_irq(dma, cpu_get_id(), channel);
	int ret;

	if (p->chan[channel].irq_disabled) {
		tracev_dwdma("dw_dma_interrupt_register(): dma %d channel %d "
			     "not working in irq mode", dma->plat_data.id,
			     channel);
		return 0;
	}

#if CONFIG_DMA_AGGREGATED_IRQ
	if (!dma->mask_irq_channels) {
#endif
		ret = interrupt_register(irq, IRQ_AUTO_UNMASK,
					 dw_dma_irq_handler, dma);
		if (ret < 0) {
			trace_dwdma_error("dw_dma_interrupt_register() error: "
					  "dma %d channel %d failed to "
					  "allocate IRQ", dma->plat_data.id,
					  channel);
			return ret;
		}

		interrupt_enable(irq);
#if CONFIG_DMA_AGGREGATED_IRQ
	}

	dma->mask_irq_channels |= BIT(channel);
#endif

	return 0;
}

static inline void dw_dma_interrupt_unregister(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t irq = dma_chan_irq(dma, cpu_get_id(), channel);

	if (p->chan[channel].irq_disabled) {
		tracev_dwdma("dw_dma_interrupt_unregister(): dma %d channel %d"
			     " not working in irq mode", dma->plat_data.id,
			     channel);
		return;
	}

#if CONFIG_DMA_AGGREGATED_IRQ
	dma->mask_irq_channels &= ~BIT(channel);

	if (!dma->mask_irq_channels) {
#endif
		interrupt_disable(irq);
		interrupt_unregister(irq);
#if CONFIG_DMA_AGGREGATED_IRQ
	}
#endif
}

static int dw_dma_setup(struct dma *dma)
{
	int i;

	/* we cannot config DMAC if DMAC has been already enabled by host */
	if (dw_read(dma, DW_DMA_CFG))
		dw_write(dma, DW_DMA_CFG, 0);

	/* now check that it's 0 */
	for (i = DW_DMA_CFG_TRIES; i > 0; i--)
		if (!dw_read(dma, DW_DMA_CFG))
			break;

	if (!i) {
		trace_dwdma_error("dw_dma_setup(): dma %d setup failed",
				  dma->plat_data.id);
		return -EIO;
	}

	for (i = 0; i < DW_MAX_CHAN; i++)
		dw_read(dma, DW_DMA_CHAN_EN);

	/* enable the DMA controller */
	dw_write(dma, DW_DMA_CFG, 1);

	/* mask all interrupts for all channels */
	dw_write(dma, DW_MASK_TFR, DW_CHAN_MASK_ALL);
	dw_write(dma, DW_MASK_BLOCK, DW_CHAN_MASK_ALL);
	dw_write(dma, DW_MASK_SRC_TRAN, DW_CHAN_MASK_ALL);
	dw_write(dma, DW_MASK_DST_TRAN, DW_CHAN_MASK_ALL);
	dw_write(dma, DW_MASK_ERR, DW_CHAN_MASK_ALL);

#if CONFIG_DMA_FIFO_PARTITION
	/* allocate FIFO partitions for each channel */
	dw_write(dma, DW_FIFO_PART1_HI,
		 DW_FIFO_CHx(DW_FIFO_SIZE) | DW_FIFO_CHy(DW_FIFO_SIZE));
	dw_write(dma, DW_FIFO_PART1_LO,
		 DW_FIFO_CHx(DW_FIFO_SIZE) | DW_FIFO_CHy(DW_FIFO_SIZE));
	dw_write(dma, DW_FIFO_PART0_HI,
		 DW_FIFO_CHx(DW_FIFO_SIZE) | DW_FIFO_CHy(DW_FIFO_SIZE));
	dw_write(dma, DW_FIFO_PART0_LO,
		 DW_FIFO_CHx(DW_FIFO_SIZE) | DW_FIFO_CHy(DW_FIFO_SIZE) |
		 DW_FIFO_UPD);
#endif

	return 0;
}

static int dw_dma_probe(struct dma *dma)
{
	struct dma_pdata *dw_pdata;
	int ret;
	int i;

	if (dma_get_drvdata(dma))
		return -EEXIST; /* already created */

	/* disable dynamic clock gating */
	pm_runtime_get_sync(DW_DMAC_CLK, dma->plat_data.id);

	/* allocate private data */
	dw_pdata = rzalloc(RZONE_SYS_RUNTIME | RZONE_FLAG_UNCACHED,
			   SOF_MEM_CAPS_RAM, sizeof(*dw_pdata));
	if (!dw_pdata) {
		trace_dwdma_error("dw_dma_probe() error: dma %d alloc failed",
				  dma->plat_data.id);
		return -ENOMEM;
	}
	dma_set_drvdata(dma, dw_pdata);

	spinlock_init(&dma->lock);

	ret = dw_dma_setup(dma);
	if (ret < 0)
		return ret;

	/* init work */
	for (i = 0; i < dma->plat_data.channels; i++)
		dw_pdata->chan[i].status = COMP_STATE_INIT;

	/* init number of channels draining */
	atomic_init(&dma->num_channels_busy, 0);

	return 0;
}

static int dw_dma_remove(struct dma *dma)
{
	tracev_dwdma("dw_dma_remove(): dma %d remove", dma->plat_data.id);

	pm_runtime_put_sync(DW_DMAC_CLK, dma->plat_data.id);
	rfree(dma_get_drvdata(dma));
	dma_set_drvdata(dma, NULL);
	return 0;
}

static int dw_dma_avail_data_size(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	int32_t read_ptr = chan->ptr_data.current_ptr;
	int32_t write_ptr = dw_read(dma, DW_DAR(channel));
	int size;

	size = write_ptr - read_ptr;
	if (size < 0)
		size += chan->ptr_data.buffer_bytes;

	return size;
}

static int dw_dma_free_data_size(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	int32_t read_ptr = dw_read(dma, DW_SAR(channel));
	int32_t write_ptr = chan->ptr_data.current_ptr;
	int size;

	size = read_ptr - write_ptr;
	if (size < 0)
		size += chan->ptr_data.buffer_bytes;

	return size;
}

static int dw_dma_get_data_size(struct dma *dma, int channel, uint32_t *avail,
				uint32_t *free)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dw_dma_chan_data *chan = p->chan + channel;
	uint32_t flags;

	tracev_dwdma("dw_dma_get_data_size(): dma %d channel %d get data size",
		     dma->plat_data.id, channel);

	spin_lock_irq(&dma->lock, flags);

	if (chan->direction == DMA_DIR_HMEM_TO_LMEM ||
	    chan->direction == DMA_DIR_DEV_TO_MEM)
		*avail = dw_dma_avail_data_size(dma, channel);
	else
		*free = dw_dma_free_data_size(dma, channel);

	spin_unlock_irq(&dma->lock, flags);

	return 0;
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
	.set_cb			= dw_dma_set_cb,
	.pm_context_restore	= dw_dma_pm_context_restore,
	.pm_context_store	= dw_dma_pm_context_store,
	.probe			= dw_dma_probe,
	.remove			= dw_dma_remove,
	.get_data_size		= dw_dma_get_data_size,
};
