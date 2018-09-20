/*
 * Copyright (c) 2018, Intel Corporation
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
 * Author: Keyon Jie <yang.jie@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <sof/atomic.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/trace.h>
#include <sof/dma.h>
#include <sof/io.h>
#include <sof/ipc.h>
#include <sof/pm_runtime.h>
#include <sof/wait.h>
#include <sof/audio/format.h>
#include <platform/dma.h>
#include <platform/platform.h>
#include <arch/cache.h>
#include <uapi/ipc.h>

#define trace_host(__e)	trace_event(TRACE_CLASS_HOST, __e)
#define tracev_host(__e)	tracev_event(TRACE_CLASS_HOST, __e)
#define trace_host_error(__e)	trace_error(TRACE_CLASS_HOST, __e)

/* Gateway Stream Registers */
#define DGCS		0x00
#define DGBBA		0x04
#define DGBS		0x08
#define DGBFPI		0x0c /* firmware need to update this when DGCS.FWCB=1 */
#define DGBRP		0x10 /* Read Only, read pointer */
#define DGBWP		0x14 /* Read Only, write pointer */
#define DGBSP		0x18
#define DGMBS		0x1c
#define DGLLPI		0x24
#define DGLPIBI		0x28

/* DGCS */
#define DGCS_SCS	BIT(31)
#define DGCS_GEN	BIT(26)
#define DGCS_FWCB	BIT(23)
#define DGCS_BSC	BIT(11)
#define DGCS_BOR	BIT(10) /* buffer overrun */
#define DGCS_BF		BIT(9)  /* buffer full */
#define DGCS_BNE	BIT(8)  /* buffer not empty */
#define DGCS_FIFORDY	BIT(5)  /* enable FIFO */

/* DGBBA */
#define DGBBA_MASK	0xffff80

/* DGBS */
#define DGBS_MASK	0xfffff0

#define HDA_DMA_MAX_CHANS		9

#define HDA_LINK_1MS_US	1000

#define HDA_STATE_PRELOAD	BIT(0)
#define HDA_STATE_BF_WAIT	BIT(1)

struct hda_chan_data {
	struct dma *dma;
	uint32_t index;
	uint32_t stream_id;
	uint32_t status;	/* common status */
	uint32_t state;		/* hda specific additional state */
	uint32_t desc_count;
	uint32_t desc_avail;
	uint32_t direction;

	uint32_t period_bytes;
	uint32_t buffer_bytes;
	struct work dma_ch_work;

	void (*cb)(void *data, uint32_t type,
		   struct dma_sg_elem *next); /* client callback */
	void *cb_data;		/* client callback data */
	int cb_type;		/* callback type */
};

struct dma_pdata {
	struct dma *dma;
	int32_t num_channels;
	struct hda_chan_data chan[HDA_DMA_MAX_CHANS];
};

static int hda_dma_stop(struct dma *dma, int channel);

static inline uint32_t host_dma_reg_read(struct dma *dma, uint32_t chan,
	uint32_t reg)
{
	return io_reg_read(dma_chan_base(dma, chan) + reg);
}

static inline void host_dma_reg_write(struct dma *dma, uint32_t chan,
	uint32_t reg, uint32_t value)
{
	io_reg_write(dma_chan_base(dma, chan) + reg, value);
}

static inline void hda_update_bits(struct dma *dma, uint32_t chan,
	uint32_t reg, uint32_t mask, uint32_t value)
{
	io_reg_update_bits(dma_chan_base(dma, chan) + reg,  mask, value);
}

static inline void hda_dma_inc_fp(struct dma *dma, uint32_t chan,
				  uint32_t value)
{
	host_dma_reg_write(dma, chan, DGBFPI, value);
	/* TODO: wp update, not rp should inc LLPI and LPIBI in the
	 * coupled input DMA
	 */
	host_dma_reg_write(dma, chan, DGLLPI, value);
	host_dma_reg_write(dma, chan, DGLPIBI, value);
}

