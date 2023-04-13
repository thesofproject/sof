// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/host_copier.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pcm_converter.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <ipc4/copier.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/dma.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_host;

LOG_MODULE_REGISTER(host_comp, CONFIG_SOF_LOG_LEVEL);

/* 8b9d100c-6d78-418f-90a3-e0e805d0852b */
DECLARE_SOF_RT_UUID("host", host_uuid, 0x8b9d100c, 0x6d78, 0x418f,
		    0x90, 0xa3, 0xe0, 0xe8, 0x05, 0xd0, 0x85, 0x2b);

DECLARE_TR_CTX(host_tr, SOF_UUID(host_uuid), LOG_LEVEL_INFO);

static inline struct dma_sg_elem *next_buffer(struct hc_buf *hc)
{
	if (!hc->elem_array.elems || !hc->elem_array.count)
		return NULL;
	if (++hc->current == hc->elem_array.count)
		hc->current = 0;
	return hc->elem_array.elems + hc->current;
}

static uint32_t host_dma_get_split(struct host_data *hd, uint32_t bytes)
{
	struct dma_sg_elem *local_elem = hd->config.elem_array.elems;
	uint32_t split_src = 0;
	uint32_t split_dst = 0;

	if (local_elem->src + bytes > hd->source->current_end)
		split_src = bytes -
			(hd->source->current_end - local_elem->src);

	if (local_elem->dest + bytes > hd->sink->current_end)
		split_dst = bytes -
			(hd->sink->current_end - local_elem->dest);

	/* get max split, so the current copy will be minimum */
	return MAX(split_src, split_dst);
}

#if CONFIG_FORCE_DMA_COPY_WHOLE_BLOCK

static int host_dma_set_config_and_copy(struct host_data *hd, struct comp_dev *dev, uint32_t bytes)
{
	struct dma_sg_elem *local_elem = hd->config.elem_array.elems;
	int ret;

	local_elem->size = bytes;

	/* reconfigure transfer */
	ret = dma_config(hd->chan->dma->z_dev, hd->chan->index, &hd->z_config);
	if (ret < 0) {
		comp_err(dev, "host_dma_set_config_and_copy(): dma_config() failed, ret = %d",
			 ret);
		return ret;
	}

	struct dma_cb_data next = {
		.channel = hd->chan,
		.elem = { .size = bytes },
	};
	notifier_event(hd->chan, NOTIFIER_ID_DMA_COPY,
		       NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));
	ret = dma_reload(hd->chan->dma->z_dev, hd->chan->index, 0, 0, bytes);
	if (ret < 0) {
		comp_err(dev, "host_dma_set_config_and_copy(): dma_copy() failed, ret = %d",
			 ret);
		return ret;
	}

	return ret;
}

/**
 * Calculates bytes to be copied in one shot mode.
 * @param dev Host component device.
 * @return Bytes to be copied.
 */
static uint32_t host_get_copy_bytes_one_shot(struct host_data *hd)
{
	struct comp_buffer *buffer = hd->local_buffer;
	struct comp_buffer __sparse_cache *buffer_c;
	uint32_t copy_bytes;

	buffer_c = buffer_acquire(buffer);

	/* calculate minimum size to copy */
	if (hd->ipc_host.direction == SOF_IPC_STREAM_PLAYBACK)
		copy_bytes = audio_stream_get_free_bytes(&buffer_c->stream);
	else
		copy_bytes = audio_stream_get_avail_bytes(&buffer_c->stream);

	buffer_release(buffer_c);

	/* copy_bytes should be aligned to minimum possible chunk of
	 * data to be copied by dma.
	 */
	return ALIGN_DOWN(copy_bytes, hd->dma_copy_align);
}

/**
 * Performs copy operation for host component working in one shot mode.
 * It means DMA needs to be reconfigured after every transfer.
 * @param dev Host component device.
 * @return 0 if succeeded, error code otherwise.
 */
static int host_copy_one_shot(struct host_data *hd, struct comp_dev *dev)
{
	uint32_t copy_bytes;
	uint32_t split_value;
	int ret = 0;

	comp_dbg(dev, "host_copy_one_shot()");

	copy_bytes = host_get_copy_bytes_one_shot(hd);
	if (!copy_bytes) {
		comp_info(dev, "host_copy_one_shot(): no bytes to copy");
		return ret;
	}

	while (copy_bytes) {
		/* get split value */
		split_value = host_dma_get_split(hd, copy_bytes);
		copy_bytes -= split_value;

		ret = host_dma_set_config_and_copy(hd, dev, copy_bytes);
		if (ret < 0)
			return ret;

		/* update copy bytes */
		copy_bytes = split_value;
	}

	return ret;
}

