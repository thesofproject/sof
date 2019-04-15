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
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/trace.h>
#include <sof/dma.h>
#include <sof/ipc.h>
#include <sof/wait.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <platform/dma.h>
#include <arch/cache.h>
#include <uapi/ipc/dai.h>

#define trace_host(__e, ...)	trace_event(TRACE_CLASS_HOST, __e, ##__VA_ARGS__)
#define tracev_host(__e, ...)	tracev_event(TRACE_CLASS_HOST, __e, ##__VA_ARGS__)
#define trace_host_error(__e, ...)	trace_error(TRACE_CLASS_HOST, __e, ##__VA_ARGS__)

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
	int chan;
	struct dma_sg_config config;
	struct comp_buffer *dma_buffer;

	uint32_t period_bytes;	/**< Size of a single period (in bytes) */
	uint32_t period_count;	/**< Number of periods */

	/* host position reporting related */
	uint32_t host_size;	/**< Host buffer size (in bytes) */
	uint32_t report_pos;	/**< Position in current report period */
	uint32_t local_pos;	/**< Local position in host buffer */

	/* host component attributes */
	uint32_t copy_blocking;	/**< True for copy in blocking mode */

	/* local and host DMA buffer info */
#if !CONFIG_DMA_GW
	struct hc_buf host;
	struct hc_buf local;
	/* pointers set during params to host or local above */
	struct hc_buf *source;
	struct hc_buf *sink;
	uint32_t split_remaining;
#endif

	/* stream info */
	struct sof_ipc_stream_posn posn; /* TODO: update this */
};

static int host_stop(struct comp_dev *dev);
static int host_copy(struct comp_dev *dev);
#if !CONFIG_DMA_GW
static int host_copy_one_shot(struct comp_dev *dev);

static inline struct dma_sg_elem *next_buffer(struct hc_buf *hc)
{
	if (!hc->elem_array.elems || !hc->elem_array.count)
		return NULL;
	if (++hc->current == hc->elem_array.count)
		hc->current = 0;
	return hc->elem_array.elems + hc->current;
}

#endif

/*
 * Host period copy between DSP and host DMA completion.
 * This is called  by DMA driver every time when DMA completes its current
 * transfer between host and DSP. The host memory is not guaranteed to be
 * continuous and also not guaranteed to have a period/buffer size that is a
 * multiple of the DSP period size. This means we must check we do not
 * overflow host period/buffer/page boundaries on each transfer and split the
 * DMA transfer if we do overflow.
 */
static void host_dma_cb(void *data, uint32_t type, struct dma_sg_elem *next)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct host_data *hd = comp_get_drvdata(dev);
#if CONFIG_DMA_GW
	uint32_t bytes = next->size;
#else
	struct dma_sg_elem *local_elem;
	struct dma_sg_elem *source_elem;
	struct dma_sg_elem *sink_elem;
	uint32_t next_size;
	uint32_t need_copy = 0;
	uint32_t period_bytes = hd->period_bytes;
	uint32_t bytes;

	local_elem = hd->config.elem_array.elems;

	bytes = local_elem->size;
#endif

	tracev_host("host_dma_cb()");

	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK)
		/* recalc available buffer space */
		comp_update_buffer_produce(hd->dma_buffer, bytes);
	else
		/* recalc available buffer space */
		comp_update_buffer_consume(hd->dma_buffer, bytes);

	dev->position += bytes;

	/* new local period, update host buffer position blks
	 * local_pos is queried by the ops.potision() API
	 */
	hd->local_pos += bytes;

	/* buffer overlap, hard code host buffer size at the moment ? */
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

