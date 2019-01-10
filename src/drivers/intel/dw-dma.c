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

#include <sof/atomic.h>
#include <sof/debug.h>
#include <sof/sof.h>
#include <sof/dma.h>
#include <sof/dw-dma.h>
#include <sof/io.h>
#include <sof/stream.h>
#include <sof/timer.h>
#include <sof/alloc.h>
#include <sof/interrupt.h>
#include <sof/work.h>
#include <sof/lock.h>
#include <sof/trace.h>
#include <sof/wait.h>
#include <sof/pm_runtime.h>
#include <sof/audio/component.h>
#include <sof/cpu.h>
#include <platform/dma.h>
#include <platform/platform.h>
#include <platform/interrupt.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <config.h>

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
#define DW_RAW_TFR			0x02C0
#define DW_RAW_BLOCK			0x02C8
#define DW_RAW_SRC_TRAN			0x02D0
#define DW_RAW_DST_TRAN			0x02D8
#define DW_RAW_ERR			0x02E0
#define DW_STATUS_TFR			0x02E8
#define DW_STATUS_BLOCK			0x02F0
#define DW_STATUS_SRC_TRAN		0x02F8
#define DW_STATUS_DST_TRAN		0x0300
#define DW_STATUS_ERR			0x0308
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

/* channel bits */
#define INT_MASK(chan)			(0x100 << chan)
#define INT_UNMASK(chan)		(0x101 << chan)
#define INT_MASK_ALL			0xFF00
#define INT_UNMASK_ALL			0xFFFF
#define CHAN_ENABLE(chan)		(0x101 << chan)
#define CHAN_DISABLE(chan)		(0x100 << chan)
#define CHAN_MASK(chan)		(0x1 << chan)

#define DW_CFG_CH_SUSPEND		0x100
#define DW_CFG_CH_FIFO_EMPTY		0x200

/* CTL_LO */
#define DW_CTLL_INT_EN			(1 << 0)
#define DW_CTLL_DST_WIDTH(x)		((x) << 1)
#define DW_CTLL_SRC_WIDTH(x)		((x) << 4)
#define DW_CTLL_DST_INC			(0 << 7)
#define DW_CTLL_DST_DEC			(1 << 7)
#define DW_CTLL_DST_FIX			(2 << 7)
#define DW_CTLL_SRC_INC			(0 << 9)
#define DW_CTLL_SRC_DEC			(1 << 9)
#define DW_CTLL_SRC_FIX			(2 << 9)
#define DW_CTLL_DST_MSIZE(x)		((x) << 11)
#define DW_CTLL_SRC_MSIZE(x)		((x) << 14)
#define DW_CTLL_FC(x)			((x) << 20)
#define DW_CTLL_FC_M2M			(0 << 20)
#define DW_CTLL_FC_M2P			(1 << 20)
#define DW_CTLL_FC_P2M			(2 << 20)
#define DW_CTLL_FC_P2P			(3 << 20)
#define DW_CTLL_DMS(x)			((x) << 23)
#define DW_CTLL_SMS(x)			((x) << 25)
#define DW_CTLL_LLP_D_EN		(1 << 27)
#define DW_CTLL_LLP_S_EN		(1 << 28)
#define DW_CTLL_RELOAD_SRC		(1 << 30)
#define DW_CTLL_RELOAD_DST		(1 << 31)

/* Haswell / Broadwell specific registers */
#if defined(CONFIG_HASWELL) || defined(CONFIG_BROADWELL)

/* CTL_HI */
#define DW_CTLH_DONE(x)			((x) << 12)
#define DW_CTLH_BLOCK_TS_MASK		0x00000fff

/* CFG_LO */
#define DW_CFG_CLASS(x)		(x << 5)

/* CFG_HI */
#define DW_CFGH_SRC_PER(x)		(x << 7)
#define DW_CFGH_DST_PER(x)		(x << 11)

/* default initial setup register values */
#define DW_CFG_LOW_DEF				0x0
#define DW_CFG_HIGH_DEF			0x4

#elif defined(CONFIG_BAYTRAIL) || defined(CONFIG_CHERRYTRAIL)
/* baytrail specific registers */

/* CTL_LO */
#define DW_CTLL_S_GATH_EN		(1 << 17)
#define DW_CTLL_D_SCAT_EN		(1 << 18)

/* CTL_HI */
#define DW_CTLH_DONE(x)			((x) << 17)
#define DW_CTLH_BLOCK_TS_MASK		0x0001ffff
#define DW_CTLH_CLASS(x)		((x) << 29)
#define DW_CTLH_WEIGHT(x)		((x) << 18)

/* CFG_LO */
#define DW_CFG_CH_DRAIN		0x400

/* CFG_HI */
#define DW_CFGH_SRC_PER(x)		((x) << 0)
#define DW_CFGH_DST_PER(x)		((x) << 4)

/* FIFO Partition */
#define DW_FIFO_PARTITION
#define DW_FIFO_PART0_LO		0x0400
#define DW_FIFO_PART0_HI		0x0404
#define DW_FIFO_PART1_LO		0x0408
#define DW_FIFO_PART1_HI		0x040C
#define DW_CH_SAI_ERR			0x0410

/* default initial setup register values */
#define DW_CFG_LOW_DEF			0x00000003
#define DW_CFG_HIGH_DEF		0x0

#elif defined(CONFIG_APOLLOLAKE) || defined(CONFIG_CANNONLAKE) \
	|| defined(CONFIG_ICELAKE) || defined CONFIG_SUECREEK

/* CTL_LO */
#define DW_CTLL_S_GATH_EN		(1 << 17)
#define DW_CTLL_D_SCAT_EN		(1 << 18)