#else /* CONFIG_FORCE_DMA_COPY_WHOLE_BLOCK */

/**
 * Calculates bytes to be copied in one shot mode.
 * @param dev Host component device.
 * @return Bytes to be copied.
 */
static uint32_t host_get_copy_bytes_one_shot(struct host_data *hd)
{
	struct dma_sg_elem *local_elem = hd->config.elem_array.elems;
	struct comp_buffer *buffer = hd->local_buffer;
	struct comp_buffer __sparse_cache *buffer_c;
	uint32_t copy_bytes;
	uint32_t split_value;

	buffer_c = buffer_acquire(buffer);

	/* calculate minimum size to copy */
	if (hd->ipc_host.direction == SOF_IPC_STREAM_PLAYBACK)
		copy_bytes = audio_stream_get_free_bytes(&buffer_c->stream);
	else
		copy_bytes = audio_stream_get_avail_bytes(&buffer_c->stream);

	buffer_release(buffer_c);

	/* copy_bytes should be aligned to minimum possible chunk of
	 * data to be copied by dma.
	 */
	copy_bytes = ALIGN_DOWN(copy_bytes, hd->dma_copy_align);

	split_value = host_dma_get_split(hd, copy_bytes);
	if (split_value)
		copy_bytes -= split_value;

	local_elem->size = copy_bytes;

	return copy_bytes;
}

/**
 * Performs copy operation for host component working in one shot mode.
 * It means DMA needs to be reconfigured after every transfer.
 * @param dev Host component device.
 * @return 0 if succeeded, error code otherwise.
 */
static int host_copy_one_shot(struct host_data *hd, struct comp_dev *dev)
{
	uint32_t copy_bytes;
	int ret = 0;

	comp_dbg(dev, "host_copy_one_shot()");

	copy_bytes = host_get_copy_bytes_one_shot(hd);
	if (!copy_bytes) {
		comp_info(dev, "host_copy_one_shot(): no bytes to copy");
		return ret;
	}

	/* reconfigure transfer */
	ret = dma_config(hd->chan->dma->z_dev, hd->chan->index, &hd->z_config);
	if (ret < 0) {
		comp_err(dev, "host_copy_one_shot(): dma_config() failed, ret = %u", ret);
		return ret;
	}

	struct dma_cb_data next = {
		.channel = hd->chan,
		.elem = { .size = copy_bytes },
	};
	notifier_event(hd->chan, NOTIFIER_ID_DMA_COPY,
		       NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));
	ret = dma_reload(hd->chan->dma->z_dev, hd->chan->index, 0, 0, copy_bytes);
	if (ret < 0)
		comp_err(dev, "host_copy_one_shot(): dma_copy() failed, ret = %u", ret);

	return ret;
}
#endif

void host_update_position(struct host_data *hd, struct comp_dev *dev, uint32_t bytes)
{
	struct comp_buffer __sparse_cache *source;
	struct comp_buffer __sparse_cache *sink;
	int ret;
	bool update_mailbox = false;
	bool send_ipc = false;

	if (hd->ipc_host.direction == SOF_IPC_STREAM_PLAYBACK) {
		source = buffer_acquire(hd->dma_buffer);
		sink = buffer_acquire(hd->local_buffer);
		ret = dma_buffer_copy_from(source, sink, hd->process, bytes);
	} else {
		source = buffer_acquire(hd->local_buffer);
		sink = buffer_acquire(hd->dma_buffer);
		ret = dma_buffer_copy_to(source, sink, hd->process, bytes);
	}

	/* assert dma_buffer_copy succeed */
	if (ret < 0)
		comp_err(dev, "host_update_position() dma buffer copy failed, dir %d bytes %d avail %d free %d",
			 hd->ipc_host.direction, bytes,
			 audio_stream_get_avail_samples(&source->stream) *
				audio_stream_frame_bytes(&source->stream),
			 audio_stream_get_free_samples(&sink->stream) *
				audio_stream_frame_bytes(&sink->stream));

	buffer_release(sink);
	buffer_release(source);

	if (ret < 0)
		return;

	hd->total_data_processed += bytes;

	/* new local period, update host buffer position blks
	 * local_pos is queried by the ops.position() API
	 */
	hd->local_pos += bytes;

	/* buffer overlap, hardcode host buffer size at the moment */
	if (hd->local_pos >= hd->host_size)
#if CONFIG_WRAP_ACTUAL_POSITION
		hd->local_pos %= hd->host_size;
#else
		hd->local_pos = 0;
#endif
	if (hd->cont_update_posn)
		update_mailbox = true;

	/* Don't send stream position if no_stream_position == 1 */
	if (!hd->no_stream_position) {
		hd->report_pos += bytes;

		/* host_period_bytes is set to zero to disable position update
		 * by IPC for FW version before 3.11, so send IPC message to
		 * driver according to this condition and report_pos.
		 */
		if (hd->host_period_bytes != 0 &&
		    hd->report_pos >= hd->host_period_bytes) {
			hd->report_pos = 0;

			/* send timestamped position to host
			 * (updates position first, by calling ops.position())
			 */
			update_mailbox = true;
			send_ipc = true;
		}
	}

	if (update_mailbox) {
		pipeline_get_timestamp(dev->pipeline, dev, &hd->posn);
		mailbox_stream_write(dev->pipeline->posn_offset,
				     &hd->posn, sizeof(hd->posn));
		if (send_ipc)
			ipc_msg_send(hd->msg, &hd->posn, false);
	}
}