#if !CONFIG_DMA_GW
	/* update src and dest positions and check for overflow */
	local_elem->src += bytes;
	local_elem->dest += bytes;
	if (local_elem->src == hd->source->current_end) {
		/* end of elem, so use next */

		source_elem = next_buffer(hd->source);
		hd->source->current_end = source_elem->src + source_elem->size;
		local_elem->src = source_elem->src;
	}
	if (local_elem->dest == hd->sink->current_end) {
		/* end of elem, so use next */

		sink_elem = next_buffer(hd->sink);
		hd->sink->current_end = sink_elem->dest + sink_elem->size;
		local_elem->dest = sink_elem->dest;
	}

	/* calc size of next transfer */
	next_size = period_bytes;
	if (local_elem->src + next_size > hd->source->current_end)
		next_size = hd->source->current_end - local_elem->src;
	if (local_elem->dest + next_size > hd->sink->current_end)
		next_size = hd->sink->current_end - local_elem->dest;

	/* are we dealing with a split transfer ? */
	if (!hd->split_remaining) {

		/* no, is next transfer split ? */
		if (next_size != period_bytes)
			hd->split_remaining = period_bytes - next_size;
	} else {

		/* yes, than calc transfer size */
		need_copy = 1;
		next_size = next_size < hd->split_remaining ?
			next_size : hd->split_remaining;
		hd->split_remaining -= next_size;
	}
	local_elem->size = next_size;

	/* schedule immediate split transfer if needed */
	if (need_copy) {
		next->src = local_elem->src;
		next->dest = local_elem->dest;
		next->size = local_elem->size;
		return;
	}

	next->size = DMA_RELOAD_END;
#endif
}

static int create_local_elems(struct comp_dev *dev, uint32_t buffer_count,
			      uint32_t buffer_bytes)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem_array *elem_array;
	int err;

#if CONFIG_DMA_GW
	elem_array = &hd->config.elem_array;
#else
	elem_array = &hd->local.elem_array;
#endif
	err = dma_sg_alloc(elem_array, RZONE_RUNTIME,
			   dev->params.direction == SOF_IPC_STREAM_PLAYBACK ?
				DMA_DIR_HMEM_TO_LMEM : DMA_DIR_LMEM_TO_HMEM,
			   buffer_count, buffer_bytes,
			   (uintptr_t)(hd->dma_buffer->addr), 0);

	if (err < 0) {
		trace_host_error("create_local_elems() error: "
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
#if defined CONFIG_DMA_GW
	struct host_data *hd = comp_get_drvdata(dev);
#endif
	int ret = 0;

	trace_host("host_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		goto out;

	if (ret == COMP_STATUS_STATE_ALREADY_SET) {
		ret = PPL_STATUS_PATH_STOP;
		goto out;
	}

	switch (cmd) {
	case COMP_TRIGGER_STOP:
		ret = host_stop(dev);
		/* host stop fails, let's emit the error log */
		if (ret < 0)
			trace_host_error("host_trigger(): host stop failed: %d",
					 ret);
		/* fall through */
	case COMP_TRIGGER_XRUN:
/* TODO: add attribute to dma interface and do run-time if() here */
#if CONFIG_DMA_GW
		ret = dma_stop(hd->dma, hd->chan);
		if (ret < 0)
			trace_host_error("host_trigger(): dma stop failed: %d",
					 ret);
#endif
		break;
	case COMP_TRIGGER_START:
#if CONFIG_DMA_GW
		if (hd->chan < 0) {
			ret = -EINVAL;
			trace_host_error("host_trigger() error:"
							 "no dma channel configured");
			goto out;
		}

		ret = dma_start(hd->dma, hd->chan);
		if (ret < 0) {
			trace_host_error("host_trigger() error: "
					"dma_start() failed, ret = %u", ret);
			goto out;
		}
#else
		/* preload first playback period for preloader task */
		if (pipeline_is_preload(dev->pipeline))
			ret = host_copy_one_shot(dev);
#endif
		break;
	default:
		break;
	}

out:
	return ret;
}

static struct comp_dev *host_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct host_data *hd;
	struct sof_ipc_comp_host *host;
	struct sof_ipc_comp_host *ipc_host = (struct sof_ipc_comp_host *)comp;
	uint32_t dir, caps, dma_dev;
	int err;


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
	err = memcpy_s(host, sizeof(*host),
	   ipc_host, sizeof(struct sof_ipc_comp_host));

	hd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*hd));
	if (!hd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, hd);

	/* request HDA DMA with shared access privilege */
	if (ipc_host->direction == SOF_IPC_STREAM_PLAYBACK)
		dir =  DMA_DIR_HMEM_TO_LMEM;
	else
		dir =  DMA_DIR_LMEM_TO_HMEM;

	caps = 0;
	dma_dev = DMA_DEV_HOST;
	hd->dma = dma_get(dir, caps, dma_dev, DMA_ACCESS_SHARED);

	if (!hd->dma) {
		trace_host_error("host_new() error: dma_get() returned NULL");
		goto error;
	}

	/* init buffer elems */
