// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/dma.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define trace_host(__e, ...) \
	trace_event(TRACE_CLASS_HOST, __e, ##__VA_ARGS__)

#define trace_host_with_ids(comp_ptr, format, ...)	\
	trace_event_with_ids(TRACE_CLASS_HOST,		\
			     comp_ptr->comp.pipeline_id,\
			     comp_ptr->comp.id,		\
			     format, ##__VA_ARGS__)

#define tracev_host(__e, ...) \
	tracev_event(TRACE_CLASS_HOST, __e, ##__VA_ARGS__)

#define tracev_host_with_ids(comp_ptr, format, ...)	\
	tracev_event_with_ids(TRACE_CLASS_HOST,		\
			     comp_ptr->comp.pipeline_id,\
			     comp_ptr->comp.id,		\
			     format, ##__VA_ARGS__)

#define trace_host_error(__e, ...) \
	trace_error(TRACE_CLASS_HOST, __e, ##__VA_ARGS__)

#define trace_host_error_with_ids(comp_ptr, format, ...)	\
	trace_error_with_ids(TRACE_CLASS_HOST,		\
			     comp_ptr->comp.pipeline_id,\
			     comp_ptr->comp.id,		\
			     format, ##__VA_ARGS__)

/**
 * \brief Host buffer info.
 */
struct hc_buf {
	struct dma_sg_elem_array elem_array; /**< array of SG elements */
	uint32_t current;		/**< index of current element */
	uint32_t current_end;
};

/**
 * \brief Host component data.
 *
 * Host reports local position in the host buffer every params.host_period_bytes
 * if the latter is != 0. report_pos is used to track progress since the last
 * multiple of host_period_bytes.
 *
 * host_size is the host buffer size (in bytes) specified in the IPC parameters.
 */
struct host_data {
	/* local DMA config */
	struct dma *dma;
	struct dma_chan_data *chan;
	struct dma_sg_config config;
	struct comp_buffer *dma_buffer;

	/* host position reporting related */
	uint32_t host_size;	/**< Host buffer size (in bytes) */
	uint32_t report_pos;	/**< Position in current report period */
	uint32_t local_pos;	/**< Local position in host buffer */

	/* host component attributes */
	enum comp_copy_type copy_type;	/**< Current host copy type */

	/* local and host DMA buffer info */
	struct hc_buf host;
	struct hc_buf local;

	/* pointers set during params to host or local above */
	struct hc_buf *source;
	struct hc_buf *sink;

	uint32_t dma_copy_align; /**< Minimal chunk of data possible to be
				   *  copied by dma connected to host
				   */

	/* processing function */
	void (*process)(struct comp_buffer *source, struct comp_buffer *sink,
			uint32_t bytes);

	/* stream info */
	struct sof_ipc_stream_posn posn; /* TODO: update this */
};

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

static void host_update_position(struct comp_dev *dev, uint32_t bytes)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct comp_buffer *local_buffer;

	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		local_buffer = list_first_item(&dev->bsink_list,
					       struct comp_buffer, source_list);
		dma_buffer_copy_from(hd->dma_buffer, local_buffer, hd->process,
				     bytes);
	} else {
		local_buffer = list_first_item(&dev->bsource_list,
					       struct comp_buffer, sink_list);
		dma_buffer_copy_to(local_buffer, hd->dma_buffer, hd->process,
				   bytes);
	}

	dev->position += bytes;

	/* new local period, update host buffer position blks
	 * local_pos is queried by the ops.position() API
	 */
	hd->local_pos += bytes;

	/* buffer overlap, hardcode host buffer size at the moment */
	if (hd->local_pos >= hd->host_size)
		hd->local_pos = 0;

	/* Don't send stream position if no_stream_position == 1 */
	if (!dev->params.no_stream_position) {
		hd->report_pos += bytes;

		/* send IPC message to driver if needed */
		if (hd->report_pos >= dev->params.host_period_bytes) {
			hd->report_pos = 0;

			/* send timestamped position to host
			 * (updates position first, by calling ops.position())
			 */
			pipeline_get_timestamp(dev->pipeline, dev, &hd->posn);
			ipc_stream_send_position(dev, &hd->posn);
		}
	}
}

/* The host memory is not guaranteed to be continuous and also not guaranteed
 * to have a period/buffer size that is a multiple of the DSP period size.
 * This means we must check we do not overflow host period/buffer/page
 * boundaries on each transfer and split the DMA transfer if we do overflow.
 */
static void host_one_shot_cb(struct comp_dev *dev, uint32_t bytes)
{
	struct host_data *hd = comp_get_drvdata(dev);
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
static void host_dma_cb(void *data, uint32_t type, struct dma_cb_data *next)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct host_data *hd = comp_get_drvdata(dev);
	uint32_t bytes = next->elem.size;

	tracev_host("host_dma_cb()");

	/* update position */
	host_update_position(dev, bytes);

	/* callback for one shot copy */
	if (hd->copy_type == COMP_COPY_ONE_SHOT)
		host_one_shot_cb(dev, bytes);
}

static int create_local_elems(struct comp_dev *dev, uint32_t buffer_count,
			      uint32_t buffer_bytes)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem_array *elem_array;
	uint32_t dir;
	int err;

	dir = dev->params.direction == SOF_IPC_STREAM_PLAYBACK ?
		DMA_DIR_HMEM_TO_LMEM : DMA_DIR_LMEM_TO_HMEM;

	/* if host buffer set we need to allocate local buffer */
	if (hd->host.elem_array.count) {
		elem_array = &hd->local.elem_array;

		/* config buffer will be used as proxy */
		err = dma_sg_alloc(&hd->config.elem_array, RZONE_RUNTIME,
				   dir, 1, 0, 0, 0);
		if (err < 0) {
			trace_host_error_with_ids(dev, "create_local_elems() "
						  "error: dma_sg_alloc() "
						  "failed");
			return err;
		}
	} else {
		elem_array = &hd->config.elem_array;
	}

	err = dma_sg_alloc(elem_array, RZONE_RUNTIME, dir, buffer_count,
			   buffer_bytes, (uintptr_t)(hd->dma_buffer->addr), 0);
	if (err < 0) {
		trace_host_error_with_ids(dev, "create_local_elems() error: "
					  "dma_sg_alloc() failed");
		return err;
	}

	return 0;
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
static int host_trigger(struct comp_dev *dev, int cmd)
{
	struct host_data *hd = comp_get_drvdata(dev);
	int ret = 0;

	trace_host_with_ids(dev, "host_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET) {
		ret = PPL_STATUS_PATH_STOP;
		return ret;
	}

	/* we should ignore any trigger commands besides start
	 * when doing one shot, because transfers will stop automatically
	 */
	if (cmd != COMP_TRIGGER_START && hd->copy_type == COMP_COPY_ONE_SHOT)
		return ret;

	if (!hd->chan) {
		trace_host_error_with_ids(dev, "host_trigger() error: no dma "
					  "channel configured");
		return -EINVAL;
	}

	switch (cmd) {
	case COMP_TRIGGER_START:
		ret = dma_start(hd->chan);
		if (ret < 0)
			trace_host_error_with_ids(dev, "host_trigger() error: "
						  "dma_start() failed, "
						  "ret = %u", ret);
		break;
	case COMP_TRIGGER_STOP:
	case COMP_TRIGGER_XRUN:
		ret = dma_stop(hd->chan);
		if (ret < 0)
			trace_host_error_with_ids(dev, "host_trigger(): dma "
						  "stop failed: %d", ret);
		break;
	default:
		break;
	}

	return ret;
}

static struct comp_dev *host_new(struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_host *ipc_host = (struct sof_ipc_comp_host *)comp;
	struct sof_ipc_comp_host *host;
	struct comp_dev *dev;
	struct host_data *hd;
	uint32_t dir;

	trace_host("host_new()");

	if (IPC_IS_SIZE_INVALID(ipc_host->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_HOST, ipc_host->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_host));
	if (!dev)
		return NULL;

	host = (struct sof_ipc_comp_host *)&dev->comp;
	assert(!memcpy_s(host, sizeof(*host),
			 ipc_host, sizeof(struct sof_ipc_comp_host)));

	hd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*hd));
	if (!hd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, hd);

	/* request HDA DMA with shared access privilege */
	dir = ipc_host->direction == SOF_IPC_STREAM_PLAYBACK ?
		DMA_DIR_HMEM_TO_LMEM : DMA_DIR_LMEM_TO_HMEM;

	hd->dma = dma_get(dir, 0, DMA_DEV_HOST, DMA_ACCESS_SHARED);
	if (!hd->dma) {
		trace_host_error("host_new() error: dma_get() returned NULL");
		rfree(hd);
		rfree(dev);
		return NULL;
	}

	/* init buffer elems */
	dma_sg_init(&hd->config.elem_array);
	dma_sg_init(&hd->host.elem_array);
	dma_sg_init(&hd->local.elem_array);

	hd->chan = NULL;
	hd->copy_type = COMP_COPY_NORMAL;
	hd->posn.comp_id = comp->id;
	dev->state = COMP_STATE_READY;

	return dev;
}

static void host_free(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	trace_host_with_ids(dev, "host_free()");

	dma_put(hd->dma);

	dma_sg_free(&hd->config.elem_array);
	rfree(hd);
	rfree(dev);
}

static int host_elements_reset(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *source_elem = NULL;
	struct dma_sg_elem *sink_elem = NULL;
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
			dev->params.direction == SOF_IPC_STREAM_PLAYBACK ?
			sink_elem->size : source_elem->size;
		local_elem->src = source_elem->src;
	}

	return 0;
}

/* configure the DMA params and descriptors for host buffer IO */
static int host_params(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &hd->config;
	uint32_t period_count;
	uint32_t period_bytes;
	uint32_t buffer_size;
	uint32_t addr_align;
	uint32_t align;
	int err;

	trace_host_with_ids(dev, "host_params()");

	/* host params always installed by pipeline IPC */
	hd->host_size = dev->params.buffer.size;

	/* retrieve DMA buffer address alignment */
	err = dma_get_attribute(hd->dma, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (err < 0) {
		trace_host_error_with_ids(dev, "host_params() error: could not "
					  "get dma buffer address alignment, "
					  "err = %d", err);
		return err;
	}

	/* retrieve DMA buffer size alignment */
	err = dma_get_attribute(hd->dma, DMA_ATTR_BUFFER_ALIGNMENT, &align);
	if (err < 0 || !align) {
		trace_host_error_with_ids(dev, "host_params() error: could not "
					  "get valid dma buffer alignment, err "
					  "= %d, align = %u", err, align);
		return -EINVAL;
	}

	/* retrieve DMA buffer period count */
	err = dma_get_attribute(hd->dma, DMA_ATTR_BUFFER_PERIOD_COUNT,
				&period_count);
	if (err < 0 || !period_count) {
		trace_host_error_with_ids(dev, "host_params() error: could not "
					  "get valid dma buffer period count, "
					  "err = %d, period_count = %u", err,
					  period_count);
		return -EINVAL;
	}

	period_bytes = dev->frames * comp_frame_bytes(dev);
	if (!period_bytes) {
		trace_host_error_with_ids(dev, "host_params() error: invalid "
					  "period_bytes");
		return -EINVAL;
	}

	/* determine source and sink buffer elements */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
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
	buffer_size = ALIGN_UP(period_count * period_bytes, align);

	/* alloc DMA buffer or change its size if exists */
	if (hd->dma_buffer) {
		err = buffer_set_size(hd->dma_buffer, buffer_size);
		if (err < 0) {
			trace_host_error_with_ids(dev, "host_params() error: "
						  "buffer_set_size() failed, "
						  "buffer_size = %u",
						  buffer_size);
			return err;
		}
	} else {
		hd->dma_buffer = buffer_alloc(buffer_size, SOF_MEM_CAPS_DMA,
					      addr_align);
		if (!hd->dma_buffer) {
			trace_host_error_with_ids(dev, "host_params() error: "
						  "failed to alloc dma buffer");
			return -ENOMEM;
		}
	}

	/* create SG DMA elems for local DMA buffer */
	err = create_local_elems(dev, period_count, buffer_size / period_count);
	if (err < 0)
		return err;

	/* set up DMA configuration - copy in sample bytes. */
	config->src_width = comp_sample_bytes(dev);
	config->dest_width = comp_sample_bytes(dev);
	config->cyclic = 0;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->is_scheduling_source = comp_is_scheduling_source(dev);
	config->period = dev->pipeline->ipc_pipe.period;

	host_elements_reset(dev);

	dev->params.stream_tag -= 1;
	/* get DMA channel from DMAC
	 * note: stream_tag is ignored by dw-dma
	 */
	hd->chan = dma_channel_get(hd->dma, dev->params.stream_tag);
	if (!hd->chan) {
		trace_host_error_with_ids(dev, "host_params() error: "
					  "hd->chan is NULL");
		return -ENODEV;
	}

	err = dma_set_config(hd->chan, &hd->config);
	if (err < 0) {
		trace_host_error_with_ids(dev, "host_params() error: "
					  "dma_set_config() failed");
		dma_channel_put(hd->chan);
		hd->chan = NULL;
		return err;
	}

	err = dma_get_attribute(hd->dma, DMA_ATTR_COPY_ALIGNMENT,
				&hd->dma_copy_align);

	if (err < 0) {
		trace_host_error_with_ids(dev, "host_params() error: "
					  "dma_get_attribute()");

		return err;
	}

	/* set up callback */
	dma_set_cb(hd->chan, DMA_CB_TYPE_COPY, host_dma_cb, dev);

	/* set processing function */
	if (dev->params.frame_fmt == SOF_IPC_FRAME_S16_LE)
		hd->process = buffer_copy_s16;
	else
		hd->process = buffer_copy_s32;

	return 0;
}

static int host_prepare(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	int ret;

	trace_host_with_ids(dev, "host_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	hd->local_pos = 0;
	hd->report_pos = 0;
	dev->position = 0;

	return 0;
}

static int host_pointer_reset(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	/* reset buffer pointers */
	hd->local_pos = 0;
	hd->report_pos = 0;
	dev->position = 0;

	return 0;
}

static int host_position(struct comp_dev *dev,
			 struct sof_ipc_stream_posn *posn)
{
	struct host_data *hd = comp_get_drvdata(dev);

	/* TODO: improve accuracy by adding current DMA position */
	posn->host_posn = hd->local_pos;

	return 0;
}

static int host_reset(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	trace_host_with_ids(dev, "host_reset()");

	if (hd->chan)
		dma_channel_put(hd->chan);

	/* free all DMA elements */
	dma_sg_free(&hd->host.elem_array);
	dma_sg_free(&hd->local.elem_array);
	dma_sg_free(&hd->config.elem_array);

	/* free DMA buffer */
	if (hd->dma_buffer) {
		buffer_free(hd->dma_buffer);
		hd->dma_buffer = NULL;
	}

	/* reset dma channel as we have put it */
	hd->chan = NULL;

	host_pointer_reset(dev);
	hd->copy_type = COMP_COPY_NORMAL;
	hd->source = NULL;
	hd->sink = NULL;
	dev->state = COMP_STATE_READY;

	return 0;
}

static uint32_t host_buffer_get_copy_bytes(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *local_elem = hd->config.elem_array.elems;
	struct comp_buffer *local_buffer;
	uint32_t avail_bytes = 0;
	uint32_t free_bytes = 0;
	uint32_t copy_bytes = 0;
	uint32_t split_value;
	int ret;

	if (hd->copy_type == COMP_COPY_ONE_SHOT) {
		/* calculate minimum size to copy */
		if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
			local_buffer = list_first_item(&dev->bsink_list,
						       struct comp_buffer,
						       source_list);
			copy_bytes = local_buffer->free;
		} else {
			local_buffer = list_first_item(&dev->bsource_list,
						       struct comp_buffer,
						       sink_list);
			copy_bytes = local_buffer->avail;
		}

		/* copy_bytes should be aligned to minimum possible chunk of
		 * data to be copied by dma.
		 */
		copy_bytes = ALIGN_DOWN(copy_bytes, hd->dma_copy_align);

		split_value = host_dma_get_split(hd, copy_bytes);
		if (split_value)
			copy_bytes -= split_value;

		local_elem->size = copy_bytes;
	} else {
		/* get data sizes from DMA */
		ret = dma_get_data_size(hd->chan, &avail_bytes,
					&free_bytes);
		if (ret < 0) {
			trace_host_error("host_buffer_cb() error: "
					 "dma_get_data_size() failed, ret = %u",
					 ret);
			return 0;
		}

		/* calculate minimum size to copy */
		if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
			local_buffer = list_first_item(&dev->bsink_list,
						       struct comp_buffer,
						       source_list);
			copy_bytes = MIN(avail_bytes, local_buffer->free);
		} else {
			local_buffer = list_first_item(&dev->bsource_list,
						       struct comp_buffer,
						       sink_list);
			copy_bytes = MIN(local_buffer->avail, free_bytes);
		}

		/* copy_bytes should be aligned to minimum possible chunk of
		 * data to be copied by dma.
		 */
		copy_bytes = ALIGN_DOWN(copy_bytes, hd->dma_copy_align);
	}

	return copy_bytes;
}

/* copy and process stream data from source to sink buffers */
static int host_copy(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	uint32_t copy_bytes = 0;
	uint32_t flags = 0;
	int ret = 0;

	tracev_host_with_ids(dev, "host_copy()");

	if (dev->state != COMP_STATE_ACTIVE)
		return 0;

	if (hd->copy_type == COMP_COPY_BLOCKING)
		flags |= DMA_COPY_BLOCKING;
	else if (hd->copy_type == COMP_COPY_ONE_SHOT)
		flags |= DMA_COPY_ONE_SHOT;

	/* update first transfer manually */
	if (!dev->position && flags & COMP_COPY_ONE_SHOT)
		host_one_shot_cb(dev, hd->dma_buffer->size);

	copy_bytes = host_buffer_get_copy_bytes(dev);
	if (!copy_bytes) {
		trace_host_with_ids(dev, "host_copy(): no bytes to copy");
		return ret;
	}

	/* reconfigure transfer */
	ret = dma_set_config(hd->chan, &hd->config);
	if (ret < 0) {
		trace_host_error("host_copy() error: "
				 "dma_set_config() failed, ret = %u",
				 ret);
		return ret;
	}

	ret = dma_copy(hd->chan, copy_bytes, flags);
	if (ret < 0) {
		trace_host_error("host_copy() error: dma_copy() "
				 "failed, ret = %u", ret);
		return ret;
	}

	return ret;
}

static void host_cache(struct comp_dev *dev, int cmd)
{
	struct host_data *hd;

	trace_host_with_ids(dev, "host_cache(), cmd = %d", cmd);

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		trace_host_with_ids(dev, "host_cache(), CACHE_WRITEBACK_INV");

		hd = comp_get_drvdata(dev);

		dma_sg_cache_wb_inv(&hd->config.elem_array);
		dma_sg_cache_wb_inv(&hd->local.elem_array);

		dcache_writeback_invalidate_region(hd->dma, sizeof(*hd->dma));
		dcache_writeback_invalidate_region(hd, sizeof(*hd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_host_with_ids(dev, "host_cache(), CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		hd = comp_get_drvdata(dev);

		dcache_invalidate_region(hd, sizeof(*hd));
		dcache_invalidate_region(hd->dma, sizeof(*hd->dma));

		dma_sg_cache_inv(&hd->local.elem_array);
		dma_sg_cache_inv(&hd->config.elem_array);
		break;
	}
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

struct comp_driver comp_host = {
	.type	= SOF_COMP_HOST,
	.ops	= {
		.new		= host_new,
		.free		= host_free,
		.params		= host_params,
		.reset		= host_reset,
		.trigger	= host_trigger,
		.copy		= host_copy,
		.prepare	= host_prepare,
		.position	= host_position,
		.cache		= host_cache,
		.set_attribute	= host_set_attribute,
	},
};

static void sys_comp_host_init(void)
{
	comp_register(&comp_host);
}

DECLARE_MODULE(sys_comp_host_init);
