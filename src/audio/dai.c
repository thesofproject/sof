// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pcm_converter.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/edma.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/list.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* tracing */
#define trace_dai(__e, ...)					\
	trace_event(TRACE_CLASS_DAI, __e, ##__VA_ARGS__)
#define trace_dai_with_ids(comp_ptr, __e, ...)			\
	trace_event_comp(TRACE_CLASS_DAI, comp_ptr,		\
			 __e, ##__VA_ARGS__)

#define tracev_dai(__e, ...)					\
	tracev_event(TRACE_CLASS_DAI, __e, ##__VA_ARGS__)
#define tracev_dai_with_ids(comp_ptr, __e, ...)			\
	tracev_event_comp(TRACE_CLASS_DAI, comp_ptr,		\
			  __e, ##__VA_ARGS__)

#define trace_dai_error(__e, ...)				\
	trace_error(TRACE_CLASS_DAI, __e, ##__VA_ARGS__)
#define trace_dai_error_with_ids(comp_ptr, __e, ...)		\
	trace_error_comp(TRACE_CLASS_DAI, comp_ptr,		\
			 __e, ##__VA_ARGS__)

struct dai_data {
	/* local DMA config */
	struct dma_chan_data *chan;
	uint32_t stream_id;
	struct dma_sg_config config;
	struct comp_buffer *dma_buffer;
	struct comp_buffer *local_buffer;

	struct dai *dai;
	struct dma *dma;
	enum sof_ipc_frame frame_fmt;
	int xrun;		/* true if we are doing xrun recovery */

	pcm_converter_func process;	/* processing function */

	uint32_t dai_pos_blks;	/* position in bytes (nearest block) */
	uint64_t start_position;	/* position on start */

	/* host can read back this value without IPC */
	uint64_t *dai_pos;

	uint64_t wallclock;	/* wall clock at stream start */
};

/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_cb(void *arg, enum notify_id type, void *data)
{
	struct dma_cb_data *next = data;
	struct comp_dev *dev = arg;
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t bytes = next->elem.size;
	uint32_t samples = bytes / sample_bytes(dd->frame_fmt);
	void *buffer_ptr;

	tracev_dai_with_ids(dev, "dai_dma_cb()");

	next->status = DMA_CB_STATUS_RELOAD;

	/* stop dma copy for pause/stop/xrun */
	if (dev->state != COMP_STATE_ACTIVE || dd->xrun) {
		/* stop the DAI */
		dai_trigger(dd->dai, COMP_TRIGGER_STOP, dev->direction);

		/* tell DMA not to reload */
		next->status = DMA_CB_STATUS_END;
	}

	/* is our pipeline handling an XRUN ? */
	if (dd->xrun) {
		/* make sure we only playback silence during an XRUN */
		if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
			/* fill buffer with silence */
			buffer_zero(dd->dma_buffer);

		return;
	}

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		dma_buffer_copy_to(dd->local_buffer, samples *
				   buffer_sample_bytes(dd->local_buffer),
				   dd->dma_buffer, bytes, dd->process, samples);

		buffer_ptr = dd->local_buffer->r_ptr;
	} else {
		dma_buffer_copy_from(dd->dma_buffer, bytes, dd->local_buffer,
				     samples *
				     buffer_sample_bytes(dd->local_buffer),
				     dd->process, samples);

		buffer_ptr = dd->local_buffer->w_ptr;
	}

	/* update host position (in bytes offset) for drivers */
	dev->position += bytes;
	if (dd->dai_pos) {
		dd->dai_pos_blks += bytes;
		*dd->dai_pos = dd->dai_pos_blks +
			(char *)buffer_ptr - (char *)dd->dma_buffer->addr;
	}
}

static struct comp_dev *dai_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_dai *dai;
	struct sof_ipc_comp_dai *ipc_dai = (struct sof_ipc_comp_dai *)comp;
	struct dai_data *dd;
	uint32_t dir, caps, dma_dev;
	int ret;

	trace_dai("dai_new()");

	if (IPC_IS_SIZE_INVALID(ipc_dai->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_DAI, ipc_dai->config);
		return NULL;
	}

	dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_dai));
	if (!dev)
		return NULL;

	dai = (struct sof_ipc_comp_dai *)&dev->comp;
	ret = memcpy_s(dai, sizeof(*dai), ipc_dai,
		       sizeof(struct sof_ipc_comp_dai));
	assert(!ret);

	dd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*dd));
	if (!dd) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, dd);

	dd->dai = dai_get(dai->type, dai->dai_index, DAI_CREAT);
	if (!dd->dai) {
		trace_dai_error("dai_new() error: dai_get() failed to create "
				"DAI.");
		goto error;
	}

	/* request GP LP DMA with shared access privilege */
	dir = dai->direction == SOF_IPC_STREAM_PLAYBACK ?
			DMA_DIR_MEM_TO_DEV : DMA_DIR_DEV_TO_MEM;

	caps = dai_get_info(dd->dai, DAI_INFO_DMA_CAPS);
	dma_dev = dai_get_info(dd->dai, DAI_INFO_DMA_DEV);

	dd->dma = dma_get(dir, caps, dma_dev, DMA_ACCESS_SHARED);
	if (!dd->dma) {
		trace_dai_error("dai_new() error: dma_get() failed to get "
				"shared access to DMA.");
		goto error;
	}

	dma_sg_init(&dd->config.elem_array);
	dd->dai_pos = NULL;
	dd->dai_pos_blks = 0;
	dd->xrun = 0;
	dd->chan = NULL;

	dev->state = COMP_STATE_READY;
	return dev;

