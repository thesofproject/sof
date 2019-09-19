// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Keyon Jie <yang.jie@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/atomic.h>
#include <sof/audio/component.h>
#include <sof/bit.h>
#include <sof/drivers/hda-dma.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dma.h>
#include <sof/lib/pm_runtime.h>
#include <sof/platform.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define trace_hddma(__e, ...) \
	trace_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define tracev_hddma(__e, ...) \
	tracev_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define trace_hddma_error(__e, ...) \
	trace_error(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)

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

#define HDA_STATE_RELEASE	BIT(0)

/* DGMBS align value */
#define HDA_DMA_BUFFER_ALIGNMENT	0x20
#define HDA_DMA_COPY_ALIGNMENT		0x20

/*
 * DMA Pointer Trace
 *
 *
 * DMA pointer trace will output hardware DMA pointers and the BNE flag
 * for n samples after stream start.
 * It will also show current values on start/stop.
 * Additionally values after the last copy will be output on stop.
 *
 * The trace will output three 32-bit values and context info,
 * looking like this:
 * hda-dma-ptr-trace AAAAooBC DDDDEEEE FFFFGGGG <context info>
 * where:
 * o - unused
 * A - indicates the direction of the transfer
 * B - will be 1 if BNE was set before an operation
 * C - will be 1 if BNE was set after an operation
 * D - hardware write pointer before an operation
 * E - hardware write pointer after an operation
 * F - hardware read pointer before an operation
 * G - hardware read pointer after an operation
 */

#define HDA_DMA_PTR_DBG		0  /* trace DMA pointers */
#define HDA_DMA_PTR_DBG_NUM_CP	32 /* number of traces to output after start */

#if HDA_DMA_PTR_DBG

enum hda_dbg_src {
	HDA_DBG_HOST = 0, /* enables dma pointer traces for host */
	HDA_DBG_LINK,	  /* enables dma pointer traces for link */
	HDA_DBG_BOTH	  /* enables dma pointer traces for host and link */
};

#define HDA_DBG_SRC HDA_DBG_BOTH

enum hda_dbg_sample {
	HDA_DBG_PRE = 0,
	HDA_DBG_POST,

	HDA_DBG_MAX_SAMPLES
};

struct hda_dbg_data {
	uint16_t cur_sample;
	uint16_t last_wp[HDA_DBG_MAX_SAMPLES];
	uint16_t last_rp[HDA_DBG_MAX_SAMPLES];
	uint8_t last_bne[HDA_DBG_MAX_SAMPLES];
};
#endif

struct hda_chan_data {
	uint32_t stream_id;
	uint32_t state;		/* hda specific additional state */
	uint32_t desc_avail;

	uint32_t period_bytes;
	uint32_t buffer_bytes;

#if HDA_DMA_PTR_DBG
	struct hda_dbg_data dbg_data;
#endif
};

static int hda_dma_stop(struct dma_chan_data *channel);

static inline void hda_dma_inc_fp(struct dma_chan_data *chan,
				  uint32_t value)
{
	dma_chan_reg_write(chan, DGBFPI, value);
	/* TODO: wp update, not rp should inc LLPI and LPIBI in the
	 * coupled input DMA
	 */
	dma_chan_reg_write(chan, DGLLPI, value);
	dma_chan_reg_write(chan, DGLPIBI, value);
}

static inline void hda_dma_inc_link_fp(struct dma_chan_data *chan,
				       uint32_t value)
{
	dma_chan_reg_write(chan, DGBFPI, value);
	/* TODO: wp update should inc LLPI and LPIBI in the input DMA */
}

#if HDA_DMA_PTR_DBG

static void hda_dma_dbg_count_reset(struct dma_chan_data *chan)
{
	struct hda_chan_data *hda_chan = dma_chan_get_data(chan);

	hda_chan->dbg_data.cur_sample = 0;
}

static void hda_dma_get_dbg_vals(struct dma_chan_data *chan,
				 enum hda_dbg_sample sample,
				 enum hda_dbg_src src)
{
	struct hda_chan_data *hda_chan = dma_chan_get_data(chan);
	struct hda_dbg_data *dbg_data = &hda_chan->dbg_data;