#if CONFIG_DMA_GW
	dma_sg_init(&hd->config.elem_array);
#else
	dma_sg_init(&hd->host.elem_array);
	dma_sg_init(&hd->local.elem_array);

	err = dma_sg_alloc(&hd->config.elem_array, RZONE_RUNTIME,
			   dir, 1, 0, 0, 0);
#endif
	if (err < 0)
		goto error;


	hd->chan = DMA_CHAN_INVALID;
	hd->copy_blocking = 0;

	/* init posn data. TODO: other fields */
	hd->posn.comp_id = comp->id;
	dev->state = COMP_STATE_READY;
	dev->is_dma_connected = 1;
	return dev;

error:
	rfree(hd);
	rfree(dev);
	return NULL;
}

static void host_free(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	trace_host("host_free()");

	dma_put(hd->dma);

	dma_sg_free(&hd->config.elem_array);
	rfree(hd);
	rfree(dev);
}

#if !CONFIG_DMA_GW
static int host_elements_reset(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *source_elem;
	struct dma_sg_elem *sink_elem;
	struct dma_sg_elem *local_elem;

	/* setup elem to point to first source elem */
	source_elem = hd->source->elem_array.elems;
	hd->source->current = 0;
	hd->source->current_end = source_elem->src + source_elem->size;

	/* setup elem to point to first sink elem */
	sink_elem = hd->sink->elem_array.elems;
	hd->sink->current = 0;
	hd->sink->current_end = sink_elem->dest + sink_elem->size;

	/* local element */
	local_elem = hd->config.elem_array.elems;
	local_elem->dest = sink_elem->dest;
	local_elem->size = dev->params.direction == SOF_IPC_STREAM_PLAYBACK ?
		sink_elem->size : source_elem->size;
	local_elem->src = source_elem->src;

	return 0;
}
#endif

static void host_buffer_cb(void *data, uint32_t bytes)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct host_data *hd = comp_get_drvdata(dev);
	uint32_t avail_bytes = 0;
	uint32_t free_bytes = 0;
	uint32_t copy_bytes = 0;
	uint32_t flags = 0;
	int ret;

	/* get data sizes from DMA */
	ret = dma_get_data_size(hd->dma, hd->chan, &avail_bytes, &free_bytes);
	if (ret < 0) {
		trace_host_error("host_buffer_cb() error: dma_get_data_size() "
				 "failed, ret = %u", ret);
		return;
	}

	/* calculate minimum size to copy */
	copy_bytes = dev->params.direction == SOF_IPC_STREAM_PLAYBACK ?
		MIN(avail_bytes, hd->dma_buffer->free) :
		MIN(hd->dma_buffer->avail, free_bytes);

	copy_bytes = MIN(copy_bytes, bytes);

	tracev_host("host_buffer_cb(), copy_bytes = 0x%x", copy_bytes);

	if (hd->copy_blocking)
		flags |= DMA_COPY_BLOCKING;

#if CONFIG_DMA_GW
	ret = dma_copy(hd->dma, hd->chan, copy_bytes, flags);
	if (ret < 0)
		trace_host_error("host_buffer_cb() error: dma_copy() failed, "
				 "ret = %u", ret);
#else
	ret = host_copy_one_shot(dev);
	if (ret < 0)
		trace_host_error("host_buffer_cb() error: host_copy_one_shot()"
				 " failed, ret = %u", ret);
#endif
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
	int err;

	trace_event(TRACE_CLASS_HOST, "host_params()");

	/* host params always installed by pipeline IPC */
	hd->host_size = dev->params.buffer.size;

	/* determine source and sink buffer elems */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		hd->dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);

		/* set callback on buffer consume */
		buffer_set_cb(hd->dma_buffer, &host_buffer_cb, dev,
			      BUFF_CB_TYPE_CONSUME);

		config->direction = DMA_DIR_HMEM_TO_LMEM;
		hd->period_count = cconfig->periods_sink;
#if CONFIG_DMA_GW
		buffer_count = hd->period_count;
		buffer_single_size = dev->frames * comp_frame_bytes(dev);