error:
	rfree(dd);
	rfree(dev);
	return NULL;
}

static void dai_free(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dd->chan)
		dma_channel_put(dd->chan);

	dma_put(dd->dma);

	dai_put(dd->dai);

	rfree(dd);
	rfree(dev);
}

/* set component audio SSP and DMA configuration */
static int dai_playback_params(struct comp_dev *dev, uint32_t period_bytes,
			       uint32_t period_count)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	uint32_t fifo;
	int err;

	/* set processing function */
	dd->process = pcm_get_conversion_function(dd->local_buffer->frame_fmt,
						  dd->frame_fmt);

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_DEV;
	config->src_width = sample_bytes(dd->frame_fmt);
	config->dest_width = sample_bytes(dd->frame_fmt);
	config->cyclic = 1;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->dest_dev = dai_get_handshake(dd->dai, dev->direction,
					     dd->stream_id);
	config->is_scheduling_source = comp_is_scheduling_source(dev);
	config->period = dev->pipeline->ipc_pipe.period;

	trace_dai_with_ids(dev, "dai_playback_params() "
			   "dest_dev = %d stream_id = %d "
			   "src_width = %d dest_width = %d",
			   config->dest_dev, dd->stream_id,
			   config->src_width, config->dest_width);

	if (!config->elem_array.elems) {
		fifo = dai_get_fifo(dd->dai, dev->direction,
				    dd->stream_id);

		trace_dai_with_ids(dev, "dai_playback_params() "
				   "fifo %X", fifo);

		err = dma_sg_alloc(&config->elem_array, SOF_MEM_ZONE_RUNTIME,
				   config->direction,
				   period_count,
				   period_bytes,
				   (uintptr_t)(dd->dma_buffer->addr),
				   fifo);
		if (err < 0) {
			trace_dai_error_with_ids(dev, "dai_playback_params() "
						 "error: dma_sg_alloc() failed "
						 "with err = %d", err);
			return err;
		}
	}

	return 0;
}