	if ((HDA_DBG_SRC == HDA_DBG_BOTH) || (src == HDA_DBG_BOTH) ||
	    (src == HDA_DBG_SRC)) {
		dbg_data->last_wp[sample] = dma_chan_reg_read(chan, DGBWP);
		dbg_data->last_rp[sample] = dma_chan_reg_read(chan, DGBRP);
		dbg_data->last_bne[sample] = (dma_chan_reg_read(chan, DGCS) &
				DGCS_BNE) ? 1 : 0;
	}
}

#define hda_dma_ptr_trace(chan, postfix, src) \
	do { \
		struct hda_chan_data *hda_chan = dma_chan_get_data(chan); \
		struct hda_dbg_data *dbg_data = &(hda_chan)->dbg_data; \
		if ((HDA_DBG_SRC == HDA_DBG_BOTH) || (src == HDA_DBG_BOTH) || \
			(src == HDA_DBG_SRC)) { \
			if (dbg_data->cur_sample < HDA_DMA_PTR_DBG_NUM_CP) { \
				uint32_t bne = merge_4b4b(\
					dbg_data->last_bne[HDA_DBG_PRE], \
					dbg_data->last_bne[HDA_DBG_POST]); \
				uint32_t info = \
					merge_16b16b((chan)->direction, bne); \
				uint32_t wp = merge_16b16b(\
					dbg_data->last_wp[HDA_DBG_PRE], \
					dbg_data->last_wp[HDA_DBG_POST]); \
				uint32_t rp = merge_16b16b(\
					dbg_data->last_rp[HDA_DBG_PRE], \
					dbg_data->last_rp[HDA_DBG_POST]); \
				trace_hddma("hda-dma-ptr-trace %08X %08X " \
					"%08X " postfix, info, wp, rp); \
				++dbg_data->cur_sample; \
			} \
		} \
	} while (0)
#else
#define hda_dma_dbg_count_reset(...)
#define hda_dma_get_dbg_vals(...)
#define hda_dma_ptr_trace(...)
#endif

static inline int hda_dma_is_buffer_full(struct dma_chan_data *chan)
{
	return dma_chan_reg_read(chan, DGCS) & DGCS_BF;
}

static inline int hda_dma_is_buffer_empty(struct dma_chan_data *chan)
{
	return !(dma_chan_reg_read(chan, DGCS) & DGCS_BNE);
}

static int hda_dma_wait_for_buffer_full(struct dma_chan_data *chan)
{
	uint64_t deadline = platform_timer_get(platform_timer) +
		clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
		PLATFORM_HOST_DMA_TIMEOUT / 1000;

	while (!hda_dma_is_buffer_full(chan)) {
		if (deadline < platform_timer_get(platform_timer)) {
			/* safe check in case we've got preempted after read */
			if (hda_dma_is_buffer_full(chan))
				return 0;

			trace_hddma_error("hda-dmac: %d wait for buffer full "
					  "timeout rp 0x%x wp 0x%x",
					  chan->dma->plat_data.id,
					  dma_chan_reg_read(chan, DGBRP),
					  dma_chan_reg_read(chan, DGBWP));
			return -ETIME;
		}
	}

	return 0;
}

static int hda_dma_wait_for_buffer_empty(struct dma_chan_data *chan)
{
	uint64_t deadline = platform_timer_get(platform_timer) +
		clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
		PLATFORM_HOST_DMA_TIMEOUT / 1000;

	while (!hda_dma_is_buffer_empty(chan)) {
		if (deadline < platform_timer_get(platform_timer)) {
			/* safe check in case we've got preempted after read */
			if (hda_dma_is_buffer_empty(chan))
				return 0;

			trace_hddma_error("hda-dmac: %d wait for buffer empty "
					  "timeout rp 0x%x wp 0x%x",
				chan->dma->plat_data.id,
				dma_chan_reg_read(chan, DGBRP),
				dma_chan_reg_read(chan, DGBWP));
			return -ETIME;
		}
	}

	return 0;
}