/* CTL_HI */
#define DW_CTLH_DONE(x)			((x) << 17)
#define DW_CTLH_BLOCK_TS_MASK		0x0001ffff
#define DW_CTLH_CLASS(x)		((x) << 29)
#define DW_CTLH_WEIGHT(x)		((x) << 18)

/* CFG_LO */
#define DW_CFG_CTL_HI_UPD_EN		(1 << 5)
#define DW_CFG_CH_DRAIN			(1 << 10)
#define DW_CFG_RELOAD_SRC		(1 << 30)
#define DW_CFG_RELOAD_DST		(1 << 31)

/* CFG_HI */
#define DW_CFGH_SRC_PER(x)		(x << 0)
#define DW_CFGH_DST_PER(x)		(x << 4)

/* FIFO Partition */
#define DW_FIFO_PARTITION
#define DW_FIFO_PART0_LO		0x0400
#define DW_FIFO_PART0_HI		0x0404
#define DW_FIFO_PART1_LO		0x0408
#define DW_FIFO_PART1_HI		0x040C
#define DW_CH_SAI_ERR			0x0410
#define DW_DMA_GLB_CFG			0x0418

/* default initial setup register values */
#define DW_CFG_LOW_DEF			0x00000003
#define DW_CFG_HIGH_DEF			0x0

#define DW_REG_MAX			DW_DMA_GLB_CFG
#endif

/* tracing */
#define trace_dwdma(__e, ...) \
	trace_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define tracev_dwdma(__e, ...) \
	tracev_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define trace_dwdma_error(__e, ...) \
	trace_error(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)

/* HW Linked list support, only enabled for APL/CNL at the moment */
#if defined CONFIG_APOLLOLAKE || defined CONFIG_CANNONLAKE \
	|| defined CONFIG_ICELAKE || defined CONFIG_SUECREEK
#define DW_USE_HW_LLI	1
#else
#define DW_USE_HW_LLI	0
#endif

/* number of tries to wait for reset */
#define DW_DMA_CFG_TRIES	10000

struct dma_id {
	struct dma *dma;
	uint32_t channel;
};

/* data for each DMA channel */
struct dma_chan_data {
	uint32_t status;
	uint32_t direction;
	struct dw_lli2 *lli;
	struct dw_lli2 *lli_current;
	uint32_t desc_count;
	uint32_t cfg_lo;
	uint32_t cfg_hi;
	struct dma_id id;
	uint32_t timer_delay;
	struct work dma_ch_work;

	/* client callback function */
	void (*cb)(void *data, uint32_t type, struct dma_sg_elem *next);
	/* client callback data */
	void *cb_data;
	/* callback type */
	int cb_type;
};

/* private data for DW DMA engine */
struct dma_pdata {
	struct dma_chan_data chan[DW_MAX_CHAN];
	uint32_t class;		/* channel class - set for controller atm */
};

static inline void dw_dma_chan_reload_lli(struct dma *dma, int channel);
static inline void dw_dma_chan_reload_next(struct dma *dma, int channel,
		struct dma_sg_elem *next);
static inline int dw_dma_interrupt_register(struct dma *dma, int channel);
static inline void dw_dma_interrupt_unregister(struct dma *dma, int channel);
static uint64_t dw_dma_work(void *data, uint64_t delay);

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
static int dw_dma_channel_get(struct dma *dma, int req_chan)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;
	int i;

	spin_lock_irq(&dma->lock, flags);

	trace_dwdma("dw-dma %d request channel", dma->plat_data.id);

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

	/* DMAC has no free channels */
	spin_unlock_irq(&dma->lock, flags);
	trace_dwdma_error("dw-dma %d no channel is free", dma->plat_data.id);
	return -ENODEV;
}

/* channel must not be running when this is called */
static void dw_dma_channel_put_unlocked(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_chan_data *chan = p->chan + channel;

	if (channel >= dma->plat_data.channels) {
		trace_dwdma_error("dw-dma: %d invalid channel %d",
				  dma->plat_data.id, channel);
		return;
	}

	trace_dwdma("dw-dma: %d channel %d put", dma->plat_data.id, channel);

	if (!chan->timer_delay) {
		/* mask block, transfer and error interrupts for channel */
		dw_write(dma, DW_MASK_TFR, INT_MASK(channel));
		dw_write(dma, DW_MASK_BLOCK, INT_MASK(channel));
		dw_write(dma, DW_MASK_ERR, INT_MASK(channel));
	}

	/* free the lli allocated by set_config*/
	if (chan->lli) {
		rfree(chan->lli);
		chan->lli = NULL;
	}

	/* set new state */
	chan->status = COMP_STATE_INIT;
	chan->cb = NULL;
	chan->desc_count = 0;

	if (chan->timer_delay)
		work_init(&chan->dma_ch_work, NULL, NULL, 0);

	atomic_sub(&dma->num_channels_busy, 1);
}

/* channel must not be running when this is called */
static void dw_dma_channel_put(struct dma *dma, int channel)
{
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);
	dw_dma_channel_put_unlocked(dma, channel);
	spin_unlock_irq(&dma->lock, flags);
}