static inline void hda_dma_inc_link_fp(struct dma *dma, uint32_t chan,
				       uint32_t value)
{
	host_dma_reg_write(dma, chan, DGBFPI, value);
	/* TODO: wp update should inc LLPI and LPIBI in the input DMA */
}

static inline uint32_t hda_dma_get_data_size(struct dma *dma, uint32_t chan)
{
	const uint32_t cs = host_dma_reg_read(dma, chan, DGCS);
	const uint32_t bs = host_dma_reg_read(dma, chan, DGBS);
	const uint32_t rp = host_dma_reg_read(dma, chan, DGBRP);
	const uint32_t wp = host_dma_reg_read(dma, chan, DGBRP);

	uint32_t ds;

	if (!(cs & DGCS_BNE))
		return 0; /* buffer is empty */
	ds = wp - rp;
	if (ds <= 0)
		ds += bs;

	return ds;
}

static inline uint32_t hda_dma_get_free_size(struct dma *dma, uint32_t chan)
{
	const uint32_t bs = host_dma_reg_read(dma, chan, DGBS);

	return bs - hda_dma_get_data_size(dma, chan);
}

static int hda_dma_preload(struct dma *dma, struct hda_chan_data *chan)
{
	struct dma_sg_elem next = {
			.src = DMA_RELOAD_LLI,
			.dest = DMA_RELOAD_LLI,
			.size = DMA_RELOAD_LLI
	};
	int i;
	int period_cnt;

	/* waiting for buffer full after start
	 * first try is unblocking, then blocking
	 */
	while (!(host_dma_reg_read(dma, chan->index, DGCS) & DGCS_BF) &&
	       (chan->state & HDA_STATE_BF_WAIT))
		;

	if (host_dma_reg_read(dma, chan->index, DGCS) & DGCS_BF) {
		chan->state &= ~(HDA_STATE_PRELOAD | HDA_STATE_BF_WAIT);
		if (chan->cb) {
			/* loop over each period */
			period_cnt = chan->buffer_bytes /
					chan->period_bytes;
			for (i = 0; i < period_cnt; i++)
				chan->cb(chan->cb_data,
					 DMA_IRQ_TYPE_LLIST, &next);
			/* do not need to test out next in this path */
		}
	} else {
		/* next call in pre-load state will be blocking */
		chan->state |= HDA_STATE_BF_WAIT;
	}

	return 0;
}

static int hda_dma_copy_ch(struct dma *dma, struct hda_chan_data *chan,
			   int bytes)
{
	struct dma_sg_elem next = {
			.src = DMA_RELOAD_LLI,
			.dest = DMA_RELOAD_LLI,
			.size = DMA_RELOAD_LLI
	};
	uint32_t flags;
	uint32_t dgcs = 0;

	tracev_host("GwU");

	/* clear link xruns */
	dgcs = host_dma_reg_read(dma, chan->index, DGCS);
	if (dgcs & DGCS_BOR)
		hda_update_bits(dma, chan->index,
				DGCS, DGCS_BOR, DGCS_BOR);

	/* make sure that previous transfer is complete */
	if (chan->direction == DMA_DIR_MEM_TO_DEV) {
		while (hda_dma_get_free_size(dma, chan->index) <
		       bytes)
			idelay(PLATFORM_DEFAULT_DELAY);
	}

	/*
	 * set BFPI to let host gateway knows we have read size,
	 * which will trigger next copy start.
	 */
	if (chan->direction == DMA_DIR_MEM_TO_DEV)
		hda_dma_inc_link_fp(dma, chan->index, bytes);
	else
		hda_dma_inc_fp(dma, chan->index, bytes);

	spin_lock_irq(&dma->lock, flags);
	if (chan->cb) {
		next.src = DMA_RELOAD_LLI;
		next.dest = DMA_RELOAD_LLI;
		next.size = DMA_RELOAD_LLI;
		chan->cb(chan->cb_data, DMA_IRQ_TYPE_LLIST, &next);
		if (next.size == DMA_RELOAD_END) {
			/* disable channel, finished */
			hda_dma_stop(dma, chan->index);
		}
	}
	spin_unlock_irq(&dma->lock, flags);

	/* Force Host DMA to exit L1 */
	pm_runtime_put(PM_RUNTIME_HOST_DMA_L1);

	return 0;
}