/* The host memory is not guaranteed to be continuous and also not guaranteed
 * to have a period/buffer size that is a multiple of the DSP period size.
 * This means we must check we do not overflow host period/buffer/page
 * boundaries on each transfer and split the DMA transfer if we do overflow.
 */
void host_one_shot_cb(struct host_data *hd, uint32_t bytes)
{
	struct dma_sg_elem *local_elem = hd->config.elem_array.elems;
	struct dma_sg_elem *source_elem;
	struct dma_sg_elem *sink_elem;

	/* update src and dest positions and check for overflow */
	local_elem->src += bytes;
	local_elem->dest += bytes;

	if (local_elem->src == hd->source->current_end) {
		/* end of element, so use next */
		source_elem = next_buffer(hd->source);
		if (source_elem) {
			hd->source->current_end = source_elem->src +
				source_elem->size;
			local_elem->src = source_elem->src;
		}
	}

	if (local_elem->dest == hd->sink->current_end) {
		/* end of element, so use next */
		sink_elem = next_buffer(hd->sink);
		if (sink_elem) {
			hd->sink->current_end = sink_elem->dest +
				sink_elem->size;
			local_elem->dest = sink_elem->dest;
		}
	}
}

/* This is called by DMA driver every time when DMA completes its current
 * transfer between host and DSP.
 */
static void host_dma_cb(void *arg, enum notify_id type, void *data)
{
	struct dma_cb_data *next = data;
	struct comp_dev *dev = arg;
	struct host_data *hd = comp_get_drvdata(dev);
	uint32_t bytes = next->elem.size;

	comp_cl_dbg(&comp_host, "host_dma_cb() %p", &comp_host);

	/* update position */
	host_update_position(hd, dev, bytes);

	/* callback for one shot copy */
	if (hd->copy_type == COMP_COPY_ONE_SHOT)
		host_one_shot_cb(hd, bytes);
}

/**
 * Calculates bytes to be copied in normal mode.
 * @param dev Host component device.
 * @return Bytes to be copied.
 */
static uint32_t host_get_copy_bytes_normal(struct host_data *hd, struct comp_dev *dev)
{
	struct comp_buffer *buffer = hd->local_buffer;
	struct comp_buffer __sparse_cache *buffer_c;
	struct comp_buffer __sparse_cache *dma_buf_c;
	struct dma_status dma_stat;
	uint32_t avail_samples;
	uint32_t free_samples;
	uint32_t dma_sample_bytes;
	uint32_t dma_copy_bytes;
	int ret;

	/* get data sizes from DMA */
	ret = dma_get_status(hd->chan->dma->z_dev, hd->chan->index, &dma_stat);
	if (ret < 0) {
		comp_err(dev, "host_get_copy_bytes_normal(): dma_get_status() failed, ret = %u",
			 ret);
		/* return 0 copy_bytes in case of error to skip DMA copy */
		return 0;
	}

	dma_buf_c = buffer_acquire(hd->dma_buffer);
	dma_sample_bytes = get_sample_bytes(dma_buf_c->stream.frame_fmt);
	buffer_release(dma_buf_c);

	buffer_c = buffer_acquire(buffer);

	/* calculate minimum size to copy */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		avail_samples = dma_stat.pending_length / dma_sample_bytes;
		free_samples = audio_stream_get_free_samples(&buffer_c->stream);
	} else {
		avail_samples = audio_stream_get_avail_samples(&buffer_c->stream);
		free_samples = dma_stat.free / dma_sample_bytes;
	}

	dma_copy_bytes = MIN(avail_samples, free_samples) * dma_sample_bytes;

	/* limit bytes per copy to one period for the whole pipeline
	 * in order to avoid high load spike
	 * if FAST_MODE is enabled, then one period limitation is omitted
	 */
	if (!(hd->ipc_host.feature_mask & BIT(IPC4_COPIER_FAST_MODE)))
		dma_copy_bytes = MIN(hd->period_bytes, dma_copy_bytes);

	if (!dma_copy_bytes)
		comp_info(dev, "no bytes to copy, available samples: %d, free_samples: %d",
			  avail_samples, free_samples);

	buffer_release(buffer_c);

	/* dma_copy_bytes should be aligned to minimum possible chunk of
	 * data to be copied by dma.
	 */
	return ALIGN_DOWN(dma_copy_bytes, hd->dma_copy_align);
}