static int dw_dma_start(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_chan_data *chan = p->chan + channel;
	struct dw_lli2 *lli = chan->lli_current;
	uint32_t flags;
	int ret = 0;

	if (channel >= dma->plat_data.channels) {
		trace_dwdma_error("dw-dma: %d invalid channel %d",
				  dma->plat_data.id, channel);
		return -EINVAL;
	}

	spin_lock_irq(&dma->lock, flags);

	tracev_dwdma("dw-dma: %d channel %d start", dma->plat_data.id, channel);

	/* is channel idle, disabled and ready ? */
	if (chan->status != COMP_STATE_PREPARE ||
		(dw_read(dma, DW_DMA_CHAN_EN) & (0x1 << channel))) {
		ret = -EBUSY;
		trace_dwdma_error("dw-dma: %d channel %d not ready",
				  dma->plat_data.id, channel);
		trace_dwdma_error(" ena 0x%x cfglow 0x%x status 0x%x",
				  dw_read(dma, DW_DMA_CHAN_EN),
				  dw_read(dma, DW_CFG_LOW(channel)),
				  chan->status);
		goto out;
	}

	/* valid stream ? */
	if (chan->lli == NULL) {
		ret = -EINVAL;
		trace_dwdma_error("dw-dma: %d channel %d invalid stream",
				  dma->plat_data.id, channel);
		goto out;
	}

	if (!chan->timer_delay) {
		/* write interrupt clear registers for the channel:
		 * ClearTfr, ClearBlock, ClearSrcTran, ClearDstTran, ClearErr
		 */
		dw_write(dma, DW_CLEAR_TFR, 0x1 << channel);
		dw_write(dma, DW_CLEAR_BLOCK, 0x1 << channel);
		dw_write(dma, DW_CLEAR_SRC_TRAN, 0x1 << channel);
		dw_write(dma, DW_CLEAR_DST_TRAN, 0x1 << channel);
		dw_write(dma, DW_CLEAR_ERR, 0x1 << channel);

		/* clear platform interrupt */
		platform_interrupt_clear(dma_irq(dma, cpu_get_id()),
					 1 << channel);
	}

#if DW_USE_HW_LLI
	/* TODO: Revisit: are we using LLP mode or single transfer ? */
	if (lli) {
		/* LLP mode - write LLP pointer */
		dw_write(dma, DW_LLP(channel), (uint32_t)lli);
	}
#endif
	/* channel needs started from scratch, so write SARn, DARn */
	dw_write(dma, DW_SAR(channel), lli->sar);
	dw_write(dma, DW_DAR(channel), lli->dar);

	/* program CTLn */
	dw_write(dma, DW_CTRL_LOW(channel), lli->ctrl_lo);
	dw_write(dma, DW_CTRL_HIGH(channel), lli->ctrl_hi);

	/* write channel config */
	dw_write(dma, DW_CFG_LOW(channel), chan->cfg_lo);
	dw_write(dma, DW_CFG_HIGH(channel), chan->cfg_hi);

	if (chan->timer_delay)
		/* activate timer for timer driven scheduling */
		work_schedule_default(&chan->dma_ch_work,
				      chan->timer_delay);
	else if (chan->status == COMP_STATE_PREPARE)
		/* enable interrupt only for the first start */
		ret = dw_dma_interrupt_register(dma, channel);

	if (ret == 0) {
		/* enable the channel */
		chan->status = COMP_STATE_ACTIVE;
		dw_write(dma, DW_DMA_CHAN_EN, CHAN_ENABLE(channel));
	}

out:
	spin_unlock_irq(&dma->lock, flags);
	return ret;
}

static int dw_dma_release(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;

	if (channel >= dma->plat_data.channels) {
		trace_dwdma_error("dw-dma: %d invalid channel %d",
				  dma->plat_data.id, channel);
		return -EINVAL;
	}

	spin_lock_irq(&dma->lock, flags);

	trace_dwdma("dw-dma: %d channel %d release", dma->plat_data.id, channel);

	/* get next lli for proper release */
	p->chan[channel].lli_current =
		(struct dw_lli2 *)p->chan[channel].lli_current->llp;

	spin_unlock_irq(&dma->lock, flags);
	return 0;
}

static int dw_dma_pause(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;

	if (channel >= dma->plat_data.channels) {
		trace_dwdma_error("dw-dma: %d invalid channel %d",
				  dma->plat_data.id, channel);
		return -EINVAL;
	}

	spin_lock_irq(&dma->lock, flags);

	trace_dwdma("dw-dma: %d channel %d pause", dma->plat_data.id, channel);

	if (p->chan[channel].status != COMP_STATE_ACTIVE)
		goto out;

	/* pause the channel */
	p->chan[channel].status = COMP_STATE_PAUSED;

out:
	spin_unlock_irq(&dma->lock, flags);
	return 0;
}

#if defined CONFIG_BAYTRAIL || defined CONFIG_CHERRYTRAIL
static int dw_dma_stop(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_chan_data *chan = p->chan + channel;
	int ret;
	uint32_t flags;

	if (channel >= dma->plat_data.channels) {
		trace_dwdma_error("dw-dma: %d invalid channel %d",
				  dma->plat_data.id, channel);
		return -EINVAL;
	}

	spin_lock_irq(&dma->lock, flags);

	trace_dwdma("dw-dma: %d channel %d stop", dma->plat_data.id, channel);

	if (chan->timer_delay)
		work_cancel_default(&chan->dma_ch_work);

	ret = poll_for_register_delay(dma_base(dma) + DW_DMA_CHAN_EN,
				      CHAN_MASK(channel), 0,
				      PLATFORM_DMA_TIMEOUT);
	if (ret < 0)
		trace_dwdma_error("dw-dma: %d channel %d timeout",
				  dma->plat_data.id, channel);

	if (!chan->timer_delay)
		dw_write(dma, DW_CLEAR_BLOCK, 0x1 << channel);

	chan->status = COMP_STATE_PREPARE;

	spin_unlock_irq(&dma->lock, flags);
	return ret;
}
#else
static int dw_dma_stop(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_chan_data *chan = p->chan + channel;
	uint32_t flags;
#if DW_USE_HW_LLI
	int i;
	struct dw_lli2 *lli;
#endif

	if (channel >= dma->plat_data.channels) {
		trace_dwdma_error("dw-dma: %d invalid channel %d",
				  dma->plat_data.id, channel);
		return -EINVAL;
	}

	spin_lock_irq(&dma->lock, flags);

	trace_dwdma("dw-dma: %d channel %d stop", dma->plat_data.id, channel);

	if (chan->timer_delay)
		work_cancel_default(&chan->dma_ch_work);

	dw_write(dma, DW_DMA_CHAN_EN, CHAN_DISABLE(channel));

#if DW_USE_HW_LLI
	lli = chan->lli;
	for (i = 0; i < chan->desc_count; i++) {
		lli->ctrl_hi &= ~DW_CTLH_DONE(1);
		lli++;
	}

	dcache_writeback_region(chan->lli,
			sizeof(struct dw_lli2) * chan->desc_count);
#endif

	if (!chan->timer_delay) {
		dw_write(dma, DW_CLEAR_BLOCK, 0x1 << channel);

		/* disable interrupt */
		dw_dma_interrupt_unregister(dma, channel);
	}

	chan->status = COMP_STATE_PREPARE;

	spin_unlock_irq(&dma->lock, flags);

	return 0;
}
#endif