static uint64_t hda_dma_work(void *data, uint64_t delay)
{
	struct hda_chan_data *chan = (struct hda_chan_data *)data;

	hda_dma_copy_ch(chan->dma, chan, chan->period_bytes);
	/* next time to re-arm */
	return HDA_LINK_1MS_US;
}

/* notify DMA to copy bytes */
static int hda_dma_copy(struct dma *dma, int channel, int bytes, uint32_t flags)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct hda_chan_data *chan = p->chan + channel;

	if (flags & DMA_COPY_PRELOAD)
		chan->state |= HDA_STATE_PRELOAD;

	if (chan->state & HDA_STATE_PRELOAD)
		return hda_dma_preload(dma, chan);
	else
		return hda_dma_copy_ch(dma, chan, bytes);
}

/* acquire the specific DMA channel */
static int hda_dma_channel_get(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);

	trace_host("Dgt");

	/* use channel if it's free */
	if (p->chan[channel].status == COMP_STATE_INIT) {
		p->chan[channel].status = COMP_STATE_READY;

		atomic_add(&dma->num_channels_busy, 1);

		/* return channel */
		spin_unlock_irq(&dma->lock, flags);
		return channel;
	}

	/* DMAC has no free channels */
	spin_unlock_irq(&dma->lock, flags);
	trace_host_error("eG0");
	return -ENODEV;
}

/* channel must not be running when this is called */
static void hda_dma_channel_put_unlocked(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	/* set new state */
	p->chan[channel].status = COMP_STATE_INIT;
	p->chan[channel].state = 0;
	p->chan[channel].period_bytes = 0;
	p->chan[channel].buffer_bytes = 0;
	p->chan[channel].cb = NULL;
	p->chan[channel].cb_type = 0;
	p->chan[channel].cb_data = NULL;
	work_init(&p->chan[channel].dma_ch_work, NULL, NULL, 0);
}

/* channel must not be running when this is called */
static void hda_dma_channel_put(struct dma *dma, int channel)
{
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);
	hda_dma_channel_put_unlocked(dma, channel);
	spin_unlock_irq(&dma->lock, flags);

	atomic_sub(&dma->num_channels_busy, 1);
}

static int hda_dma_start(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;
	uint32_t dgcs;
	int ret = 0;

	spin_lock_irq(&dma->lock, flags);

	trace_host("DEn");

	/* is channel idle, disabled and ready ? */
	dgcs = host_dma_reg_read(dma, channel, DGCS);
	if (p->chan[channel].status != COMP_STATE_PREPARE ||
	    (dgcs & DGCS_GEN)) {
		ret = -EBUSY;
		trace_host_error("eS0");
		trace_error_value(dgcs);
		trace_error_value(p->chan[channel].status);
		goto out;
	}

	/* enable the channel */
	hda_update_bits(dma, channel, DGCS, DGCS_GEN | DGCS_FIFORDY,
			DGCS_GEN | DGCS_FIFORDY);

	/* full buffer is copied at startup */
	p->chan[channel].desc_avail = p->chan[channel].desc_count;

	pm_runtime_put(PM_RUNTIME_HOST_DMA_L1);

	/* activate timer if configured in cyclic mode */
	if (p->chan[channel].dma_ch_work.cb) {
		work_schedule_default(&p->chan[channel].dma_ch_work,
				      HDA_LINK_1MS_US);
	}

	/* start link output transfer now */
	if (p->chan[channel].direction == DMA_DIR_MEM_TO_DEV)
		hda_dma_inc_link_fp(dma, channel,
				    p->chan[channel].buffer_bytes);

out:
	spin_unlock_irq(&dma->lock, flags);
	return ret;
}