static int dai_capture_params(struct comp_dev *dev, uint32_t period_bytes,
			      uint32_t period_count)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	uint32_t fifo;
	int err;

	/* set processing function */
	dd->process = pcm_get_conversion_function(dd->frame_fmt,
						  dd->local_buffer->frame_fmt);

	/* set up DMA configuration */
	config->direction = DMA_DIR_DEV_TO_MEM;
	config->cyclic = 1;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->src_dev = dai_get_handshake(dd->dai, dev->direction,
					    dd->stream_id);
	config->is_scheduling_source = comp_is_scheduling_source(dev);
	config->period = dev->pipeline->ipc_pipe.period;

	/* TODO: Make this code platform-specific or move it driver callback */
	if (dai_get_info(dd->dai, DAI_INFO_TYPE) == SOF_DAI_INTEL_DMIC) {
		/* For DMIC the DMA src and dest widths should always be 4 bytes
		 * due to 32 bit FIFO packer. Setting width to 2 bytes for
		 * 16 bit format would result in recording at double rate.
		 */
		config->src_width = 4;
		config->dest_width = 4;
	} else {
		config->src_width = sample_bytes(dd->frame_fmt);
		config->dest_width = sample_bytes(dd->frame_fmt);
	}

	trace_dai_with_ids(dev, "dai_capture_params() "
			   "src_dev = %d stream_id = %d "
			   "src_width = %d dest_width = %d",
			   config->src_dev, dd->stream_id,
			   config->src_width, config->dest_width);

	if (!config->elem_array.elems) {
		fifo = dai_get_fifo(dd->dai, dev->direction,
				    dd->stream_id);

		trace_dai_with_ids(dev, "dai_capture_params() "
				   "fifo %X", fifo);

		err = dma_sg_alloc(&config->elem_array, SOF_MEM_ZONE_RUNTIME,
				   config->direction,
				   period_count,
				   period_bytes,
				   (uintptr_t)(dd->dma_buffer->addr),
				   fifo);
		if (err < 0) {
			trace_dai_error_with_ids(dev, "dai_capture_params() "
						 "error: dma_sg_alloc() failed "
						 "with err = %d", err);
			return err;
		}
	}

	return 0;
}

static int dai_params(struct comp_dev *dev,
		      struct sof_ipc_stream_params *params)
{
	struct sof_ipc_comp_config *dconfig = COMP_GET_CONFIG(dev);
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t frame_size;
	uint32_t period_count;
	uint32_t period_bytes;
	uint32_t buffer_size;
	uint32_t addr_align;
	uint32_t align;
	int err;

	trace_dai_with_ids(dev, "dai_params()");

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		dd->local_buffer = list_first_item(&dev->bsource_list,
						   struct comp_buffer,
						   sink_list);
	else
		dd->local_buffer = list_first_item(&dev->bsink_list,
						   struct comp_buffer,
						   source_list);

	/* check if already configured */
	if (dev->state == COMP_STATE_PREPARE) {
		trace_dai_with_ids(dev, "dai_params() component has been "
				   "already configured.");
		return 0;
	}

	/* can set params on only init state */
	if (dev->state != COMP_STATE_READY) {
		trace_dai_error_with_ids(dev, "dai_params() error: Component"
					 " is not in init state.");
		return -EINVAL;
	}