/* fill in "status" with current DMA channel state and position */
static int dw_dma_status(struct dma *dma, int channel,
	struct dma_chan_status *status, uint8_t direction)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	if (channel >= dma->plat_data.channels) {
		trace_dwdma_error("dw-dma: %d invalid channel %d",
				  dma->plat_data.id, channel);
		return -EINVAL;
	}

	status->state = p->chan[channel].status;
	status->r_pos = dw_read(dma, DW_SAR(channel));
	status->w_pos = dw_read(dma, DW_DAR(channel));
	status->timestamp = timer_get_system(platform_timer);

	return 0;
}

/*
 * use array to get burst_elems for specific slot number setting.
 * the relation between msize and burst_elems should be
 * 2 ^ msize = burst_elems
 */
static const uint32_t burst_elems[] = {1, 2, 4, 8};

/* set the DMA channel configuration, source/target address, buffer sizes */
static int dw_dma_set_config(struct dma *dma, int channel,
	struct dma_sg_config *config)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_chan_data *chan = p->chan + channel;
	struct dma_sg_elem *sg_elem;
	struct dw_lli2 *lli_desc;
	struct dw_lli2 *lli_desc_head;
	struct dw_lli2 *lli_desc_tail;
	uint32_t flags;
	uint32_t msize = 3;/* default msize */
	int i, ret = 0;

	if (channel >= dma->plat_data.channels) {
		trace_dwdma_error("dw-dma: %d invalid channel %d",
				  dma->plat_data.id, channel);
		return -EINVAL;
	}

	spin_lock_irq(&dma->lock, flags);

	tracev_dwdma("dw-dma: %d channel %d config", dma->plat_data.id,
		     channel);

	/* default channel config */
	chan->direction = config->direction;
	chan->timer_delay = config->timer_delay;
	chan->cfg_lo = DW_CFG_LOW_DEF;
	chan->cfg_hi = DW_CFG_HIGH_DEF;

	if (!config->elem_array.count) {
		trace_dwdma_error("dw-dma: %d channel %d no elems",
				  dma->plat_data.id, channel);
		ret = -EINVAL;
		goto out;
	}

	/* do we need to realloc descriptors */
	if (config->elem_array.count != chan->desc_count) {

		chan->desc_count = config->elem_array.count;

		/* allocate descriptors for channel */
		if (chan->lli)
			rfree(chan->lli);
		chan->lli = rzalloc(RZONE_SYS_RUNTIME,
				SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_DMA,
				sizeof(struct dw_lli2) *
				chan->desc_count);
		if (chan->lli == NULL) {
			trace_dwdma_error("dw-dma: %d channel %d LLI alloc failed",
					  dma->plat_data.id, channel);
			ret = -ENOMEM;
			goto out;
		}
	}

	/* initialise descriptors */
	bzero(chan->lli, sizeof(struct dw_lli2) * chan->desc_count);
	lli_desc = lli_desc_head = chan->lli;
	lli_desc_tail = chan->lli + chan->desc_count - 1;

	/* configure msize if burst_elems is set */
	if (config->burst_elems) {
		/* burst_elems set, configure msize */
		for (i = 0; i < ARRAY_SIZE(burst_elems); i++) {
			if (burst_elems[i] == config->burst_elems) {
				msize = i;
				break;
			}
		}
	}

	if (!chan->timer_delay) {
		/* unmask block, transfer and error interrupts
		 * for channel
		 */
		dw_write(dma, DW_MASK_TFR, INT_UNMASK(channel));
		dw_write(dma, DW_MASK_BLOCK, INT_UNMASK(channel));
		dw_write(dma, DW_MASK_ERR, INT_UNMASK(channel));
	}

	/* fill in lli for the elem in the list */
	for (i = 0; i < config->elem_array.count; i++) {

		sg_elem = config->elem_array.elems + i;

		/* write CTL_LOn for each lli */
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
			trace_dwdma_error("dw-dma: %d channel %d invalid src width %d",
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
			trace_dwdma_error("dw-dma: %d channel %d invalid dest width %d",
					  dma->plat_data.id, channel,
					  config->dest_width);
			ret = -EINVAL;
			goto out;
		}

		lli_desc->ctrl_lo |= DW_CTLL_SRC_MSIZE(msize) |
			DW_CTLL_DST_MSIZE(msize) |
			DW_CTLL_INT_EN; /* enable interrupt */

		/* config the SINC and DINC field of CTL_LOn,
		 * SRC/DST_PER filed of CFGn
		 */
		switch (config->direction) {
		case DMA_DIR_LMEM_TO_HMEM:
			lli_desc->ctrl_lo |= DW_CTLL_FC_M2M | DW_CTLL_SRC_INC |
				DW_CTLL_DST_INC;
#if DW_USE_HW_LLI
			lli_desc->ctrl_lo |=
				DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;
#endif
			lli_desc->sar =
				(uint32_t)sg_elem->src | PLATFORM_HOST_DMA_MASK;
			lli_desc->dar = (uint32_t)sg_elem->dest;
			break;
		case DMA_DIR_HMEM_TO_LMEM:
			lli_desc->ctrl_lo |= DW_CTLL_FC_M2M | DW_CTLL_SRC_INC |
				DW_CTLL_DST_INC;
#if DW_USE_HW_LLI
			lli_desc->ctrl_lo |=
				DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;
#endif
			lli_desc->dar =
				(uint32_t)sg_elem->dest
				| PLATFORM_HOST_DMA_MASK;
			lli_desc->sar = (uint32_t)sg_elem->src;
			break;
		case DMA_DIR_MEM_TO_MEM:
			lli_desc->ctrl_lo |= DW_CTLL_FC_M2M | DW_CTLL_SRC_INC |
				DW_CTLL_DST_INC;
#if DW_USE_HW_LLI
			lli_desc->ctrl_lo |=
				DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;
#endif
			lli_desc->sar = (uint32_t)sg_elem->src
					| PLATFORM_HOST_DMA_MASK;
			lli_desc->dar = (uint32_t)sg_elem->dest
					| PLATFORM_HOST_DMA_MASK;
			break;
		case DMA_DIR_MEM_TO_DEV:
			lli_desc->ctrl_lo |= DW_CTLL_FC_M2P | DW_CTLL_SRC_INC |
				DW_CTLL_DST_FIX;
#if DW_USE_HW_LLI
			lli_desc->ctrl_lo |= DW_CTLL_LLP_S_EN;
			lli_desc->ctrl_hi |= DW_CTLH_DONE(1);
			chan->cfg_lo |= DW_CFG_RELOAD_DST;
#endif
			chan->cfg_hi |= DW_CFGH_DST_PER(config->dest_dev);
			lli_desc->sar = (uint32_t)sg_elem->src
					| PLATFORM_HOST_DMA_MASK;
			lli_desc->dar = (uint32_t)sg_elem->dest;
			break;
		case DMA_DIR_DEV_TO_MEM:
			lli_desc->ctrl_lo |= DW_CTLL_FC_P2M | DW_CTLL_SRC_FIX |
				DW_CTLL_DST_INC;
#if DW_USE_HW_LLI
			lli_desc->ctrl_lo |= DW_CTLL_LLP_D_EN;
			lli_desc->ctrl_hi |= DW_CTLH_DONE(0);
			chan->cfg_lo |= DW_CFG_RELOAD_SRC;
#endif
			chan->cfg_hi |= DW_CFGH_SRC_PER(config->src_dev);
			lli_desc->sar = (uint32_t)sg_elem->src;
			lli_desc->dar = (uint32_t)sg_elem->dest
					| PLATFORM_HOST_DMA_MASK;
			break;
		case DMA_DIR_DEV_TO_DEV:
			lli_desc->ctrl_lo |= DW_CTLL_FC_P2P | DW_CTLL_SRC_FIX |
				DW_CTLL_DST_FIX;
#if DW_USE_HW_LLI
			lli_desc->ctrl_lo |=
				DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN;
#endif
			chan->cfg_hi |= DW_CFGH_SRC_PER(config->src_dev) |
				DW_CFGH_DST_PER(config->dest_dev);
			lli_desc->sar = (uint32_t)sg_elem->src;
			lli_desc->dar = (uint32_t)sg_elem->dest;
			break;
		default:
			trace_dwdma_error("dw-dma: %d channel %d invalid direction %d",
					  dma->plat_data.id, channel,
					  config->direction);
			ret = -EINVAL;
			goto out;
		}

		if (sg_elem->size > DW_CTLH_BLOCK_TS_MASK) {
			trace_dwdma_error("dw-dma: %d channel %d block size too big %d",
					  dma->plat_data.id, channel,
					  sg_elem->size);
			ret = -EINVAL;
			goto out;
		}

		/* set transfer size of element */
#if defined CONFIG_BAYTRAIL || defined CONFIG_CHERRYTRAIL \
	|| defined CONFIG_APOLLOLAKE || defined CONFIG_CANNONLAKE \
	|| defined CONFIG_ICELAKE || defined CONFIG_SUECREEK
		lli_desc->ctrl_hi = DW_CTLH_CLASS(p->class) |
			(sg_elem->size & DW_CTLH_BLOCK_TS_MASK);
#elif defined CONFIG_BROADWELL || defined CONFIG_HASWELL
		/* for bdw, the unit is transaction--TR_WIDTH. */
		lli_desc->ctrl_hi = (sg_elem->size /
			(1 << (lli_desc->ctrl_lo >> 4 & 0x7)))
			& DW_CTLH_BLOCK_TS_MASK;
#endif

		/* set next descriptor in list */
		lli_desc->llp = (uint32_t)(lli_desc + 1);

		/* next descriptor */
		lli_desc++;
	}

