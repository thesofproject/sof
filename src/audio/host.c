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
	uint32_t period_bytes;	/**< Size of a single period (in bytes) */

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

	/* helpers used in split transactions */
	uint32_t split_value;
	uint32_t last_split_value;

	uint32_t dma_copy_align; /**< Minimal chunk of data possible to be
				   *  copied by dma connected to host
				   */

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

	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK)
		comp_update_buffer_produce(hd->dma_buffer, bytes);
	else
		comp_update_buffer_consume(hd->dma_buffer, bytes);

	dev->position += bytes;

	/* new local period, update host buffer position blks
	 * local_pos is queried by the ops.position() API
	 */
	hd->local_pos += bytes;

	/* buffer overlap, hardcode host buffer size at the moment */
	if (hd->local_pos >= hd->host_size)
		hd->local_pos = 0;

	/* NO_IRQ mode if host_period_size == 0 */
	if (dev->params.host_period_bytes != 0) {
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
static void host_dma_cb_irq(struct comp_dev *dev, struct dma_cb_data *next)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *local_elem = hd->config.elem_array.elems;
	struct dma_sg_elem *source_elem;
	struct dma_sg_elem *sink_elem;
	uint32_t curr_split_value = 0;
	uint32_t next_bytes = 0;
	uint32_t bytes = hd->last_split_value ? hd->last_split_value :
		local_elem->size;

	tracev_host("host_dma_cb_irq()");
	tracev_host("we have src at %p and dst at %p", (uintptr_t)local_elem->src, (uintptr_t)local_elem->dest);

	/* update position */
	host_update_position(dev, bytes);

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

	/* we need to perform split transfer */
	if (hd->split_value) {
		/* check for possible double split */
		curr_split_value = host_dma_get_split(hd, hd->split_value);
		if (curr_split_value) {
			next_bytes = hd->split_value - curr_split_value;
			hd->split_value = curr_split_value;
		} else {
			next_bytes = hd->split_value;
			hd->split_value = 0;
		}

		hd->last_split_value = next_bytes;

		next->elem.src = local_elem->src;
		next->elem.dest = local_elem->dest;
		next->elem.size = next_bytes;
		next->status = DMA_CB_STATUS_SPLIT;
		return;
	}

	hd->last_split_value = 0;

	next->status = DMA_CB_STATUS_END;
}

/* This is called by DMA driver every time when DMA completes its current
 * transfer between host and DSP.
 */