	err = dma_get_attribute(dd->dma, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (err < 0) {
		trace_dai_error_with_ids(dev, "dai_params() error: could not "
					 "get dma buffer address alignment, "
					 "err = %d", err);
		return err;
	}

	err = dma_get_attribute(dd->dma, DMA_ATTR_BUFFER_ALIGNMENT, &align);
	if (err < 0 || !align) {
		trace_dai_error_with_ids(dev, "dai_params() error: could not "
				"get valid dma buffer alignment, err = %d, "
				"align = %u", err, align);
		return -EINVAL;
	}

	err = dma_get_attribute(dd->dma, DMA_ATTR_BUFFER_PERIOD_COUNT,
				&period_count);
	if (err < 0 || !period_count) {
		trace_dai_error_with_ids(dev, "dai_params() error: could not "
					  "get valid dma buffer period count, "
					  "err = %d, period_count = %u", err,
					  period_count);
		return -EINVAL;
	}

	dd->frame_fmt = dconfig->frame_fmt;

	/* calculate frame size */
	frame_size = frame_bytes(dd->frame_fmt, dd->local_buffer->channels);

	/* calculate period size */
	period_bytes = dev->frames * frame_size;
	if (!period_bytes) {
		trace_dai_error_with_ids(dev, "dai_params() error: invalid "
					 "period_bytes.");
		return -EINVAL;
	}

	/* calculate DMA buffer size */
	buffer_size = ALIGN_UP(period_count * period_bytes, align);

	/* alloc DMA buffer or change its size if exists */
	if (dd->dma_buffer) {
		err = buffer_set_size(dd->dma_buffer, buffer_size);
		if (err < 0) {
			trace_dai_error_with_ids(dev, "dai_params() error: "
						 "buffer_set_size() failed, "
						 "buffer_size = %u",
						 buffer_size);
			return err;
		}
	} else {
		dd->dma_buffer = buffer_alloc(buffer_size, SOF_MEM_CAPS_DMA,
					      addr_align);
		if (!dd->dma_buffer) {
			trace_dai_error_with_ids(dev, "dai_params() error: "
						 "failed to alloc dma buffer");
			return -ENOMEM;
		}
	}

	return dev->direction == SOF_IPC_STREAM_PLAYBACK ?
		dai_playback_params(dev, period_bytes, period_count) :
		dai_capture_params(dev, period_bytes, period_count);
}

static int dai_prepare(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret = 0;

	trace_dai_with_ids(dev, "dai_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	dev->position = 0;

	if (!dd->chan) {
		trace_dai_error_with_ids(dev, "dai_prepare() error: Missing "
					 "dd->chan.");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	if (!dd->config.elem_array.elems) {
		trace_dai_error_with_ids(dev, "dai_prepare() error: Missing "
					 "dd->config.elem_array.elems.");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	/* TODO: not sure what this wb is for? */
	/* write back buffer contents from cache */
	dcache_writeback_region(dd->dma_buffer->addr, dd->dma_buffer->size);

	/* dma reconfig not required if XRUN handling */
	if (dd->xrun) {
		/* after prepare, we have recovered from xrun */
		dd->xrun = 0;
		return ret;
	}

	ret = dma_set_config(dd->chan, &dd->config);
	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

static int dai_reset(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;

	trace_dai_with_ids(dev, "dai_reset()");

	dma_sg_free(&config->elem_array);

	if (dd->dma_buffer) {
		buffer_free(dd->dma_buffer);
		dd->dma_buffer = NULL;
	}

	dd->dai_pos_blks = 0;
	if (dd->dai_pos)
		*dd->dai_pos = 0;
	dd->dai_pos = NULL;
	dd->wallclock = 0;
	dev->position = 0;
	dd->xrun = 0;
	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

static void dai_update_start_position(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	/* update starting wallclock */
	platform_dai_wallclock(dev, &dd->wallclock);

	/* update start position */
	dd->start_position = dev->position;
}

/* used to pass standard and bespoke command (with data) to component */
static int dai_comp_trigger(struct comp_dev *dev, int cmd)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret;

	trace_dai_with_ids(dev, "dai_comp_trigger(), command = %u", cmd);

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* setup callback */
	notifier_unregister(dev, dd->chan, NOTIFIER_ID_DMA_COPY);
	notifier_register(dev, dd->chan, NOTIFIER_ID_DMA_COPY,
			  dai_dma_cb);

	switch (cmd) {
	case COMP_TRIGGER_START:
		trace_dai_with_ids(dev, "dai_comp_trigger(), START");

		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun == 0) {
			/* start the DAI */
			dai_trigger(dd->dai, cmd, dev->direction);
			ret = dma_start(dd->chan);
			if (ret < 0)
				return ret;
		} else {
			dd->xrun = 0;
		}

		dai_update_start_position(dev);
		break;
	case COMP_TRIGGER_RELEASE:
		/* before release, we clear the buffer data to 0s,
		 * then there is no history data sent out after release.
		 * this is only supported at capture mode.
		 */
		if (dev->direction == SOF_IPC_STREAM_CAPTURE)
			buffer_zero(dd->dma_buffer);

		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun == 0) {
			/* recover valid start position */
			ret = dma_release(dd->chan);
			if (ret < 0)
				return ret;

			/* start the DAI */
			dai_trigger(dd->dai, cmd, dev->direction);
			ret = dma_start(dd->chan);
			if (ret < 0)
				return ret;
		} else {
			dd->xrun = 0;
		}

		dai_update_start_position(dev);
		break;
	case COMP_TRIGGER_XRUN:
		trace_dai_with_ids(dev, "dai_comp_trigger(), XRUN");
		dd->xrun = 1;

		/* fallthrough */
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
		trace_dai_with_ids(dev, "dai_comp_trigger(), PAUSE/STOP");
		ret = dma_stop(dd->chan);
		dai_trigger(dd->dai, COMP_TRIGGER_STOP, dev->direction);
		break;
	default:
		break;
	}

	return ret;
}

/* report xrun occurrence */
static void dai_report_xrun(struct comp_dev *dev, uint32_t bytes)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		trace_dai_error_with_ids(dev, "dai_report_xrun() error: "
					 "underrun due to no data available");
		comp_underrun(dev, dd->local_buffer, bytes);
	} else {
		trace_dai_error_with_ids(dev, "dai_report_xrun() error: "
					 "overrun due to no data available");
		comp_overrun(dev, dd->local_buffer, bytes);
	}
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t avail_bytes = 0;
	uint32_t free_bytes = 0;
	uint32_t copy_bytes = 0;
	uint32_t src_samples;
	uint32_t sink_samples;
	int ret = 0;

	tracev_dai_with_ids(dev, "dai_copy()");

	/* get data sizes from DMA */
	ret = dma_get_data_size(dd->chan, &avail_bytes, &free_bytes);
	if (ret < 0) {
		dai_report_xrun(dev, 0);
		return ret;
	}

	/* calculate minimum size to copy */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK) {
		src_samples = dd->local_buffer->avail /
			buffer_sample_bytes(dd->local_buffer);
		sink_samples = free_bytes / sample_bytes(dd->frame_fmt);
		copy_bytes = MIN(src_samples, sink_samples) *
			sample_bytes(dd->frame_fmt);
	} else {
		src_samples = avail_bytes / sample_bytes(dd->frame_fmt);
		sink_samples = dd->local_buffer->free /
			buffer_sample_bytes(dd->local_buffer);
		copy_bytes = MIN(src_samples, sink_samples) *
			sample_bytes(dd->frame_fmt);
	}