static void hda_dma_post_copy(struct dma_chan_data *chan, int bytes)
{
	struct dma_cb_data next = { .elem = { .size = bytes } };

	if (chan->cb)
		chan->cb(chan->cb_data, DMA_CB_TYPE_COPY, &next);

	/* Force Host DMA to exit L1 */
	if (chan->direction == DMA_DIR_HMEM_TO_LMEM ||
	    chan->direction == DMA_DIR_LMEM_TO_HMEM)
		pm_runtime_put(PM_RUNTIME_HOST_DMA_L1, 0);
}

static int hda_dma_link_copy_ch(struct dma_chan_data *chan, int bytes)
{
	uint32_t dgcs = 0;
	int ret = 0;

	tracev_hddma("hda-dmac: %d channel %d -> copy 0x%x bytes",
		     chan->dma->plat_data.id, chan->index, bytes);

	hda_dma_get_dbg_vals(chan, HDA_DBG_PRE, HDA_DBG_LINK);

	/* clear link xruns */
	dgcs = dma_chan_reg_read(chan, DGCS);
	if (dgcs & DGCS_BOR)
		dma_chan_reg_update_bits(chan, DGCS, DGCS_BOR, DGCS_BOR);

	/*
	 * set BFPI to let link gateway know we have read size,
	 * which will trigger next copy start.
	 */
	hda_dma_inc_link_fp(chan, bytes);
	hda_dma_post_copy(chan, bytes);

	hda_dma_get_dbg_vals(chan, HDA_DBG_POST, HDA_DBG_LINK);
	hda_dma_ptr_trace(chan, "link copy", HDA_DBG_LINK);

	return ret;
}

/* lock should be held by caller */
static void hda_dma_enable_unlock(struct dma_chan_data *channel)
{
	struct hda_chan_data *hda_chan;

	trace_hddma("hda-dmac: %d channel %d -> enable",
		    channel->dma->plat_data.id, channel->index);

	hda_dma_get_dbg_vals(channel, HDA_DBG_PRE, HDA_DBG_BOTH);

	/* enable the channel */
	dma_chan_reg_update_bits(channel, DGCS, DGCS_GEN | DGCS_FIFORDY,
				 DGCS_GEN | DGCS_FIFORDY);

	/* full buffer is copied at startup */
	hda_chan = dma_chan_get_data(channel);
	hda_chan->desc_avail = channel->desc_count;

	/* Force Host DMA to exit L1 */
	if (channel->direction == DMA_DIR_HMEM_TO_LMEM ||
	    channel->direction == DMA_DIR_LMEM_TO_HMEM)
		pm_runtime_put(PM_RUNTIME_HOST_DMA_L1, 0);

	/* start link output transfer now */
	if (channel->direction == DMA_DIR_MEM_TO_DEV &&
	    !(hda_chan->state & HDA_STATE_RELEASE))
		hda_dma_inc_link_fp(channel, hda_chan->buffer_bytes);

	hda_chan->state &= ~HDA_STATE_RELEASE;

	hda_dma_get_dbg_vals(channel, HDA_DBG_POST, HDA_DBG_BOTH);
	hda_dma_ptr_trace(channel, "enable", HDA_DBG_BOTH);
}

/* notify DMA to copy bytes */
static int hda_dma_link_copy(struct dma_chan_data *channel, int bytes,
			     uint32_t flags)
{
	return hda_dma_link_copy_ch(channel, bytes);
}

