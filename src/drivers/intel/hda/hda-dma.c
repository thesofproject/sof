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
#include <sof/lib/notifier.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ee12fa71-4579-45d7-bde2-b32c6893a122 */
DECLARE_SOF_UUID("hda-dma", hda_dma_uuid, 0xee12fa71, 0x4579, 0x45d7,
		 0xbd, 0xe2, 0xb3, 0x2c, 0x68, 0x93, 0xa1, 0x22);

DECLARE_TR_CTX(hdma_tr, SOF_UUID(hda_dma_uuid), LOG_LEVEL_INFO);

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
/* NOTE: both XRUN bits are the same, just direction is different */
#define DGCS_BOR	BIT(10) /* buffer overrun (input streams) */
#define DGCS_BUR	BIT(10) /* buffer underrun (output streams) */
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
#define HDA_DMA_BUFFER_ADDRESS_ALIGNMENT 0x80

/* DMA host transfer timeout in microseconds */
#define HDA_DMA_TIMEOUT	200

/* DMA number of buffer periods */
#define HDA_DMA_BUFFER_PERIOD_COUNT	2

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

	bool irq_disabled;	/**< indicates whether channel is used by the
				  * pipeline scheduled on DMA
				  */
	bool l1_exit_needed;	/**< indicates if l1 exit needed at ll
				  * scheduler post run
				  */

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
				tr_info(&hdma_tr, \
					"hda-dma-ptr-trace %08X %08X %08X " \
					postfix, info, wp, rp); \
				++dbg_data->cur_sample; \
			} \
		} \
	} while (0)
#else /* HDA_DMA_PTR_DBG */
#define hda_dma_dbg_count_reset(...)
#define hda_dma_get_dbg_vals(...)
#define hda_dma_ptr_trace(...)
#endif