	tracev_dai_with_ids(dev, "dai_copy(), copy_bytes = 0x%x", copy_bytes);

	/* return if it's not stream start */
	if (!copy_bytes && dd->start_position != dev->position)
		return 0;

	ret = dma_copy(dd->chan, copy_bytes, 0);
	if (ret < 0) {
		dai_report_xrun(dev, copy_bytes);
		return ret;
	}

	return ret;
}

static int dai_position(struct comp_dev *dev, struct sof_ipc_stream_posn *posn)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	/* TODO: improve accuracy by adding current DMA position */
	posn->dai_posn = dev->position;

	/* set stream start wallclock */
	posn->wallclock = dd->wallclock;

	return 0;
}

static int dai_config(struct comp_dev *dev, struct sof_ipc_dai_config *config)
{
	struct sof_ipc_comp_config *dconfig = COMP_GET_CONFIG(dev);
	struct dai_data *dd = comp_get_drvdata(dev);
	struct sof_ipc_comp_dai *dai = (struct sof_ipc_comp_dai *)&dev->comp;
	int channel = 0;
	int handshake;

	trace_dai_with_ids(dev, "config comp %d pipe %d dai %d type %d",
			   dev->comp.id, dev->comp.pipeline_id,
			   config->dai_index, config->type);

	/* cannot configure DAI while active */
	if (dev->state == COMP_STATE_ACTIVE) {
		trace_dai_error_with_ids(dev, "dai_config() error: Component "
					 "is in active state.");
		return -EINVAL;
	}

	switch (config->type) {
	case SOF_DAI_INTEL_SSP:
		/* set dma burst elems to slot number */
		dd->config.burst_elems = config->ssp.tdm_slots;
		break;
	case SOF_DAI_INTEL_DMIC:
		trace_dai_with_ids(dev, "dai_config(), config->type = "
				   "SOF_DAI_INTEL_DMIC");

		/* We can use always the largest burst length. */
		dd->config.burst_elems = 8;

		trace_dai_with_ids(dev, "dai_config(), "
				   "config->dmic.fifo_bits = %u; "
				   "config->dmic.num_pdm_active = %u;",
				   config->dmic.fifo_bits,
				   config->dmic.num_pdm_active);
		break;
	case SOF_DAI_INTEL_HDA:
		channel = config->hda.link_dma_ch;
		trace_dai_with_ids(dev, "dai_config(), channel = %d",
				   channel);

		/*
		 * For HDA DAIs, the driver sends the DAI_CONFIG IPC
		 * during every link hw_params and hw_free, apart from the
		 * the first DAI_CONFIG IPC sent during topology parsing.
		 * Free the channel that is currently in use before
		 * assigning the new one.
		 */
		if (dd->chan) {
			dma_channel_put(dd->chan);
			dd->chan = NULL;
		}
		break;
	case SOF_DAI_INTEL_ALH:
		/* SDW HW FIFO always requires 32bit MSB aligned sample data for
		 * all formats, such as 8/16/24/32 bits.
		 */
		dconfig->frame_fmt = SOF_IPC_FRAME_S32_LE;

		dd->config.burst_elems =
			dd->dai->plat_data.fifo[dai->direction].depth;

		/* As with HDA, the DMA channel is assigned in runtime,
		 * not during topology parsing.
		 */
		channel = config->alh.stream_id;
		dd->stream_id = config->alh.stream_id;
		trace_dai_with_ids(dev, "dai_config(), channel = %d",
				   channel);
		break;
	case SOF_DAI_IMX_SAI:
		handshake = dai_get_handshake(dd->dai, dai->direction,
					      dd->stream_id);
		channel = EDMA_HS_GET_CHAN(handshake);

		dd->config.burst_elems =
			dd->dai->plat_data.fifo[dai->direction].depth;
		break;
	case SOF_DAI_IMX_ESAI:
		handshake = dai_get_handshake(dd->dai, dai->direction,
					      dd->stream_id);
		channel = EDMA_HS_GET_CHAN(handshake);

		dd->config.burst_elems =
			dd->dai->plat_data.fifo[dai->direction].depth;
		break;
	default:
		/* other types of DAIs not handled for now */
		trace_dai_error_with_ids(dev, "dai_config() error: Handling of "
					 "DAIs other than SOF_DAI_INTEL_SSP, "
					 "SOF_DAI_INTEL_ALH, "
					 "SOF_DAI_INTEL_DMIC or "
					 "SOF_DAI_INTEL_HDA is not handled for "
					 "now.");
		break;
	}

	platform_shared_commit(dd->dai, sizeof(*dd->dai));

	if (channel != DMA_CHAN_INVALID) {
		if (!dd->chan)
			/* get dma channel at first config only */
			dd->chan = dma_channel_get(dd->dma, channel);

		if (!dd->chan) {
			trace_dai_error_with_ids(dev, "dai_config() error: "
						 "dma_channel_get() failed");
			dd->chan = NULL;
			return -EIO;
		}
	}

	return dai_set_config(dd->dai, config);
}