/**
 * Performs copy operation for host component working in normal mode.
 * It means DMA works continuously and doesn't need reconfiguration.
 * @param dev Host component device.
 * @return 0 if succeeded, error code otherwise.
 */
static int host_copy_normal(struct host_data *hd, struct comp_dev *dev)
{
	struct comp_buffer __sparse_cache *buffer_c;
	uint32_t copy_bytes;
	const unsigned int threshold =
#if CONFIG_HOST_DMA_RELOAD_DELAY_ENABLE
		CONFIG_HOST_DMA_RELOAD_THRESHOLD +
#endif
		0;
	int ret = 0;

	comp_dbg(dev, "host_copy_normal()");

	copy_bytes = host_get_copy_bytes_normal(hd, dev);
	if (!copy_bytes)
		return 0;

	/* Register Host DMA usage */
	pm_runtime_get(PM_RUNTIME_HOST_DMA_L1, 0);

	struct dma_cb_data next = {
		.channel = hd->chan,
		.elem = { .size = copy_bytes },
	};
	notifier_event(hd->chan, NOTIFIER_ID_DMA_COPY,
		       NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));

	hd->partial_size += copy_bytes;
	buffer_c = buffer_acquire(hd->dma_buffer);

	/*
	 * On large buffers we don't need to reload DMA on every period. When
	 * CONFIG_HOST_DMA_RELOAD_DELAY_ENABLE is selected on buffers, larger
	 * than 8 periods, only do that when the threshold is reached, while
	 * also adding a 2ms safety margin.
	 */
	if (!IS_ENABLED(CONFIG_HOST_DMA_RELOAD_DELAY_ENABLE) ||
	    buffer_c->stream.size < hd->period_bytes << 3 ||
	    buffer_c->stream.size - hd->partial_size <= (2 + threshold) * hd->period_bytes) {
		ret = dma_reload(hd->chan->dma->z_dev, hd->chan->index, 0, 0, hd->partial_size);
		if (ret < 0)
			comp_err(dev, "host_copy_normal(): dma_copy() failed, ret = %u", ret);

		hd->partial_size = 0;
	}

	buffer_release(buffer_c);

	return ret;
}

static int create_local_elems(struct host_data *hd, struct comp_dev *dev, uint32_t buffer_count,
			      uint32_t buffer_bytes, uint32_t direction)
{
	struct comp_buffer __sparse_cache *dma_buf_c;
	struct dma_sg_elem_array *elem_array;
	uint32_t dir;
	int err;

	dir = direction == SOF_IPC_STREAM_PLAYBACK ?
		DMA_DIR_HMEM_TO_LMEM : DMA_DIR_LMEM_TO_HMEM;

	/* if host buffer set we need to allocate local buffer */
	if (hd->host.elem_array.count) {
		elem_array = &hd->local.elem_array;

		/* config buffer will be used as proxy */
		err = dma_sg_alloc(&hd->config.elem_array, SOF_MEM_ZONE_RUNTIME,
				   dir, 1, 0, 0, 0);
		if (err < 0) {
			comp_err(dev, "create_local_elems(): dma_sg_alloc() failed");
			return err;
		}
	} else {
		elem_array = &hd->config.elem_array;
	}

	dma_buf_c = buffer_acquire(hd->dma_buffer);
	err = dma_sg_alloc(elem_array, SOF_MEM_ZONE_RUNTIME, dir, buffer_count,
			   buffer_bytes,
			   (uintptr_t)(dma_buf_c->stream.addr), 0);
	buffer_release(dma_buf_c);
	if (err < 0) {
		comp_err(dev, "create_local_elems(): dma_sg_alloc() failed");
		return err;
	}

	return 0;
}

static void hda_dma_l1_exit_notify(void *arg, enum notify_id type, void *data)
{
	/* Force Host DMA to exit L1 if needed */
	pm_runtime_put(PM_RUNTIME_HOST_DMA_L1, 0);
}

/**
 * \brief Command handler.
 * \param[in,out] dev Device
 * \param[in] cmd Command
 * \return 0 if successful, error code otherwise.
 *
 * Used to pass standard and bespoke commands (with data) to component.
 * This function is common for all dma types, with one exception:
 * dw-dma is run on demand, so no start()/stop() is issued.
 */