#if DW_USE_HW_LLI
	chan->cfg_lo |= DW_CFG_CTL_HI_UPD_EN;
#endif

	/* end of list or cyclic buffer ? */
	if (config->cyclic) {
		lli_desc_tail->llp = (uint32_t)lli_desc_head;
	} else {
		lli_desc_tail->llp = 0x0;
#if DW_USE_HW_LLI
		lli_desc_tail->ctrl_lo &=
			~(DW_CTLL_LLP_S_EN | DW_CTLL_LLP_D_EN);
#endif
	}

	/* write back descriptors so DMA engine can read them directly */
	dcache_writeback_region(chan->lli,
			sizeof(struct dw_lli2) * chan->desc_count);

	chan->status = COMP_STATE_PREPARE;
	chan->lli_current = chan->lli;

	if (chan->timer_delay)
		work_init(&chan->dma_ch_work, dw_dma_work,
			  &chan->id, WORK_SYNC);
out:
	spin_unlock_irq(&dma->lock, flags);
	return ret;
}

/* restore DMA conext after leaving D3 */
static int dw_dma_pm_context_restore(struct dma *dma)
{
	return 0;
}

/* store DMA conext after leaving D3 */
static int dw_dma_pm_context_store(struct dma *dma)
{
	/* disable the DMA controller */
	dw_write(dma, DW_DMA_CFG, 0);

	return 0;
}