/* notify DMA to copy bytes */
static int hda_dma_host_copy(struct dma_chan_data *channel, int bytes,
			     uint32_t flags)
{
	int ret;

	tracev_hddma("hda-dmac: %d channel %d -> copy 0x%x bytes",
		     channel->dma->plat_data.id, channel->index, bytes);

	hda_dma_get_dbg_vals(channel, HDA_DBG_PRE, HDA_DBG_HOST);

	if (flags & DMA_COPY_PRELOAD) {
		/* report lack of data if preload is not yet finished */
		ret = channel->direction == DMA_DIR_HMEM_TO_LMEM ?
			hda_dma_is_buffer_full(channel) :
			hda_dma_is_buffer_empty(channel);
		if (!ret)
			return -ENODATA;
	} else {
		/* set BFPI to let host gateway know we have read size,
		 * which will trigger next copy start.
		 */
		hda_dma_inc_fp(channel, bytes);
	}

	/* blocking mode copy */
	if (flags & DMA_COPY_BLOCKING) {
		ret = channel->direction == DMA_DIR_HMEM_TO_LMEM ?
			hda_dma_wait_for_buffer_full(channel) :
			hda_dma_wait_for_buffer_empty(channel);
		if (ret < 0)
			return ret;
	}

	hda_dma_post_copy(channel, bytes);

	hda_dma_get_dbg_vals(channel, HDA_DBG_POST, HDA_DBG_HOST);
	hda_dma_ptr_trace(channel, "host copy", HDA_DBG_HOST);

	return 0;
}

/* acquire the specific DMA channel */
static struct dma_chan_data *hda_dma_channel_get(struct dma *dma,
						 unsigned int channel)
{
	uint32_t flags;

	if (channel >= dma->plat_data.channels) {
		trace_hddma_error("hda-dmac: %d invalid channel %d",
				  dma->plat_data.id, channel);
		return NULL;
	}

	spin_lock_irq(dma->lock, flags);

	trace_hddma("hda-dmac: %d channel %d -> get", dma->plat_data.id,
		    channel);

	/* use channel if it's free */
	if (dma->chan[channel].status == COMP_STATE_INIT) {
		dma->chan[channel].status = COMP_STATE_READY;

		atomic_add(&dma->num_channels_busy, 1);

		/* return channel */
		spin_unlock_irq(dma->lock, flags);
		return &dma->chan[channel];
	}

	/* DMAC has no free channels */
	spin_unlock_irq(dma->lock, flags);
	trace_hddma_error("hda-dmac: %d no free channel %d", dma->plat_data.id,
			  channel);
	return NULL;
}

/* channel must not be running when this is called */
static void hda_dma_channel_put_unlocked(struct dma_chan_data *channel)
{
	struct hda_chan_data *hda_chan = dma_chan_get_data(channel);

	/* set new state */
	channel->status = COMP_STATE_INIT;
	hda_chan->state = 0;
	hda_chan->period_bytes = 0;
	hda_chan->buffer_bytes = 0;
	channel->cb = NULL;
	channel->cb_type = 0;
	channel->cb_data = NULL;
}

/* channel must not be running when this is called */
static void hda_dma_channel_put(struct dma_chan_data *channel)
{
	struct dma *dma = channel->dma;
	uint32_t flags;

	spin_lock_irq(dma->lock, flags);
	hda_dma_channel_put_unlocked(channel);
	spin_unlock_irq(dma->lock, flags);

	atomic_sub(&dma->num_channels_busy, 1);
}

static int hda_dma_start(struct dma_chan_data *channel)
{
	uint32_t flags;
	uint32_t dgcs;
	int ret = 0;

	irq_local_disable(flags);

	trace_hddma("hda-dmac: %d channel %d -> start",
		    channel->dma->plat_data.id, channel->index);

	hda_dma_dbg_count_reset(channel);

	/* is channel idle, disabled and ready ? */
	dgcs = dma_chan_reg_read(channel, DGCS);
	if (channel->status != COMP_STATE_PREPARE || (dgcs & DGCS_GEN)) {
		ret = -EBUSY;
		trace_hddma_error("hda-dmac: %d channel %d busy. dgcs 0x%x "
				  "status %d", channel->dma->plat_data.id,
				  channel->index, dgcs, channel->status);
		goto out;
	}

	hda_dma_enable_unlock(channel);

	channel->status = COMP_STATE_ACTIVE;
	channel->core = cpu_get_id();

out:
	irq_local_enable(flags);
	return ret;
}