int host_zephyr_trigger(struct host_data *hd, struct comp_dev *dev, int cmd)
{
	int ret = 0;

	/* we should ignore any trigger commands besides start
	 * when doing one shot, because transfers will stop automatically
	 */
	if (cmd != COMP_TRIGGER_START && hd->copy_type == COMP_COPY_ONE_SHOT)
		return ret;

	if (!hd->chan) {
		comp_err(dev, "host_trigger(): no dma channel configured");
		return -EINVAL;
	}

	switch (cmd) {
	case COMP_TRIGGER_START:
		hd->partial_size = 0;
		ret = dma_start(hd->chan->dma->z_dev, hd->chan->index);
		if (ret < 0)
			comp_err(dev, "host_trigger(): dma_start() failed, ret = %u",
				 ret);
		/* Register common L1 exit for all channels */
		ret = notifier_register(NULL, scheduler_get_data(SOF_SCHEDULE_LL_TIMER),
								NOTIFIER_ID_LL_POST_RUN, hda_dma_l1_exit_notify,
								NOTIFIER_FLAG_AGGREGATE);
		break;
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_XRUN:
		if (dev->state == COMP_STATE_ACTIVE) {
			ret = dma_stop(hd->chan->dma->z_dev, hd->chan->index);
			if (ret < 0)
				comp_err(dev, "host_trigger(): dma stop failed: %d",
					 ret);
			/* Unregister L1 exit */
			notifier_unregister(NULL, scheduler_get_data(SOF_SCHEDULE_LL_TIMER),
								NOTIFIER_ID_LL_POST_RUN);
		}

		break;
	default:
		break;
	}

	return ret;
}