static void hda_dma_l1_exit_notify(void *arg, enum notify_id type, void *data)
{
	struct hda_chan_data *hda_chan = arg;

	/* Force Host DMA to exit L1 if needed */
	if (hda_chan->l1_exit_needed) {
		pm_runtime_put(PM_RUNTIME_HOST_DMA_L1, 0);
		hda_chan->l1_exit_needed = false;
	}
}

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
	struct timer *timer = timer_get();
	uint64_t deadline = platform_timer_get(timer) +
		clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
		HDA_DMA_TIMEOUT / 1000;

	while (!hda_dma_is_buffer_full(chan)) {
		if (deadline < platform_timer_get(timer)) {
			/* safe check in case we've got preempted after read */
			if (hda_dma_is_buffer_full(chan))
				return 0;

			tr_err(&hdma_tr, "hda-dmac: %d wait for buffer full timeout rp 0x%x wp 0x%x",
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
	struct timer *timer = timer_get();
	uint64_t deadline = platform_timer_get(timer) +
		clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
		HDA_DMA_TIMEOUT / 1000;

	while (!hda_dma_is_buffer_empty(chan)) {
		if (deadline < platform_timer_get(timer)) {
			/* safe check in case we've got preempted after read */
			if (hda_dma_is_buffer_empty(chan))
				return 0;

			tr_err(&hdma_tr, "hda-dmac: %d wait for buffer empty timeout rp 0x%x wp 0x%x",
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
	struct hda_chan_data *hda_chan = dma_chan_get_data(chan);
	struct dma_cb_data next = {
		.channel = chan,
		.elem = { .size = bytes },
	};

	notifier_event(chan, NOTIFIER_ID_DMA_COPY,
		       NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));

	if (chan->direction == DMA_DIR_HMEM_TO_LMEM ||
	    chan->direction == DMA_DIR_LMEM_TO_HMEM) {
		/* set BFPI to let host gateway know we have read size,
		 * which will trigger next copy start.
		 */
		hda_dma_inc_fp(chan, bytes);

		/*
		 * Force Host DMA to exit L1 if scheduled on DMA,
		 * otherwise, perform L1 exit at LL scheduler powt run.
		 */
		if (!hda_chan->irq_disabled)
			pm_runtime_put(PM_RUNTIME_HOST_DMA_L1, 0);
		else if (bytes)
			hda_chan->l1_exit_needed = true;
	} else {
		/*
		 * set BFPI to let link gateway know we have read size,
		 * which will trigger next copy start.
		 */
		hda_dma_inc_link_fp(chan, bytes);
	}
}

static int hda_dma_link_copy_ch(struct dma_chan_data *chan, int bytes)
{
	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> copy 0x%x bytes",
	       chan->dma->plat_data.id, chan->index, bytes);

	hda_dma_get_dbg_vals(chan, HDA_DBG_PRE, HDA_DBG_LINK);

	hda_dma_post_copy(chan, bytes);

	hda_dma_get_dbg_vals(chan, HDA_DBG_POST, HDA_DBG_LINK);
	hda_dma_ptr_trace(chan, "link copy", HDA_DBG_LINK);

	return 0;
}

static int hda_dma_host_start(struct dma_chan_data *channel)
{
	struct hda_chan_data *hda_chan = dma_chan_get_data(channel);
	int ret = 0;

	/* Force Host DMA to exit L1 only on start*/
	if (!(hda_chan->state & HDA_STATE_RELEASE))
		pm_runtime_put(PM_RUNTIME_HOST_DMA_L1, 0);

	if (!hda_chan->irq_disabled)
		return ret;

	/* Register common L1 exit for all channels */
	ret = notifier_register(hda_chan, scheduler_get_data(SOF_SCHEDULE_LL_TIMER),
				NOTIFIER_ID_LL_POST_RUN, hda_dma_l1_exit_notify,
				NOTIFIER_FLAG_AGGREGATE);
	if (ret < 0)
		tr_err(&hdma_tr, "hda-dmac: %d channel %d, cannot register notification %d",
		       channel->dma->plat_data.id, channel->index,
		       ret);

	return ret;
}

static void hda_dma_host_stop(struct dma_chan_data *channel)
{
	struct hda_chan_data *hda_chan = dma_chan_get_data(channel);

	if (!hda_chan->irq_disabled)
		return;

	/* Unregister L1 entry */
	notifier_unregister(NULL, scheduler_get_data(SOF_SCHEDULE_LL_TIMER),
			    NOTIFIER_ID_LL_PRE_RUN);

	/* Unregister L1 exit */
	notifier_unregister(NULL, scheduler_get_data(SOF_SCHEDULE_LL_TIMER),
			    NOTIFIER_ID_LL_POST_RUN);
}

/* lock should be held by caller */
static int hda_dma_enable_unlock(struct dma_chan_data *channel)
{
	struct hda_chan_data *hda_chan;
	int ret;

	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> enable",
		channel->dma->plat_data.id, channel->index);

	hda_dma_get_dbg_vals(channel, HDA_DBG_PRE, HDA_DBG_BOTH);

	/* enable the channel */
	dma_chan_reg_update_bits(channel, DGCS, DGCS_GEN | DGCS_FIFORDY,
				 DGCS_GEN | DGCS_FIFORDY);

	/* full buffer is copied at startup */
	hda_chan = dma_chan_get_data(channel);
	hda_chan->desc_avail = channel->desc_count;

	if (channel->direction == DMA_DIR_HMEM_TO_LMEM ||
	    channel->direction == DMA_DIR_LMEM_TO_HMEM) {
		pm_runtime_get(PM_RUNTIME_HOST_DMA_L1, 0);
		ret = hda_dma_host_start(channel);
		if (ret < 0)
			return ret;
	}

	/* start link output transfer now */
	if (channel->direction == DMA_DIR_MEM_TO_DEV &&
	    !(hda_chan->state & HDA_STATE_RELEASE))
		hda_dma_inc_link_fp(channel, hda_chan->buffer_bytes);

	hda_chan->state &= ~HDA_STATE_RELEASE;

	hda_dma_get_dbg_vals(channel, HDA_DBG_POST, HDA_DBG_BOTH);
	hda_dma_ptr_trace(channel, "enable", HDA_DBG_BOTH);

	return 0;
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

	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> copy 0x%x bytes",
	       channel->dma->plat_data.id, channel->index, bytes);

	hda_dma_get_dbg_vals(channel, HDA_DBG_PRE, HDA_DBG_HOST);

	/* Register Host DMA usage */
	pm_runtime_get(PM_RUNTIME_HOST_DMA_L1, 0);

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
		tr_err(&hdma_tr, "hda-dmac: %d invalid channel %d",
		       dma->plat_data.id, channel);
		return NULL;
	}

	spin_lock_irq(&dma->lock, flags);

	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> get", dma->plat_data.id, channel);

	/* use channel if it's free */
	if (dma->chan[channel].status == COMP_STATE_INIT) {
		dma->chan[channel].status = COMP_STATE_READY;

		atomic_add(&dma->num_channels_busy, 1);

		/* return channel */
		spin_unlock_irq(&dma->lock, flags);
		return &dma->chan[channel];
	}

	/* DMAC has no free channels */
	spin_unlock_irq(&dma->lock, flags);
	tr_err(&hdma_tr, "hda-dmac: %d no free channel %d", dma->plat_data.id,
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

	/* Make sure that all callbacks to this channel are freed */
	notifier_unregister_all(NULL, channel);
}

/* channel must not be running when this is called */
static void hda_dma_channel_put(struct dma_chan_data *channel)
{
	struct dma *dma = channel->dma;
	uint32_t flags;

	spin_lock_irq(&dma->lock, flags);
	hda_dma_channel_put_unlocked(channel);
	spin_unlock_irq(&dma->lock, flags);

	atomic_sub(&dma->num_channels_busy, 1);
}

static int hda_dma_start(struct dma_chan_data *channel)
{
	uint32_t flags;
	uint32_t dgcs;
	int ret = 0;

	irq_local_disable(flags);

	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> start",
	       channel->dma->plat_data.id, channel->index);

	hda_dma_dbg_count_reset(channel);

	/* is channel idle, disabled and ready ? */
	dgcs = dma_chan_reg_read(channel, DGCS);
	if (channel->status != COMP_STATE_PREPARE || (dgcs & DGCS_GEN)) {
		ret = -EBUSY;
		tr_err(&hdma_tr, "hda-dmac: %d channel %d busy. dgcs 0x%x status %d",
		       channel->dma->plat_data.id,
		       channel->index, dgcs, channel->status);
		goto out;
	}

	ret = hda_dma_enable_unlock(channel);
	if (ret < 0)
		goto out;

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
	int ret = 0;

	irq_local_disable(flags);

	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> release",
	       channel->dma->plat_data.id, channel->index);

	/*
	 * Prepare for the handling of release condition on the first work cb.
	 * This flag will be unset afterwards.
	 */
	hda_chan->state |= HDA_STATE_RELEASE;

	if (channel->direction == DMA_DIR_HMEM_TO_LMEM ||
	    channel->direction == DMA_DIR_LMEM_TO_HMEM)
		ret = hda_dma_host_start(channel);

	irq_local_enable(flags);
	return ret;
}