static void host_dma_cb(void *data, uint32_t type, struct dma_cb_data *next)
{
	struct comp_dev *dev = (struct comp_dev *)data;

	tracev_host("host_dma_cb()");

	switch (type) {
	case DMA_CB_TYPE_IRQ:
		host_dma_cb_irq(dev, next);
		break;
	case DMA_CB_TYPE_COPY:
		tracev_host("host_dma_cb() calling function that doesn't change next");
		host_update_position(dev, next->elem.size);
		break;
	default:
		trace_host_error("host_dma_cb() error: Wrong callback type "
				 "= %u", type);
		break;
	}
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

	/* we should ignore any trigger commands when doing one shot,
	 * because transfers will start in copy and stop automatically
	 */
	if (hd->copy_type == COMP_COPY_ONE_SHOT)
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

	trace_host("host_new() dev: %p", (intptr_t)dev);
	trace_host("host_new() hd: %p", (intptr_t)hd);

	/* request HDA DMA with shared access privilege */
	dir = ipc_host->direction == SOF_IPC_STREAM_PLAYBACK ?
		DMA_DIR_HMEM_TO_LMEM : DMA_DIR_LMEM_TO_HMEM;

	//return dev;
	hd->dma = dma_get(dir, 0, DMA_DEV_HOST, DMA_ACCESS_SHARED);
	if (!hd->dma) {
		trace_host_error("host_new() error: dma_get() returned NULL");
		rfree(hd);
		rfree(dev);
		return NULL;
	}
	tracev_host("host_new() got dma ID %d (expected 1)", hd->dma->plat_data.id);

	/* init buffer elems */
	dma_sg_init(&hd->config.elem_array);
	dma_sg_init(&hd->host.elem_array);
	dma_sg_init(&hd->local.elem_array);

	hd->chan = NULL;
	hd->copy_type = COMP_COPY_NORMAL;
	hd->posn.comp_id = comp->id;
	dev->state = COMP_STATE_READY;
	dev->is_dma_connected = 1;

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

static uint32_t host_buffer_get_copy_bytes(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *local_elem = hd->config.elem_array.elems;
	uint32_t avail_bytes = 0;
	uint32_t free_bytes = 0;
	uint32_t copy_bytes = 0;
	int ret;

	if (hd->copy_type == COMP_COPY_ONE_SHOT) {
		/* calculate minimum size to copy */
		copy_bytes = dev->params.direction == SOF_IPC_STREAM_PLAYBACK ?
			hd->dma_buffer->free : hd->dma_buffer->avail;

		/* copy_bytes should be aligned to minimum possible chunk of
		 * data to be copied by dma.
		 */
		copy_bytes = ALIGN_DOWN(copy_bytes, hd->dma_copy_align);

		hd->split_value = host_dma_get_split(hd, copy_bytes);
		if (hd->split_value)
			copy_bytes -= hd->split_value;

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
		copy_bytes = dev->params.direction == SOF_IPC_STREAM_PLAYBACK ?
			MIN(avail_bytes, hd->dma_buffer->free) :
			MIN(hd->dma_buffer->avail, free_bytes);

		/* copy_bytes should be aligned to minimum possible chunk of
		 * data to be copied by dma.
		 */
		copy_bytes = ALIGN_DOWN(copy_bytes, hd->dma_copy_align);
	}

	return copy_bytes;
}

static void host_buffer_cb(void *data, uint32_t bytes)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct host_data *hd = comp_get_drvdata(dev);
	uint32_t copy_bytes = host_buffer_get_copy_bytes(dev);
	uint32_t flags = 0;
	int ret;

	tracev_host("host_buffer_cb(), copy_bytes = 0x%x", copy_bytes);

	if (hd->copy_type == COMP_COPY_BLOCKING)
		flags |= DMA_COPY_BLOCKING;
	else if (hd->copy_type == COMP_COPY_ONE_SHOT)
		flags |= DMA_COPY_ONE_SHOT;

	/* reconfigure transfer */
	ret = dma_set_config(hd->chan, &hd->config);
	if (ret < 0) {
		trace_host_error("host_buffer_cb() error: dma_set_config() "
				 "failed, ret = %u", ret);
		return;
	}

	ret = dma_copy(hd->chan, copy_bytes, flags);
	if (ret < 0)
		trace_host_error("host_buffer_cb() error: dma_copy() failed, "
				 "ret = %u", ret);
}