static int host_trigger(struct comp_dev *dev, int cmd)
{
	struct host_data *hd = comp_get_drvdata(dev);
	int ret;

	comp_dbg(dev, "host_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return host_zephyr_trigger(hd, dev, cmd);
}

int host_zephyr_new(struct host_data *hd, struct comp_dev *dev,
		    const struct ipc_config_host *ipc_host, uint32_t config_id)
{
	uint32_t dir;

	hd->ipc_host = *ipc_host;
	/* request HDA DMA with shared access privilege */
	dir = hd->ipc_host.direction == SOF_IPC_STREAM_PLAYBACK ?
		DMA_DIR_HMEM_TO_LMEM : DMA_DIR_LMEM_TO_HMEM;

	hd->dma = dma_get(dir, 0, DMA_DEV_HOST, DMA_ACCESS_SHARED);
	if (!hd->dma) {
		comp_err(dev, "host_new(): dma_get() returned NULL");
		return -ENODEV;
	}

	/* init buffer elems */
	dma_sg_init(&hd->config.elem_array);
	dma_sg_init(&hd->host.elem_array);
	dma_sg_init(&hd->local.elem_array);

	ipc_build_stream_posn(&hd->posn, SOF_IPC_STREAM_POSITION, config_id);

	hd->msg = ipc_msg_init(hd->posn.rhdr.hdr.cmd, sizeof(hd->posn));
	if (!hd->msg) {
		comp_err(dev, "host_new(): ipc_msg_init failed");
		dma_put(hd->dma);
		return -ENOMEM;
	}
	hd->chan = NULL;
	hd->copy_type = COMP_COPY_NORMAL;

	return 0;
}

static struct comp_dev *host_new(const struct comp_driver *drv,
				 const struct comp_ipc_config *config,
				 const void *spec)
{
	struct comp_dev *dev;
	struct host_data *hd;
	const struct ipc_config_host *ipc_host = spec;
	int ret;

	comp_cl_dbg(&comp_host, "host_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	hd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*hd));
	if (!hd)
		goto e_data;

	comp_set_drvdata(dev, hd);

	ret = host_zephyr_new(hd, dev, ipc_host, dev->ipc_config.id);
	if (ret)
		goto e_dev;

	dev->state = COMP_STATE_READY;

	return dev;

e_dev:
	rfree(hd);
e_data:
	rfree(dev);
	return NULL;
}

void host_zephyr_free(struct host_data *hd)
{
	dma_put(hd->dma);

	ipc_msg_free(hd->msg);
	dma_sg_free(&hd->config.elem_array);
}

static void host_free(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	comp_dbg(dev, "host_free()");
	host_zephyr_free(hd);
	rfree(hd);
	rfree(dev);
}

static int host_elements_reset(struct host_data *hd, int direction)
{
	struct dma_sg_elem *source_elem;
	struct dma_sg_elem *sink_elem;
	struct dma_sg_elem *local_elem;

	/* setup elem to point to first source elem */
	source_elem = hd->source->elem_array.elems;
	if (source_elem) {
		hd->source->current = 0;
		hd->source->current_end = source_elem->src + source_elem->size;
	}

	/* setup elem to point to first sink elem */
	sink_elem = hd->sink->elem_array.elems;
	if (sink_elem) {
		hd->sink->current = 0;
		hd->sink->current_end = sink_elem->dest + sink_elem->size;
	}

	/* local element */
	if (source_elem && sink_elem) {
		local_elem = hd->config.elem_array.elems;
		local_elem->dest = sink_elem->dest;
		local_elem->size =
			direction == SOF_IPC_STREAM_PLAYBACK ?
			sink_elem->size : source_elem->size;
		local_elem->src = source_elem->src;
	}

	return 0;
}

static int host_verify_params(struct comp_dev *dev,
			      struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "host_verify_params()");

	ret = comp_verify_params(dev, 0, params);
	if (ret < 0) {
		comp_err(dev, "host_verify_params(): comp_verify_params() failed");
		return ret;
	}

	return 0;
}

/* configure the DMA params and descriptors for host buffer IO */
int host_zephyr_params(struct host_data *hd, struct comp_dev *dev,
		       struct sof_ipc_stream_params *params)
{
	struct dma_sg_config *config = &hd->config;
	struct dma_sg_elem *sg_elem;
	struct dma_config *dma_cfg = &hd->z_config;
	struct dma_block_config dma_block_cfg;
	struct comp_buffer __sparse_cache *host_buf_c;
	struct comp_buffer __sparse_cache *dma_buf_c;
	uint32_t period_count;
	uint32_t period_bytes;
	uint32_t buffer_size;
	uint32_t addr_align;
	uint32_t align;
	int i, channel, err;
	bool is_scheduling_source = dev == dev->pipeline->sched_comp;

	/* host params always installed by pipeline IPC */
	hd->host_size = params->buffer.size;
	hd->stream_tag = params->stream_tag;
	hd->no_stream_position = params->no_stream_position;
	hd->host_period_bytes = params->host_period_bytes;
	hd->cont_update_posn = params->cont_update_posn;

	/* retrieve DMA buffer address alignment */
	err = dma_get_attribute(hd->dma->z_dev, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (err < 0) {
		comp_err(dev, "host_params(): could not get dma buffer address alignment, err = %d",
			 err);
		return err;
	}

	/* retrieve DMA buffer size alignment */
	err = dma_get_attribute(hd->dma->z_dev, DMA_ATTR_BUFFER_SIZE_ALIGNMENT, &align);
	if (err < 0 || !align) {
		comp_err(dev, "host_params(): could not get valid dma buffer alignment, err = %d, align = %u",
			 err, align);
		return -EINVAL;
	}

	/* retrieve DMA buffer period count */
	period_count = hd->dma->plat_data.period_count;
	if (!period_count) {
		comp_err(dev, "host_params(): could not get valid dma buffer period count");
		return -EINVAL;
	}

	if (params->direction == SOF_IPC_STREAM_PLAYBACK)
		hd->local_buffer = list_first_item(&dev->bsink_list,
						   struct comp_buffer,
						   source_list);
	else
		hd->local_buffer = list_first_item(&dev->bsource_list,
						   struct comp_buffer,
						   sink_list);
	host_buf_c = buffer_acquire(hd->local_buffer);

	period_bytes = dev->frames * get_frame_bytes(params->frame_fmt, params->channels);

	if (!period_bytes) {
		comp_err(dev, "host_params(): invalid period_bytes");
		err = -EINVAL;
		goto out;
	}

	/* determine source and sink buffer elements */
	if (params->direction == SOF_IPC_STREAM_PLAYBACK) {
		config->direction = DMA_DIR_HMEM_TO_LMEM;
		hd->source = &hd->host;
		hd->sink = &hd->local;
	} else {
		config->direction = DMA_DIR_LMEM_TO_HMEM;
		hd->source = &hd->local;
		hd->sink = &hd->host;
	}

	/* TODO: should be taken from DMA */
	if (hd->host.elem_array.count) {
		period_bytes *= period_count;
		period_count = 1;
	}

	/* calculate DMA buffer size */
	buffer_size = ALIGN_UP(period_bytes, align) * period_count;
	buffer_size = MAX(buffer_size, ALIGN_UP(hd->ipc_host.dma_buffer_size, align));

	/* alloc DMA buffer or change its size if exists */
	/*
	 * Host DMA buffer cannot be shared. So we actually don't need to lock,
	 * but we have to write back caches after we finish anywae
	 */
	if (hd->dma_buffer) {
		dma_buf_c = buffer_acquire(hd->dma_buffer);
		err = buffer_set_size(dma_buf_c, buffer_size);
		buffer_release(dma_buf_c);
		if (err < 0) {
			comp_err(dev, "host_params(): buffer_set_size() failed, buffer_size = %u",
				 buffer_size);
			goto out;
		}
	} else {
		hd->dma_buffer = buffer_alloc(buffer_size, SOF_MEM_CAPS_DMA,
					      addr_align);
		if (!hd->dma_buffer) {
			comp_err(dev, "host_params(): failed to alloc dma buffer");
			err = -ENOMEM;
			goto out;
		}

		dma_buf_c = buffer_acquire(hd->dma_buffer);
		buffer_set_params(dma_buf_c, params, BUFFER_UPDATE_FORCE);

		/* set processing function */
		if (params->direction == SOF_IPC_STREAM_CAPTURE)
			hd->process = pcm_get_conversion_function(host_buf_c->stream.frame_fmt,
								  dma_buf_c->stream.frame_fmt);
		else
			hd->process = pcm_get_conversion_function(dma_buf_c->stream.frame_fmt,
								  host_buf_c->stream.frame_fmt);

		config->src_width = audio_stream_sample_bytes(&dma_buf_c->stream);
		config->dest_width = config->src_width;
		buffer_release(dma_buf_c);
	}

	/* create SG DMA elems for local DMA buffer */
	err = create_local_elems(hd, dev, period_count, buffer_size / period_count,
				 params->direction);
	if (err < 0)
		goto out;

	/* set up DMA configuration - copy in sample bytes. */
	config->cyclic = 0;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->is_scheduling_source = is_scheduling_source;
	config->period = dev->pipeline->period;

	host_elements_reset(hd, params->direction);

	hd->stream_tag -= 1;
	uint32_t hda_chan = hd->stream_tag;
	/* get DMA channel from DMAC
	 * note: stream_tag is ignored by dw-dma
	 */
	channel = dma_request_channel(hd->dma->z_dev, &hda_chan);
	if (channel < 0) {
		comp_err(dev, "host_params(): requested channel %d is busy", hda_chan);
		err = -ENODEV;
		goto out;
	}
	hd->chan = &hd->dma->chan[channel];

	uint32_t buffer_addr = 0;
	uint32_t buffer_bytes = 0;
	uint32_t addr;

	hd->chan->direction = config->direction;
	hd->chan->desc_count = config->elem_array.count;
	hd->chan->is_scheduling_source = config->is_scheduling_source;
	hd->chan->period = config->period;

	memset(dma_cfg, 0, sizeof(*dma_cfg));

	dma_cfg->block_count = 1;
	dma_cfg->source_data_size = config->src_width;
	dma_cfg->dest_data_size = config->dest_width;
	dma_cfg->head_block  = &dma_block_cfg;

	for (i = 0; i < config->elem_array.count; i++) {
		sg_elem = config->elem_array.elems + i;

		if (config->direction == DMA_DIR_HMEM_TO_LMEM ||
		    config->direction == DMA_DIR_DEV_TO_MEM)
			addr = sg_elem->dest;
		else
			addr = sg_elem->src;

		buffer_bytes += sg_elem->size;

		if (buffer_addr == 0)
			buffer_addr = addr;
	}

	dma_block_cfg.block_size = buffer_bytes;

	switch (config->direction) {
	case DMA_DIR_LMEM_TO_HMEM:
		dma_cfg->channel_direction = MEMORY_TO_HOST;
		dma_block_cfg.source_address = buffer_addr;
		break;
	case DMA_DIR_HMEM_TO_LMEM:
		dma_cfg->channel_direction = HOST_TO_MEMORY;
		dma_block_cfg.dest_address = buffer_addr;
		break;
	}

	err = dma_config(hd->chan->dma->z_dev, hd->chan->index, dma_cfg);
	if (err < 0) {
		comp_err(dev, "host_params(): dma_config() failed");
		dma_release_channel(hd->dma->z_dev, hd->chan->index);
		hd->chan = NULL;
		goto out;
	}

	err = dma_get_attribute(hd->dma->z_dev, DMA_ATTR_COPY_ALIGNMENT,
				&hd->dma_copy_align);

	if (err < 0) {
		comp_err(dev, "host_params(): dma_get_attribute()");

		goto out;
	}

	/* minimal copied data shouldn't be less than alignment */
	hd->period_bytes = ALIGN_UP(period_bytes, hd->dma_copy_align);

	/* set copy function */
	hd->copy = hd->copy_type == COMP_COPY_ONE_SHOT ? host_copy_one_shot :
		host_copy_normal;

out:
	buffer_release(host_buf_c);
	return err;
}

static int host_params(struct comp_dev *dev,
		       struct sof_ipc_stream_params *params)
{
	struct host_data *hd = comp_get_drvdata(dev);
	int err;

	comp_dbg(dev, "host_params()");

	err = host_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "host_params(): pcm params verification failed.");
		return err;
	}

	err = host_zephyr_params(hd, dev, params);
	if (err >= 0)
		/* set up callback */
		notifier_register(dev, hd->chan, NOTIFIER_ID_DMA_COPY, host_dma_cb, 0);

	return err;
}