static int hda_dma_pause(struct dma_chan_data *channel)
{
	uint32_t flags;

	irq_local_disable(flags);

	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> pause",
	       channel->dma->plat_data.id, channel->index);

	if (channel->status != COMP_STATE_ACTIVE)
		goto out;

	/* stop the channel */
	hda_dma_stop(channel);

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

	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> stop",
	       channel->dma->plat_data.id, channel->index);

	if (channel->direction == DMA_DIR_HMEM_TO_LMEM ||
	    channel->direction == DMA_DIR_LMEM_TO_HMEM)
		hda_dma_host_stop(channel);

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
	status->timestamp = timer_get_system(timer_get());

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

	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> config", dma->plat_data.id, channel->index);

	if (!config->elem_array.count) {
		tr_err(&hdma_tr, "hda-dmac: %d channel %d no DMA descriptors",
		       dma->plat_data.id, channel->index);
		ret = -EINVAL;
		goto out;
	}

	if ((config->direction & (DMA_DIR_MEM_TO_DEV | DMA_DIR_DEV_TO_MEM)) &&
	    !config->irq_disabled) {
		tr_err(&hdma_tr, "hda-dmac: %d channel %d HDA Link DMA doesn't support irq scheduling",
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
			tr_err(&hdma_tr, "hda-dmac: %d chan %d - non continuous elem",
			       dma->plat_data.id, channel->index);
			tr_err(&hdma_tr, " addr 0x%x buffer 0x%x size 0x%x",
			       addr, buffer_addr, buffer_bytes);
			ret = -EINVAL;
			goto out;
		}

		/* make sure period_bytes are constant */
		if (period_bytes && period_bytes != sg_elem->size) {
			tr_err(&hdma_tr, "hda-dmac: %d chan %d - period size not constant %d",
			       dma->plat_data.id,
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
		tr_err(&hdma_tr, "hda-dmac: %d chan %d - buffer not DMA aligned 0x%x",
		       dma->plat_data.id,
		       channel->index, buffer_bytes);
		ret = -EINVAL;
		goto out;
	}
	hda_chan = dma_chan_get_data(channel);
	hda_chan->period_bytes = period_bytes;
	hda_chan->buffer_bytes = buffer_bytes;
	hda_chan->irq_disabled = config->irq_disabled;

	/* init channel in HW */
	dma_chan_reg_write(channel, DGBBA, buffer_addr);
	dma_chan_reg_write(channel, DGBS, buffer_bytes);

	if (config->direction == DMA_DIR_LMEM_TO_HMEM ||
	    config->direction == DMA_DIR_HMEM_TO_LMEM)
		dma_chan_reg_write(channel, DGMBS,
				   ALIGN_UP_COMPILE(buffer_bytes,
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

static int hda_dma_probe(struct dma *dma)
{
	struct hda_chan_data *hda_chan;
	int i;
	struct dma_chan_data *chan;

	tr_info(&hdma_tr, "hda-dmac :%d -> probe", dma->plat_data.id);

	if (dma->chan)
		return -EEXIST; /* already created */

	dma->chan = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			    sizeof(struct dma_chan_data) * dma->plat_data.channels);

	if (!dma->chan) {
		tr_err(&hdma_tr, "hda-dmac: %d channels alloc failed",
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

		hda_chan = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0,
				   SOF_MEM_CAPS_RAM,
				   sizeof(struct hda_chan_data));
		if (!hda_chan) {
			tr_err(&hdma_tr, "hda-dma: %d channel %d private data alloc failed",
			       dma->plat_data.id, i);
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

	tr_info(&hdma_tr, "hda-dmac :%d -> remove", dma->plat_data.id);

	for (i = 0; i < dma->plat_data.channels; i++)
		rfree(dma_chan_get_data(&dma->chan[i]));

	rfree(dma->chan);
	dma->chan = NULL;

	return 0;
}

static int hda_dma_link_check_xrun(struct dma_chan_data *chan)
{
	uint32_t dgcs = dma_chan_reg_read(chan, DGCS);

	if (chan->direction == DMA_DIR_MEM_TO_DEV && dgcs & DGCS_BUR) {
		tr_err(&hdma_tr, "hda_dma_link_check_xrun(): underrun detected");
		dma_chan_reg_update_bits(chan, DGCS, DGCS_BUR, DGCS_BUR);
	} else if (chan->direction == DMA_DIR_DEV_TO_MEM && dgcs & DGCS_BOR) {
		tr_err(&hdma_tr, "hda_dma_link_check_xrun(): overrun detected");
		dma_chan_reg_update_bits(chan, DGCS, DGCS_BOR, DGCS_BOR);
	}

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
	int ret = 0;

	tr_dbg(&hdma_tr, "hda-dmac: %d channel %d -> get_data_size",
	       channel->dma->plat_data.id, channel->index);

	irq_local_disable(flags);

	ret = hda_dma_link_check_xrun(channel);
	if (ret < 0)
		goto unlock;

	if (channel->direction == DMA_DIR_HMEM_TO_LMEM ||
	    channel->direction == DMA_DIR_DEV_TO_MEM)
		*avail = hda_dma_avail_data_size(channel);
	else
		*free = hda_dma_free_data_size(channel);

unlock:
	irq_local_enable(flags);

	return ret;
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
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = HDA_DMA_BUFFER_ADDRESS_ALIGNMENT;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = HDA_DMA_BUFFER_PERIOD_COUNT;
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
	.pm_context_restore	= hda_dma_pm_context_restore,
	.pm_context_store	= hda_dma_pm_context_store,
	.probe			= hda_dma_probe,
	.remove			= hda_dma_remove,
	.get_data_size		= hda_dma_data_size,
	.get_attribute		= hda_dma_get_attribute,
	.interrupt		= hda_dma_interrupt,
};