#else
		buffer_count = 1;
		buffer_single_size = hd->period_count * dev->frames *
			comp_frame_bytes(dev);

		hd->source = &hd->host;
		hd->sink = &hd->local;
#endif
	} else {
		hd->dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);

		/* set callback on buffer produce */
		buffer_set_cb(hd->dma_buffer, &host_buffer_cb, dev,
			      BUFF_CB_TYPE_PRODUCE);

		config->direction = DMA_DIR_LMEM_TO_HMEM;
		hd->period_count = cconfig->periods_source;
		buffer_count = hd->period_count;
		buffer_single_size = dev->frames * comp_frame_bytes(dev);

#if !CONFIG_DMA_GW
		hd->source = &hd->local;
		hd->sink = &hd->host;
#endif
	}

	/* validate period count */
	if (hd->period_count == 0) {
		trace_host_error("host_params() error: invalid period_count");
		return -EINVAL;
	}

	/* calculate period size based on config */
	hd->period_bytes = dev->frames * comp_frame_bytes(dev);
	if (hd->period_bytes == 0) {
		trace_host_error("host_params() error: invalid period_bytes");
		return -EINVAL;
	}

	/* resize the buffer if space is available to align with period size */
	buffer_size = hd->period_count * hd->period_bytes;
	err = buffer_set_size(hd->dma_buffer, buffer_size);
	if (err < 0) {
		trace_host_error("host_params() error:"
				 "buffer_set_size() failed, buffer_size = %u",
				 buffer_size);
		return err;
	}

#if !CONFIG_DMA_GW
	/* component buffer size must be divisor of host buffer size */
	if (hd->host_size % hd->period_bytes) {
		trace_comp_error("host_params() error: component buffer size "
				"is not a divisor of host buffer size, "
				"host_size = %u, period_bytes = %u",
				hd->host_size, hd->period_bytes);
		return -EINVAL;
	}
#endif

	/* create SG DMA elems for local DMA buffer */
	err = create_local_elems(dev, buffer_count, buffer_single_size);
	if (err < 0)
		return err;

	/* set up DMA configuration - copy in sample bytes. */
	config->src_width = comp_sample_bytes(dev);
	config->dest_width = comp_sample_bytes(dev);
	config->cyclic = 0;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);

#if CONFIG_DMA_GW
	dev->params.stream_tag -= 1;
	/* get DMA channel from DMAC
	 * note: stream_tag is ignored by dw-dma
	 */
	hd->chan = dma_channel_get(hd->dma, dev->params.stream_tag);
	if (hd->chan < 0) {
		trace_host_error("host_params() error: hd->chan < 0");
		return -ENODEV;
	}
	err = dma_set_config(hd->dma, hd->chan, &hd->config);
	if (err < 0) {
		trace_host_error("host_params() error: "
				 "dma_set_config() failed");
		dma_channel_put(hd->dma, hd->chan);
		hd->chan = DMA_CHAN_INVALID;
		return err;
	}
#else
	host_elements_reset(dev);
	/* get DMA channel from DMAC
	 * note: stream_tag is ignored by dw-dma
	 */
	hd->chan = dma_channel_get(hd->dma, 0);
	if (hd->chan < 0) {
		trace_host_error("host_params() error: hd->chan < 0");
		return -ENODEV;
	}
#endif
	/* set up callback */
	dma_set_cb(hd->dma, hd->chan, DMA_CB_TYPE_IRQ |
		   DMA_CB_TYPE_COPY, host_dma_cb, dev);

	return 0;
}