static int dw_dma_set_cb(struct dma *dma, int channel, int type,
		void (*cb)(void *data, uint32_t type, struct dma_sg_elem *next),
		void *data)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_chan_data *chan = p->chan + channel;
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);
	chan->cb = cb;
	chan->cb_data = data;
	chan->cb_type = type;
	spin_unlock_irq(&dma->lock, flags);

	return 0;
}

/* reload using LLI data */
static inline void dw_dma_chan_reload_lli(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_chan_data *chan = p->chan + channel;
	struct dw_lli2 *lli = chan->lli_current;

	/* only need to reload if this is a block transfer */
	if (!lli || lli->llp == 0) {
		chan->status = COMP_STATE_PREPARE;
		return;
	}

	/* get current and next block pointers */
	lli = (struct dw_lli2 *)lli->llp;
	chan->lli_current = lli;

	/* channel needs started from scratch, so write SARn, DARn */
	dw_write(dma, DW_SAR(channel), lli->sar);
	dw_write(dma, DW_DAR(channel), lli->dar);

	/* program CTLn */
	dw_write(dma, DW_CTRL_LOW(channel), lli->ctrl_lo);
	dw_write(dma, DW_CTRL_HIGH(channel), lli->ctrl_hi);

	/* program CFGn */
	dw_write(dma, DW_CFG_LOW(channel), chan->cfg_lo);
	dw_write(dma, DW_CFG_HIGH(channel), chan->cfg_hi);

	/* enable the channel */
	dw_write(dma, DW_DMA_CHAN_EN, CHAN_ENABLE(channel));
}

/* reload using callback data */
static inline void dw_dma_chan_reload_next(struct dma *dma, int channel,
		struct dma_sg_elem *next)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_chan_data *chan = p->chan + channel;
	struct dw_lli2 *lli = chan->lli_current;

	/* channel needs started from scratch, so write SARn, DARn */
	dw_write(dma, DW_SAR(channel), next->src);
	dw_write(dma, DW_DAR(channel), next->dest);

	/* set transfer size of element */
#if defined CONFIG_BAYTRAIL || defined CONFIG_CHERRYTRAIL \
	|| defined CONFIG_APOLLOLAKE || defined CONFIG_CANNONLAKE \
	|| defined CONFIG_ICELAKE || defined CONFIG_SUECREEK
		lli->ctrl_hi = DW_CTLH_CLASS(p->class) |
			(next->size & DW_CTLH_BLOCK_TS_MASK);
#elif defined CONFIG_BROADWELL || defined CONFIG_HASWELL
	/* for the unit is transaction--TR_WIDTH. */
	lli->ctrl_hi = (next->size / (1 << (lli->ctrl_lo >> 4 & 0x7)))
		& DW_CTLH_BLOCK_TS_MASK;
#endif

	/* program CTLn */
	dw_write(dma, DW_CTRL_LOW(channel), lli->ctrl_lo);
	dw_write(dma, DW_CTRL_HIGH(channel), lli->ctrl_hi);

	/* program CFGn */
	dw_write(dma, DW_CFG_LOW(channel), chan->cfg_lo);
	dw_write(dma, DW_CFG_HIGH(channel), chan->cfg_hi);

	/* enable the channel */
	dw_write(dma, DW_DMA_CHAN_EN, CHAN_ENABLE(channel));
}

static void dw_dma_setup(struct dma *dma)
{
	struct dw_drv_plat_data *dp = dma->plat_data.drv_plat_data;
	int i;

	/* we cannot config DMAC if DMAC has been already enabled by host */
	if (dw_read(dma, DW_DMA_CFG) != 0)
		dw_write(dma, DW_DMA_CFG, 0x0);

	/* now check that it's 0 */
	for (i = DW_DMA_CFG_TRIES; i > 0; i--) {
		if (dw_read(dma, DW_DMA_CFG) == 0)
			goto found;
	}
	trace_dwdma_error("dw-dma: dmac %d setup failed", dma->plat_data.id);
	return;

found:
	for (i = 0; i <  DW_MAX_CHAN; i++)
		dw_read(dma, DW_DMA_CHAN_EN);

#ifdef HAVE_HDDA
	/* enable HDDA before DMAC */
	shim_write(SHIM_HMDC, SHIM_HMDC_HDDA_ALLCH);
#endif

	/* enable the DMA controller */
	dw_write(dma, DW_DMA_CFG, 1);

	/* mask all interrupts for all 8 channels */
	dw_write(dma, DW_MASK_TFR, INT_MASK_ALL);
	dw_write(dma, DW_MASK_BLOCK, INT_MASK_ALL);
	dw_write(dma, DW_MASK_SRC_TRAN, INT_MASK_ALL);
	dw_write(dma, DW_MASK_DST_TRAN, INT_MASK_ALL);
	dw_write(dma, DW_MASK_ERR, INT_MASK_ALL);

#ifdef DW_FIFO_PARTITION
	/* TODO: we cannot config DMA FIFOs if DMAC has been already */
	/* allocate FIFO partitions, 128 bytes for each ch */
	dw_write(dma, DW_FIFO_PART1_LO, 0x100080);
	dw_write(dma, DW_FIFO_PART1_HI, 0x100080);
	dw_write(dma, DW_FIFO_PART0_HI, 0x100080);
	dw_write(dma, DW_FIFO_PART0_LO, 0x100080 | (1 << 26));
	dw_write(dma, DW_FIFO_PART0_LO, 0x100080);
#endif

	/* set channel priorities */
	for (i = 0; i <  DW_MAX_CHAN; i++) {
#if defined CONFIG_BAYTRAIL || defined CONFIG_CHERRYTRAIL \
	|| defined CONFIG_APOLLOLAKE || defined CONFIG_CANNONLAKE \
	|| defined CONFIG_ICELAKE || defined CONFIG_SUECREEK
		dw_write(dma, DW_CTRL_HIGH(i),
			 DW_CTLH_CLASS(dp->chan[i].class));
#elif defined CONFIG_BROADWELL || defined CONFIG_HASWELL
		dw_write(dma, DW_CFG_LOW(i),
			 DW_CFG_CLASS(dp->chan[i].class));
#endif
	}
}