/* configure the DMA params and descriptors for host buffer IO */
static int host_params(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *cconfig = COMP_GET_CONFIG(dev);
	struct dma_sg_config *config = &hd->config;
	uint32_t buffer_size;
	uint32_t buffer_count;
	uint32_t buffer_single_size;
	uint32_t period_count;
	uint32_t align;
	int err;

	trace_host_with_ids(dev, "host_params()");
	tracev_host("dev is %p", (intptr_t)(void *)dev);
	tracev_host("hd->dma is %p", (intptr_t)(void *)hd->dma);

	/* host params always installed by pipeline IPC */
	hd->host_size = dev->params.buffer.size;
	tracev_host("host_params(): Got host size; if this shows we're good for now");

	err = dma_get_attribute(hd->dma, DMA_ATTR_BUFFER_ALIGNMENT, &align);
	if (err < 0) {
		trace_host_error_with_ids(dev, "could not get dma buffer "
					  "alignment");
		return err;
	}
	tracev_host("host_params() got DMA buffer alignment");

	/* determine source and sink buffer elems */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		tracev_host("host_params() for playback...");
		hd->dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);

		/* set callback on buffer consume */
		buffer_set_cb(hd->dma_buffer, &host_buffer_cb, dev,
			      BUFF_CB_TYPE_CONSUME);

		config->direction = DMA_DIR_HMEM_TO_LMEM;

		period_count = cconfig->periods_sink;

		buffer_count = hd->host.elem_array.count ? 1 : period_count;
		buffer_single_size = ALIGN_UP(dev->frames *
					      comp_frame_bytes(dev),
					      align);

		if (hd->host.elem_array.count)
			buffer_single_size *= period_count;

		hd->source = &hd->host;
		hd->sink = &hd->local;
	} else {
		tracev_host("host_params() for capture...");
		hd->dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);

		/* set callback on buffer produce */
		buffer_set_cb(hd->dma_buffer, &host_buffer_cb, dev,
			      BUFF_CB_TYPE_PRODUCE);

		config->direction = DMA_DIR_LMEM_TO_HMEM;

		period_count = cconfig->periods_source;

		buffer_count = hd->host.elem_array.count ? 1 : period_count;
		buffer_single_size = ALIGN_UP(dev->frames *
					      comp_frame_bytes(dev),
					      align);

		if (hd->host.elem_array.count)
			buffer_single_size *= period_count;

		hd->source = &hd->local;
		hd->sink = &hd->host;
	}

	/* validate period count */
	if (!period_count) {
		trace_host_error_with_ids(dev, "host_params() error: invalid "
					  "period_count");
		return -EINVAL;
	}

	tracev_host("host_params() about to check period_bytes");
	hd->period_bytes = ALIGN_UP(dev->frames * comp_frame_bytes(dev), align);
	tracev_host("host_params() set period_bytes");

	if (hd->period_bytes == 0) {
		trace_host_error_with_ids(dev, "host_params() error: invalid "
					  "period_bytes");
		return -EINVAL;
	}

	/* resize the buffer if space is available to align with period size */
	buffer_size = period_count * hd->period_bytes;
	tracev_host("host_params() about to set buffer size...");
	err = buffer_set_size(hd->dma_buffer, buffer_size);
	tracev_host("host_params() did set buffer size");
	if (err < 0) {
		trace_host_error_with_ids(dev, "host_params() error:"
					  "buffer_set_size() failed, "
					  "buffer_size = %u", buffer_size);
		return err;
	}

	/* create SG DMA elems for local DMA buffer */
	err = create_local_elems(dev, buffer_count, buffer_single_size);
	if (err < 0)
		return err;

	/* set up DMA configuration - copy in sample bytes. */
	config->src_width = comp_sample_bytes(dev);
	config->dest_width = comp_sample_bytes(dev);
	config->cyclic = 0;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);

	tracev_host("host_params() about to call host_elements_reset");
	host_elements_reset(dev);
	tracev_host("host_params() elements reset ok");

	dev->params.stream_tag -= 1;
	/* get DMA channel from DMAC
	 * note: stream_tag is ignored by dw-dma
	 */
	tracev_host("host_params() before dma_channel_get");
	hd->chan = dma_channel_get(hd->dma, dev->params.stream_tag);
	tracev_host("host_params() after dma_channel_get");
	if (!hd->chan) {
		trace_host_error_with_ids(dev, "host_params() error: "
					  "hd->chan is NULL");
		return -ENODEV;
	}

	err = dma_set_config(hd->chan, &hd->config);
	tracev_host("host_params(0 after dma_set_config");
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
	dma_set_cb(hd->chan, DMA_CB_TYPE_IRQ |
		   DMA_CB_TYPE_COPY, host_dma_cb, dev);

	tracev_host("host_params() ended successfully");
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
	hd->split_value = 0;
	hd->last_split_value = 0;
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
	tracev_host("dev is %p", (intptr_t)dev);
	tracev_host("hd is %p", (intptr_t)hd);
	tracev_host("hd->chan is %d", hd->chan->index);

	tracev_host("host_reset about to put DMA channel");
	dma_channel_put(hd->chan);

	/* free all DMA elements */
	tracev_host("host_reset about to free sg elements");
	dma_sg_free(&hd->host.elem_array);
	dma_sg_free(&hd->local.elem_array);
	dma_sg_free(&hd->config.elem_array);

	/* reset dma channel as we have put it */
	hd->chan = NULL;

	tracev_host("host_reset about to do host_pointer_reset");
	host_pointer_reset(dev);
	hd->copy_type = COMP_COPY_NORMAL;
	hd->source = NULL;
	hd->sink = NULL;
	dev->state = COMP_STATE_READY;

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int host_copy(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	int ret = 0;

	tracev_host_with_ids(dev, "host_copy()");

	if (dev->state != COMP_STATE_ACTIVE)
		return 0;

	/* here only do preload, further copies happen
	 * in host_buffer_cb()
	 */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK &&
	    !dev->position) {
		ret = dma_copy(hd->chan, hd->dma_buffer->size,
			       DMA_COPY_PRELOAD);
		if (ret < 0) {
			if (ret == -ENODATA) {
				/* preload not finished, so stop processing */
				trace_host_with_ids(dev, "host_copy(), preload"
						    " not yet finished");
				return PPL_STATUS_PATH_STOP;
			}

			trace_host_error_with_ids(dev, "host_copy() error: "
						  "dma_copy() failed, "
						  "ret = %u", ret);
			return ret;
		}
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