static int host_prepare(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	int ret;

	trace_host("host_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	hd->local_pos = 0;
	hd->report_pos = 0;
#if !CONFIG_DMA_GW
	hd->split_remaining = 0;
#endif
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

static int host_stop(struct comp_dev *dev)
{
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

#if !CONFIG_DMA_GW
static int host_buffer(struct comp_dev *dev,
		       struct dma_sg_elem_array *elem_array,
		       uint32_t host_size)
{
	struct host_data *hd = comp_get_drvdata(dev);

	hd->host.elem_array = *elem_array;

	return 0;
}
#endif

static int host_reset(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	trace_host("host_reset()");

#if !CONFIG_DMA_GW
	dma_channel_put(hd->dma, hd->chan);
	/* free all host DMA elements */
	dma_sg_free(&hd->host.elem_array);

	/* free all local DMA elements */
	dma_sg_free(&hd->local.elem_array);
#endif

#if CONFIG_DMA_GW
	dma_stop(hd->dma, hd->chan);
	dma_channel_put(hd->dma, hd->chan);

	/* free array for hda-dma only, do not free single one for dw-dma */
	dma_sg_free(&hd->config.elem_array);
#endif

	/* reset dma channel as we have put it */
	hd->chan = DMA_CHAN_INVALID;

	hd->copy_blocking = 0;

	host_pointer_reset(dev);
#if !CONFIG_DMA_GW
	hd->source = NULL;
	hd->sink = NULL;
#endif
	dev->state = COMP_STATE_READY;

	return 0;
}

#if !CONFIG_DMA_GW
/* perform one shot copy from source to sink buffers */
static int host_copy_one_shot(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	int ret;

	/* do DMA transfer */
	ret = dma_set_config(hd->dma, hd->chan, &hd->config);
	if (ret < 0)
		return ret;

	ret = dma_start(hd->dma, hd->chan);

	return ret;
}
#endif

/* copy and process stream data from source to sink buffers */
static int host_copy(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *local_elem;
	int ret = 0;

	tracev_host("host_copy()");

	if (dev->state != COMP_STATE_ACTIVE)
		return 0;

	local_elem = hd->config.elem_array.elems;

	/* enough free or avail to copy ? */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		if (hd->dma_buffer->free < local_elem->size) {
			/* buffer is enough avail, just return. */
			tracev_host("host_copy(), buffer is enough avail");
			return 0;
		}
	} else {
		if (hd->dma_buffer->avail < local_elem->size) {
			/* buffer is enough empty, just return. */
			tracev_host("host_copy(), buffer is enough empty");
			return 0;
		}
	}

	/* TODO: this could be run-time if() based on the same attribute
	 * as in the host_trigger().
	 */
#if CONFIG_DMA_GW
	/* here only do preload, further copies happen
	 * in host_buffer_cb()
	 */
	if (pipeline_is_preload(dev->pipeline)) {
		ret = dma_copy(hd->dma, hd->chan, hd->dma_buffer->size,
			       DMA_COPY_PRELOAD | DMA_COPY_BLOCKING);
		if (ret < 0) {
			trace_host_error("host_copy() error: dma_copy() "
					 "failed, ret = %u", ret);
			return ret;
		}
	}
#endif
	/* For !CONFIG_DMA_GW preload happens in host_trigger() and
	 * further copies happen in host_buffer_cb()
	 */

	return ret;
}

static void host_cache(struct comp_dev *dev, int cmd)
{
	struct host_data *hd;

	trace_event(TRACE_CLASS_HOST, "host_cache(), cmd = %d", cmd);

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		trace_host("host_cache(), CACHE_WRITEBACK_INV");

		hd = comp_get_drvdata(dev);

		dma_sg_cache_wb_inv(&hd->config.elem_array);

#if !CONFIG_DMA_GW
		dma_sg_cache_wb_inv(&hd->local.elem_array);
#endif

		dcache_writeback_invalidate_region(hd->dma, sizeof(*hd->dma));
		dcache_writeback_invalidate_region(hd, sizeof(*hd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_host("host_cache(), CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		hd = comp_get_drvdata(dev);
		dcache_invalidate_region(hd, sizeof(*hd));
		dcache_invalidate_region(hd->dma, sizeof(*hd->dma));

#if !CONFIG_DMA_GW
		dma_sg_cache_inv(&hd->local.elem_array);
#endif

		dma_sg_cache_inv(&hd->config.elem_array);
		break;
	}
}

static int host_set_attribute(struct comp_dev *dev, uint32_t type,
			      uint32_t value)
{
	struct host_data *hd = comp_get_drvdata(dev);

	switch (type) {
	case COMP_ATTR_COPY_BLOCKING:
		hd->copy_blocking = value;
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
#if !CONFIG_DMA_GW
		.host_buffer	= host_buffer,
#endif
		.position	= host_position,
		.cache		= host_cache,
		.set_attribute	= host_set_attribute,
	},
};

static void sys_comp_host_init(void)
{
	comp_register(&comp_host);
}

DECLARE_COMPONENT(sys_comp_host_init);