static void dw_dma_process_block(struct dma_chan_data *chan,
				 struct dma_sg_elem *next)
{
	/* reload lli by default */
	next->src = DMA_RELOAD_LLI;
	next->dest = DMA_RELOAD_LLI;
	next->size = DMA_RELOAD_LLI;

	if (chan->cb)
		chan->cb(chan->cb_data, DMA_IRQ_TYPE_BLOCK, next);

	if (next->size == DMA_RELOAD_END) {
		tracev_dwdma("dw-dma: %d channel %d block end",
			    chan->id.dma->plat_data.id, chan->id.channel);

		/* disable channel, finished */
		dw_write(chan->id.dma, DW_DMA_CHAN_EN,
			 CHAN_DISABLE(chan->id.channel));
		chan->status = COMP_STATE_PREPARE;
	}

	chan->lli_current->ctrl_hi &= ~DW_CTLH_DONE(1);
	dcache_writeback_region(chan->lli_current, sizeof(*chan->lli_current));

	chan->lli_current = (struct dw_lli2 *)chan->lli_current->llp;
}

static uint64_t dw_dma_work(void *data, uint64_t delay)
{
	struct dma_id *dma_id = (struct dma_id *)data;
	struct dma *dma = dma_id->dma;
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_sg_elem next;
	int i = dma_id->channel;

	tracev_dwdma("dw-dma: %d channel work", dma->plat_data.id,
		     dma_id->channel);

	if (p->chan[i].status != COMP_STATE_ACTIVE) {
		trace_dwdma_error("dw-dma: %d channel %d not running",
				  dma->plat_data.id, dma_id->channel);
		/* skip if channel is not running */
		return 0;
	}

	dw_dma_process_block(&p->chan[i], &next);

	return next.size == DMA_RELOAD_END ? 0 : p->chan[i].timer_delay;
}

#if CONFIG_APOLLOLAKE
/* interrupt handler for DW DMA */
static void dw_dma_irq_handler(void *data)
{
	struct dma_id *dma_id = (struct dma_id *)data;
	struct dma *dma = dma_id->dma;
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_sg_elem next;
	uint32_t status_tfr, status_block, status_err, status_intr;
	uint32_t mask;
	int i = dma_id->channel;

	status_intr = dw_read(dma, DW_INTR_STATUS);
	if (!status_intr) {
		trace_dwdma_error("dw-dma: %d IRQ with no status",
				  dma->plat_data.id);
	}

	tracev_dwdma("dw-dma: %d IRQ status 0x%x", dma->plat_data.id,
		     status_intr);

	/* get the source of our IRQ. */
	status_block = dw_read(dma, DW_STATUS_BLOCK);
	status_tfr = dw_read(dma, DW_STATUS_TFR);

	/* TODO: handle errors, just clear them atm */
	status_err = dw_read(dma, DW_STATUS_ERR);
	if (status_err) {
		trace_dwdma("dw-dma: %d IRQ error 0x%", dma->plat_data.id,
			    status_err);
		dw_write(dma, DW_CLEAR_ERR, status_err & i);
	}

	mask = 0x1 << i;

	/* clear interrupts for channel*/
	dw_write(dma, DW_CLEAR_BLOCK, status_block & mask);
	dw_write(dma, DW_CLEAR_TFR, status_tfr & mask);

	/* skip if channel is not running */
	if (p->chan[i].status != COMP_STATE_ACTIVE) {
		trace_dwdma_error("dw-dma: %d channel %d not running",
				  dma->plat_data.id, dma_id->channel);
		return;
	}

	/* end of a LLI block */
	if (status_block & mask &&
	    p->chan[i].cb_type & DMA_IRQ_TYPE_BLOCK)
		dw_dma_process_block(&p->chan[i], &next);
}

static inline int dw_dma_interrupt_register(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t irq = dma_irq(dma, cpu_get_id()) +
		(channel << SOF_IRQ_BIT_SHIFT);
	int ret;

	trace_event(TRACE_CLASS_DMA, "dw_dma_interrupt_register()");

	ret = interrupt_register(irq, IRQ_AUTO_UNMASK, dw_dma_irq_handler,
				 &p->chan[channel].id);
	if (ret < 0) {
		trace_dwdma_error("DWDMA failed to allocate IRQ");
		return ret;
	}

	interrupt_enable(irq);
	return 0;
}

static inline void dw_dma_interrupt_unregister(struct dma *dma, int channel)
{
	uint32_t irq = dma_irq(dma, cpu_get_id()) +
		(channel << SOF_IRQ_BIT_SHIFT);

	interrupt_disable(irq);
	interrupt_unregister(irq);
}
#else
static void dw_dma_process_transfer(struct dma_chan_data *chan,
				    struct dma_sg_elem *next)
{
	/* reload lli by default */
	next->src = DMA_RELOAD_LLI;
	next->dest = DMA_RELOAD_LLI;
	next->size = DMA_RELOAD_LLI;