static int hda_dma_release(struct dma *dma, int channel)
{
	/* TODO: to be removed, no longer called by anyone */
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);

	trace_host("Dpr");

	/* resume and reload DMA */
	p->chan[channel].status = COMP_STATE_ACTIVE;

	spin_unlock_irq(&dma->lock, flags);
	return 0;
}

static int hda_dma_pause(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);

	trace_host("Dpa");

	if (p->chan[channel].status != COMP_STATE_ACTIVE)
		goto out;

	/* pause the channel */
	p->chan[channel].status = COMP_STATE_PAUSED;

out:
	spin_unlock_irq(&dma->lock, flags);
	return 0;
}

static int hda_dma_stop(struct dma *dma, int channel)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);

	trace_host("DDi");

	if (p->chan[channel].dma_ch_work.cb)
		work_cancel_default(&p->chan[channel].dma_ch_work);

	/* disable the channel */
	hda_update_bits(dma, channel, DGCS, DGCS_GEN | DGCS_FIFORDY, 0);
	p->chan[channel].status = COMP_STATE_PREPARE;

	spin_unlock_irq(&dma->lock, flags);
	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int hda_dma_status(struct dma *dma, int channel,
	struct dma_chan_status *status, uint8_t direction)
{
	struct dma_pdata *p = dma_get_drvdata(dma);

	status->state = p->chan[channel].status;
	status->r_pos =  host_dma_reg_read(dma, channel, DGBRP);
	status->w_pos = host_dma_reg_read(dma, channel, DGBWP);
	status->timestamp = timer_get_system(platform_timer);

	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int hda_dma_set_config(struct dma *dma, int channel,
	struct dma_sg_config *config)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_sg_elem *sg_elem;
	uint32_t buffer_addr = 0;
	uint32_t period_bytes = 0;
	uint32_t buffer_bytes = 0;
	uint32_t flags;
	uint32_t addr;
	uint32_t dgcs;
	int i;
	int ret = 0;

	spin_lock_irq(&dma->lock, flags);

	trace_host("Dsc");

	if (!config->elem_array.count) {
		trace_host_error("eD1");
		ret = -EINVAL;
		goto out;
	}

	/* default channel config */
	p->chan[channel].direction = config->direction;
	p->chan[channel].desc_count = config->elem_array.count;

	/* validate - HDA only supports continuous elems of same size  */
	for (i = 0; i < config->elem_array.count; i++) {
		sg_elem = config->elem_array.elems + i;

		if (config->direction == DMA_DIR_HMEM_TO_LMEM ||
		    config->direction == DMA_DIR_DEV_TO_MEM)
			addr = sg_elem->dest;
		else
			addr = sg_elem->src;

		/* make sure elem is continuous */
		if (buffer_addr && (buffer_addr + buffer_bytes) != addr) {
			trace_host_error("eD2");
			ret = -EINVAL;
			goto out;
		}

		/* make sure period_bytes are constant */
		if (period_bytes && period_bytes != sg_elem->size) {
			trace_host_error("eD3");
			ret = -EINVAL;
			goto out;
		}

		/* update counters */
		period_bytes = sg_elem->size;
		buffer_bytes += period_bytes;

		if (buffer_addr == 0)
			buffer_addr = addr;
	}
	/* buffer size must be multiple of hda dma burst size */
	if (buffer_bytes % PLATFORM_HDA_BUFFER_ALIGNMENT) {
		ret = -EINVAL;
		goto out;
	}

	p->chan[channel].period_bytes = period_bytes;
	p->chan[channel].buffer_bytes = buffer_bytes;

	/* initialize timer */
	if (config->cyclic) {
		work_init(&p->chan[channel].dma_ch_work, hda_dma_work,
			  &p->chan[channel], WORK_SYNC);
	}

	/* init channel in HW */
	host_dma_reg_write(dma, channel, DGBBA,  buffer_addr);
	host_dma_reg_write(dma, channel, DGBS,  buffer_bytes);