int host_zephyr_prepare(struct host_data *hd)
{
	struct comp_buffer __sparse_cache *buf_c = buffer_acquire(hd->dma_buffer);

	buffer_zero(buf_c);
	buffer_release(buf_c);

	return 0;
}

static int host_prepare(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	int ret;

	comp_dbg(dev, "host_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return host_zephyr_prepare(hd);
}

static int host_position(struct comp_dev *dev,
			 struct sof_ipc_stream_posn *posn)
{
	struct host_data *hd = comp_get_drvdata(dev);

	/* TODO: improve accuracy by adding current DMA position */
	posn->host_posn = hd->local_pos;

	return 0;
}

void host_zephyr_reset(struct host_data *hd, uint16_t state)
{
	if (hd->chan) {
		dma_stop(hd->chan->dma->z_dev, hd->chan->index);
		/* Unregister L1 exit */
		notifier_unregister(NULL, scheduler_get_data(SOF_SCHEDULE_LL_TIMER),
							NOTIFIER_ID_LL_POST_RUN);

		dma_release_channel(hd->dma->z_dev, hd->chan->index);
		hd->chan = NULL;
	}

	/* free all DMA elements */
	dma_sg_free(&hd->host.elem_array);
	dma_sg_free(&hd->local.elem_array);
	dma_sg_free(&hd->config.elem_array);

	/* free DMA buffer */
	if (hd->dma_buffer) {
		buffer_free(hd->dma_buffer);
		hd->dma_buffer = NULL;
	}

	/* reset buffer pointers */
	hd->local_pos = 0;
	hd->report_pos = 0;
	hd->total_data_processed = 0;

	hd->copy_type = COMP_COPY_NORMAL;
	hd->source = NULL;
	hd->sink = NULL;
}

static int host_reset(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	comp_dbg(dev, "host_reset()");
	/* remove callback first for host reset */
	if (hd->chan)
		notifier_unregister(dev, hd->chan, NOTIFIER_ID_DMA_COPY);

	host_zephyr_reset(hd, dev->state);
	dev->state = COMP_STATE_READY;

	return 0;
}

/* copy and process stream data from source to sink buffers */
int host_zephyr_copy(struct host_data *hd, struct comp_dev *dev)
{
	return hd->copy(hd, dev);
}

static int host_copy(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	if (dev->state != COMP_STATE_ACTIVE)
		return 0;

	return host_zephyr_copy(hd, dev);
}

static int host_get_attribute(struct comp_dev *dev, uint32_t type,
			      void *value)
{
	struct host_data *hd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_COPY_TYPE:
		*(enum comp_copy_type *)value = hd->copy_type;
		break;
	case COMP_ATTR_COPY_DIR:
		*(uint32_t *)value = hd->ipc_host.direction;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int host_set_attribute(struct comp_dev *dev, uint32_t type,
			      void *value)
{
	struct host_data *hd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_COPY_TYPE:
		hd->copy_type = *(enum comp_copy_type *)value;
		break;
	case COMP_ATTR_HOST_BUFFER:
		hd->host.elem_array = *(struct dma_sg_elem_array *)value;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static uint64_t host_get_processed_data(struct comp_dev *dev, uint32_t stream_no, bool input)
{
	struct host_data *hd = comp_get_drvdata(dev);
	uint64_t ret = 0;
	bool source = dev->direction == SOF_IPC_STREAM_PLAYBACK;

	/* Return value only if direction and stream number match.
	 * The host supports only one stream.
	 */
	if (stream_no == 0 && source == input)
		ret = hd->total_data_processed;

	return ret;
}

static const struct comp_driver comp_host = {
	.type	= SOF_COMP_HOST,
	.uid	= SOF_RT_UUID(host_uuid),
	.tctx	= &host_tr,
	.ops	= {
		.create				= host_new,
		.free				= host_free,
		.params				= host_params,
		.reset				= host_reset,
		.trigger			= host_trigger,
		.copy				= host_copy,
		.prepare			= host_prepare,
		.position			= host_position,
		.get_attribute			= host_get_attribute,
		.set_attribute			= host_set_attribute,
		.get_total_data_processed	= host_get_processed_data,
	},
};

static SHARED_DATA struct comp_driver_info comp_host_info = {
	.drv = &comp_host,
};

UT_STATIC void sys_comp_host_init(void)
{
	comp_register(platform_shared_get(&comp_host_info,
					  sizeof(comp_host_info)));
}

DECLARE_MODULE(sys_comp_host_init);
SOF_MODULE_INIT(host, sys_comp_host_init);