	if (chan->cb)
		chan->cb(chan->cb_data, DMA_IRQ_TYPE_LLIST, next);

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
		chan->lli_current = (struct dw_lli2 *)chan->lli_current->llp;
		break;
	case DMA_RELOAD_LLI:
		dw_dma_chan_reload_lli(chan->id.dma, chan->id.channel);
		break;
	default:
		dw_dma_chan_reload_next(chan->id.dma, chan->id.channel, next);
		break;
	}
}

/* interrupt handler for DMA */
static void dw_dma_irq_handler(void *data)
{
	struct dma *dma = (struct dma *)data;
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_sg_elem next;
	uint32_t status_tfr;
	uint32_t status_block;
	uint32_t status_block_new;
	uint32_t status_err;
	uint32_t status_intr;
	uint32_t mask;
	uint32_t pmask;
	int i;

	status_intr = dw_read(dma, DW_INTR_STATUS);
	if (!status_intr)
		return;

	tracev_dwdma("dw-dma: %d IRQ status 0x%x", dma->plat_data.id,
		     status_intr);

	/* get the source of our IRQ. */
	status_block = dw_read(dma, DW_STATUS_BLOCK);
	status_tfr = dw_read(dma, DW_STATUS_TFR);

	/* clear interrupts */
	dw_write(dma, DW_CLEAR_BLOCK, status_block);
	dw_write(dma, DW_CLEAR_TFR, status_tfr);

	/* TODO: handle errors, just clear them atm */
	status_err = dw_read(dma, DW_STATUS_ERR);
	dw_write(dma, DW_CLEAR_ERR, status_err);
	if (status_err)
		trace_dwdma_error("dw-dma: %d error 0x%x", dma->plat_data.id,
				  status_err);

	/* clear platform and DSP interrupt */
	pmask = status_block | status_tfr | status_err;
	platform_interrupt_clear(dma_irq(dma, cpu_get_id()), pmask);

	/* confirm IRQ cleared */
	status_block_new = dw_read(dma, DW_STATUS_BLOCK);
	if (status_block_new) {
		trace_dwdma_error("dw-dma: %d status block 0x%x not cleared",
				  dma->plat_data.id, status_block_new);
	}

	for (i = 0; i < dma->plat_data.channels; i++) {

		/* skip if channel is not running */
		if (p->chan[i].status != COMP_STATE_ACTIVE)
			continue;

		mask = 0x1 << i;

#if DW_USE_HW_LLI
		/* end of a LLI block */
		if (status_block & mask &&
		    p->chan[i].cb_type & DMA_IRQ_TYPE_BLOCK)
			dw_dma_process_block(&p->chan[i], &next);
#endif
		/* end of a transfer */
		if (status_tfr & mask &&
		    p->chan[i].cb_type & DMA_IRQ_TYPE_LLIST)
			dw_dma_process_transfer(&p->chan[i], &next);
	}
}

static inline int dw_dma_interrupt_register(struct dma *dma, int channel)
{
	uint32_t irq = dma_irq(dma, cpu_get_id());
	int ret;

	ret = interrupt_register(irq, IRQ_AUTO_UNMASK, dw_dma_irq_handler, dma);
	if (ret < 0) {
		trace_dwdma_error("DWDMA failed to allocate IRQ");
		return ret;
	}

	interrupt_enable(irq);
	return 0;
}

static inline void dw_dma_interrupt_unregister(struct dma *dma, int channel)
{
	uint32_t irq = dma_irq(dma, cpu_get_id());

	interrupt_disable(irq);
	interrupt_unregister(irq);
}
#endif

static int dw_dma_probe(struct dma *dma)
{
	struct dma_pdata *dw_pdata;
	int i;

	if (dma_get_drvdata(dma))
		return -EEXIST; /* already created */

	/* disable dynamic clock gating */
	pm_runtime_get_sync(DW_DMAC_CLK, dma->plat_data.id);

	/* allocate private data */
	dw_pdata = rzalloc(RZONE_SYS_RUNTIME | RZONE_FLAG_UNCACHED,
			   SOF_MEM_CAPS_RAM, sizeof(*dw_pdata));
	if (!dw_pdata) {
		trace_error(TRACE_CLASS_DMA, "dw_dma_probe() error: "
			    "alloc failed");
		return -ENOMEM;
	}
	dma_set_drvdata(dma, dw_pdata);

	spinlock_init(&dma->lock);

	dw_dma_setup(dma);

	/* init work */
	for (i = 0; i < dma->plat_data.channels; i++) {
		dw_pdata->chan[i].id.dma = dma;
		dw_pdata->chan[i].id.channel = i;
		dw_pdata->chan[i].status = COMP_STATE_INIT;
	}

	/* init number of channels draining */
	atomic_init(&dma->num_channels_busy, 0);

	return 0;
}

static int dw_dma_remove(struct dma *dma)
{
	pm_runtime_put_sync(DW_DMAC_CLK, dma->plat_data.id);
	rfree(dma_get_drvdata(dma));
	dma_set_drvdata(dma, NULL);
	return 0;
}

const struct dma_ops dw_dma_ops = {
	.channel_get	= dw_dma_channel_get,
	.channel_put	= dw_dma_channel_put,
	.start		= dw_dma_start,
	.stop		= dw_dma_stop,
	.pause		= dw_dma_pause,
	.release	= dw_dma_release,
	.status		= dw_dma_status,
	.set_config	= dw_dma_set_config,
	.set_cb		= dw_dma_set_cb,
	.pm_context_restore		= dw_dma_pm_context_restore,
	.pm_context_store		= dw_dma_pm_context_store,
	.probe		= dw_dma_probe,
	.remove		= dw_dma_remove,
};