static void dai_cache(struct comp_dev *dev, int cmd)
{
	struct dai_data *dd;

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		trace_dai_with_ids(dev, "dai_cache(), CACHE_WRITEBACK_INV");

		dd = comp_get_drvdata(dev);

		dma_sg_cache_wb_inv(&dd->config.elem_array);

		dcache_writeback_invalidate_region(dd->dma_buffer,
						   sizeof(*dd->dma_buffer));
		dcache_writeback_invalidate_region(dd, sizeof(*dd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_dai_with_ids(dev, "dai_cache(), CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		dd = comp_get_drvdata(dev);
		dcache_invalidate_region(dd, sizeof(*dd));
		dcache_invalidate_region(dd->dma_buffer,
					 sizeof(*dd->dma_buffer));

		dma_sg_cache_inv(&dd->config.elem_array);
		break;
	}
}

static const struct comp_driver comp_dai = {
	.type	= SOF_COMP_DAI,
	.ops	= {
		.new		= dai_new,
		.free		= dai_free,
		.params		= dai_params,
		.trigger	= dai_comp_trigger,
		.copy		= dai_copy,
		.prepare	= dai_prepare,
		.reset		= dai_reset,
		.dai_config	= dai_config,
		.position	= dai_position,
		.cache		= dai_cache,
	},
};

static SHARED_DATA struct comp_driver_info comp_dai_info = {
	.drv = &comp_dai,
};

static void sys_comp_dai_init(void)
{
	comp_register(platform_shared_get(&comp_dai_info,
					  sizeof(comp_dai_info)));
}

DECLARE_MODULE(sys_comp_dai_init);