	if (config->direction == DMA_DIR_LMEM_TO_HMEM ||
	    config->direction == DMA_DIR_HMEM_TO_LMEM)
		host_dma_reg_write(dma, channel, DGMBS,
				   ALIGN_UP(buffer_bytes,
					    PLATFORM_HDA_BUFFER_ALIGNMENT));

	/* firmware control buffer */
	dgcs = DGCS_FWCB;

	/* set DGCS.SCS bit to 1 for 16bit(2B) container */
	if ((config->direction & (DMA_DIR_HMEM_TO_LMEM | DMA_DIR_DEV_TO_MEM) &&
	     config->dest_width <= 2) ||
	    (config->direction & (DMA_DIR_LMEM_TO_HMEM | DMA_DIR_MEM_TO_DEV) &&
	     config->src_width <= 2))
		dgcs |= DGCS_SCS;

	/* set DGCS.FIFORDY for output dma */
	if ((config->cyclic && config->direction == DMA_DIR_MEM_TO_DEV) ||
	    (!config->cyclic && config->direction == DMA_DIR_LMEM_TO_HMEM))
		dgcs |= DGCS_FIFORDY;

	host_dma_reg_write(dma, channel, DGCS, dgcs);

	p->chan[channel].status = COMP_STATE_PREPARE;
out:
	spin_unlock_irq(&dma->lock, flags);
	return ret;
}

/* restore DMA conext after leaving D3 */
static int hda_dma_pm_context_restore(struct dma *dma)
{
	return 0;
}

/* store DMA conext after leaving D3 */
static int hda_dma_pm_context_store(struct dma *dma)
{
	return 0;
}

static int hda_dma_set_cb(struct dma *dma, int channel, int type,
	void (*cb)(void *data, uint32_t type, struct dma_sg_elem *next),
	void *data)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);
	p->chan[channel].cb = cb;
	p->chan[channel].cb_data = data;
	p->chan[channel].cb_type = type;
	spin_unlock_irq(&dma->lock, flags);

	return 0;
}

static int hda_dma_probe(struct dma *dma)
{
	struct dma_pdata *hda_pdata;
	int i;
	struct hda_chan_data *chan;

	/* allocate private data */
	hda_pdata = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*hda_pdata));
	dma_set_drvdata(dma, hda_pdata);

	spinlock_init(&dma->lock);

	/* init channel status */
	chan = hda_pdata->chan;

	for (i = 0; i < HDA_DMA_MAX_CHANS; i++, chan++) {
		chan->dma = dma;
		chan->index = i;
		chan->status = COMP_STATE_INIT;
	}

	/* init number of channels draining */
	atomic_init(&dma->num_channels_busy, 0);

	return 0;
}

const struct dma_ops hda_host_dma_ops = {
	.channel_get	= hda_dma_channel_get,
	.channel_put	= hda_dma_channel_put,
	.start		= hda_dma_start,
	.stop		= hda_dma_stop,
	.copy		= hda_dma_copy,
	.pause		= hda_dma_pause,
	.release	= hda_dma_release,
	.status		= hda_dma_status,
	.set_config	= hda_dma_set_config,
	.set_cb		= hda_dma_set_cb,
	.pm_context_restore		= hda_dma_pm_context_restore,
	.pm_context_store		= hda_dma_pm_context_store,
	.probe		= hda_dma_probe,
};

const struct dma_ops hda_link_dma_ops = {
	.channel_get	= hda_dma_channel_get,
	.channel_put	= hda_dma_channel_put,
	.start		= hda_dma_start,
	.stop		= hda_dma_stop,
	.copy		= hda_dma_copy,
	.pause		= hda_dma_pause,
	.release	= hda_dma_release,
	.status		= hda_dma_status,
	.set_config	= hda_dma_set_config,
	.set_cb		= hda_dma_set_cb,
	.pm_context_restore		= hda_dma_pm_context_restore,
	.pm_context_store		= hda_dma_pm_context_store,
	.probe		= hda_dma_probe,
};