static int hda_dma_release(struct dma_chan_data *channel)
{
	struct hda_chan_data *hda_chan = dma_chan_get_data(channel);
	uint32_t flags;

	irq_local_disable(flags);

	trace_hddma("hda-dmac: %d channel %d -> release",
		    channel->dma->plat_data.id, channel->index);

	/*
	 * Prepare for the handling of release condition on the first work cb.
	 * This flag will be unset afterwards.
	 */
	hda_chan->state |= HDA_STATE_RELEASE;

	irq_local_enable(flags);
	return 0;
}

static int hda_dma_pause(struct dma_chan_data *channel)
{
	uint32_t flags;

	irq_local_disable(flags);

	trace_hddma("hda-dmac: %d channel %d -> pause",
		    channel->dma->plat_data.id, channel->index);

	if (channel->status != COMP_STATE_ACTIVE)
		goto out;

	/* pause the channel */
	channel->status = COMP_STATE_PAUSED;

out:
	irq_local_enable(flags);
	return 0;
}

static int hda_dma_stop(struct dma_chan_data *channel)
{
	struct hda_chan_data *hda_chan;
	uint32_t flags;

	irq_local_disable(flags);

	hda_dma_dbg_count_reset(channel);
	hda_dma_ptr_trace(channel, "last-copy", HDA_DBG_BOTH);
	hda_dma_get_dbg_vals(channel, HDA_DBG_PRE, HDA_DBG_BOTH);

	trace_hddma("hda-dmac: %d channel %d -> stop",
		    channel->dma->plat_data.id, channel->index);

	/* disable the channel */
	dma_chan_reg_update_bits(channel, DGCS, DGCS_GEN | DGCS_FIFORDY, 0);
	channel->status = COMP_STATE_PREPARE;
	hda_chan = dma_chan_get_data(channel);
	hda_chan->state = 0;

	hda_dma_get_dbg_vals(channel, HDA_DBG_POST, HDA_DBG_BOTH);
	hda_dma_ptr_trace(channel, "stop", HDA_DBG_BOTH);

	irq_local_enable(flags);
	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int hda_dma_status(struct dma_chan_data *channel,
			  struct dma_chan_status *status, uint8_t direction)
{
	status->state = channel->status;
	status->r_pos = dma_chan_reg_read(channel, DGBRP);
	status->w_pos = dma_chan_reg_read(channel, DGBWP);
	status->timestamp = timer_get_system(platform_timer);

	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int hda_dma_set_config(struct dma_chan_data *channel,
			      struct dma_sg_config *config)
{
	struct dma *dma = channel->dma;
	struct hda_chan_data *hda_chan;
	struct dma_sg_elem *sg_elem;
	uint32_t buffer_addr = 0;
	uint32_t period_bytes = 0;
	uint32_t buffer_bytes = 0;
	uint32_t flags;
	uint32_t addr;
	uint32_t dgcs;
	int i;
	int ret = 0;

	if (channel->status == COMP_STATE_ACTIVE)
		return 0;

	irq_local_disable(flags);

	trace_hddma("hda-dmac: %d channel %d -> config",
		    dma->plat_data.id, channel->index);

	if (!config->elem_array.count) {
		trace_hddma_error("hda-dmac: %d channel %d no DMA descriptors",
				  dma->plat_data.id, channel->index);
		ret = -EINVAL;
		goto out;
	}

	if ((config->direction & (DMA_DIR_MEM_TO_DEV | DMA_DIR_DEV_TO_MEM)) &&
	    !config->irq_disabled) {
		trace_hddma_error("hda-dmac: %d channel %d HDA Link DMA "
				  "doesn't support irq scheduling",
				  dma->plat_data.id, channel->index);
		ret = -EINVAL;
		goto out;
	}

	/* default channel config */
	channel->direction = config->direction;
	channel->desc_count = config->elem_array.count;
	channel->is_scheduling_source = config->is_scheduling_source;
	channel->period = config->period;

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
			trace_hddma_error("hda-dmac: %d chan %d - "
					  "non continuous elem",
					  dma->plat_data.id, channel->index);
			trace_hddma_error(" addr 0x%x buffer 0x%x size 0x%x",
					  addr, buffer_addr, buffer_bytes);
			ret = -EINVAL;
			goto out;
		}

		/* make sure period_bytes are constant */
		if (period_bytes && period_bytes != sg_elem->size) {
			trace_hddma_error("hda-dmac: %d chan %d - period size "
					  "not constant %d", dma->plat_data.id,
					  channel->index, period_bytes);
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
	if (buffer_bytes % HDA_DMA_BUFFER_ALIGNMENT) {
		trace_hddma_error("hda-dmac: %d chan %d - buffer not DMA "
				  "aligned 0x%x", dma->plat_data.id,
				  channel->index, buffer_bytes);
		ret = -EINVAL;
		goto out;
	}
	hda_chan = dma_chan_get_data(channel);
	hda_chan->period_bytes = period_bytes;
	hda_chan->buffer_bytes = buffer_bytes;

	/* init channel in HW */
	dma_chan_reg_write(channel, DGBBA, buffer_addr);
	dma_chan_reg_write(channel, DGBS, buffer_bytes);

	if (config->direction == DMA_DIR_LMEM_TO_HMEM ||
	    config->direction == DMA_DIR_HMEM_TO_LMEM)
		dma_chan_reg_write(channel, DGMBS,
				   ALIGN_UP(buffer_bytes,
					    HDA_DMA_BUFFER_ALIGNMENT));

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

	dma_chan_reg_write(channel, DGCS, dgcs);

	channel->status = COMP_STATE_PREPARE;
out:
	irq_local_enable(flags);
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

static int hda_dma_set_cb(struct dma_chan_data *channel, int type,
	void (*cb)(void *data, uint32_t type, struct dma_cb_data *next),
	void *data)
{
	uint32_t flags;

	irq_local_disable(flags);
	channel->cb = cb;
	channel->cb_data = data;
	channel->cb_type = type;
	irq_local_enable(flags);

	return 0;
}

static int hda_dma_probe(struct dma *dma)
{
	struct hda_chan_data *hda_chan;
	int i;
	struct dma_chan_data *chan;

	trace_hddma("hda-dmac :%d -> probe", dma->plat_data.id);

	if (dma->chan)
		return -EEXIST; /* already created */

	dma->chan = rzalloc(RZONE_SYS_RUNTIME | RZONE_FLAG_UNCACHED,
			    SOF_MEM_CAPS_RAM, sizeof(struct dma_chan_data) *
			    dma->plat_data.channels);

	if (!dma->chan) {
		trace_hddma_error("hda-dmac: %d channels alloc failed",
				  dma->plat_data.id);
		goto out;
	}

	/* init channel status */
	chan = dma->chan;

	for (i = 0; i < dma->plat_data.channels; i++, chan++) {
		chan->dma = dma;
		chan->index = i;
		chan->status = COMP_STATE_INIT;
		chan->core = DMA_CORE_INVALID;

		hda_chan = rzalloc(RZONE_SYS_RUNTIME | RZONE_FLAG_UNCACHED,
				   SOF_MEM_CAPS_RAM,
				   sizeof(struct hda_chan_data));
		if (!hda_chan) {
			trace_hddma_error("hda-dma: %d channel %d private data "
					  "alloc failed", dma->plat_data.id, i);
			goto out;
		}

		dma_chan_set_data(chan, hda_chan);
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

static int hda_dma_remove(struct dma *dma)
{
	int i;

	trace_hddma("hda-dmac :%d -> remove", dma->plat_data.id);

	for (i = 0; i < dma->plat_data.channels; i++)
		rfree(dma_chan_get_data(&dma->chan[i]));

	rfree(dma->chan);
	dma->chan = NULL;

	return 0;
}

static int hda_dma_avail_data_size(struct dma_chan_data *chan)
{
	struct hda_chan_data *hda_chan = dma_chan_get_data(chan);
	uint32_t status;
	int32_t read_ptr;
	int32_t write_ptr;
	int size;

	status = dma_chan_reg_read(chan, DGCS);

	if (status & DGCS_BF)
		return hda_chan->buffer_bytes;

	if (!(status & DGCS_BNE))
		return 0;

	read_ptr = dma_chan_reg_read(chan, DGBRP);
	write_ptr = dma_chan_reg_read(chan, DGBWP);

	size = write_ptr - read_ptr;
	if (size <= 0)
		size += hda_chan->buffer_bytes;

	return size;
}

static int hda_dma_free_data_size(struct dma_chan_data *chan)
{
	struct hda_chan_data *hda_chan = dma_chan_get_data(chan);
	uint32_t status;
	int32_t read_ptr;
	int32_t write_ptr;
	int size;

	status = dma_chan_reg_read(chan, DGCS);

	if (status & DGCS_BF)
		return 0;

	if (!(status & DGCS_BNE))
		return hda_chan->buffer_bytes;

	read_ptr = dma_chan_reg_read(chan, DGBRP);
	write_ptr = dma_chan_reg_read(chan, DGBWP);

	size = read_ptr - write_ptr;
	if (size <= 0)
		size += hda_chan->buffer_bytes;

	return size;
}

static int hda_dma_data_size(struct dma_chan_data *channel,
			     uint32_t *avail, uint32_t *free)
{
	uint32_t flags;

	tracev_hddma("hda-dmac: %d channel %d -> get_data_size",
		     channel->dma->plat_data.id, channel->index);

	irq_local_disable(flags);

	if (channel->direction == DMA_DIR_HMEM_TO_LMEM ||
	    channel->direction == DMA_DIR_DEV_TO_MEM)
		*avail = hda_dma_avail_data_size(channel);
	else
		*free = hda_dma_free_data_size(channel);

	irq_local_enable(flags);

	return 0;
}

static int hda_dma_get_attribute(struct dma *dma, uint32_t type,
				 uint32_t *value)
{
	int ret = 0;

	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
		*value = HDA_DMA_BUFFER_ALIGNMENT;
		break;
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = HDA_DMA_COPY_ALIGNMENT;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int hda_dma_interrupt(struct dma_chan_data *channel,
			     enum dma_irq_cmd cmd)
{
	/* HDA-DMA doesn't support interrupts */
	return -EINVAL;
}

const struct dma_ops hda_host_dma_ops = {
	.channel_get		= hda_dma_channel_get,
	.channel_put		= hda_dma_channel_put,
	.start			= hda_dma_start,
	.stop			= hda_dma_stop,
	.copy			= hda_dma_host_copy,
	.pause			= hda_dma_pause,
	.release		= hda_dma_release,
	.status			= hda_dma_status,
	.set_config		= hda_dma_set_config,
	.set_cb			= hda_dma_set_cb,
	.pm_context_restore	= hda_dma_pm_context_restore,
	.pm_context_store	= hda_dma_pm_context_store,
	.probe			= hda_dma_probe,
	.remove			= hda_dma_remove,
	.get_data_size		= hda_dma_data_size,
	.get_attribute		= hda_dma_get_attribute,
	.interrupt		= hda_dma_interrupt,
};

const struct dma_ops hda_link_dma_ops = {
	.channel_get		= hda_dma_channel_get,
	.channel_put		= hda_dma_channel_put,
	.start			= hda_dma_start,
	.stop			= hda_dma_stop,
	.copy			= hda_dma_link_copy,
	.pause			= hda_dma_pause,
	.release		= hda_dma_release,
	.status			= hda_dma_status,
	.set_config		= hda_dma_set_config,
	.set_cb			= hda_dma_set_cb,
	.pm_context_restore	= hda_dma_pm_context_restore,
	.pm_context_store	= hda_dma_pm_context_store,
	.probe			= hda_dma_probe,
	.remove			= hda_dma_remove,
	.get_data_size		= hda_dma_data_size,
	.get_attribute		= hda_dma_get_attribute,
	.interrupt		= hda_dma_interrupt,
};
