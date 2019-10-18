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
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
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
#define trace_dai(format, ...) trace_event(TRACE_CLASS_DAI, format,	\
					   ##__VA_ARGS__)
#define trace_dai_with_ids(comp_ptr, format, ...)	\
	trace_event_with_ids(TRACE_CLASS_DAI,		\
			     comp_ptr->comp.pipeline_id,\
			     comp_ptr->comp.id,		\
			     format, ##__VA_ARGS__)

#define trace_dai_error(format, ...) trace_error(TRACE_CLASS_DAI, format, \
						 ##__VA_ARGS__)

#define trace_dai_error_with_ids(comp_ptr, format, ...)	\
	trace_error_with_ids(TRACE_CLASS_DAI,		\
			     comp_ptr->comp.pipeline_id,\
			     comp_ptr->comp.id,		\
			     format, ##__VA_ARGS__)

#define tracev_dai(format, ...) tracev_event(TRACE_CLASS_DAI, format,	\
					     ##__VA_ARGS__)
#define tracev_dai_with_ids(comp_ptr, format, ...)	\
	tracev_event_with_ids(TRACE_CLASS_DAI,		\
			     comp_ptr->comp.pipeline_id,\
			     comp_ptr->comp.id,		\
			     format, ##__VA_ARGS__)

struct dai_data {
	/* local DMA config */
	struct dma_chan_data *chan;
	uint32_t stream_id;
	struct dma_sg_config config;
	struct comp_buffer *dma_buffer;

	struct dai *dai;
	struct dma *dma;
	uint32_t frame_bytes;
	int xrun;		/* true if we are doing xrun recovery */

	/* processing function */
	void (*process)(struct comp_buffer *source, struct comp_buffer *sink,
			uint32_t bytes);

	uint32_t dai_pos_blks;	/* position in bytes (nearest block) */
	uint64_t start_position;	/* position on start */

	/* host can read back this value without IPC */
	uint64_t *dai_pos;

	uint64_t wallclock;	/* wall clock at stream start */
};

/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_cb(void *data, uint32_t type, struct dma_cb_data *next)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t bytes = next->elem.size;
	struct comp_buffer *local_buffer;
	void *buffer_ptr;

	tracev_dai_with_ids(dev, "dai_dma_cb()");

	next->status = DMA_CB_STATUS_RELOAD;

	/* stop dma copy for pause/stop/xrun */
	if (dev->state != COMP_STATE_ACTIVE || dd->xrun) {
		/* stop the DAI */
		dai_trigger(dd->dai, COMP_TRIGGER_STOP, dev->params.direction);

		/* tell DMA not to reload */
		next->status = DMA_CB_STATUS_END;
	}

	/* is our pipeline handling an XRUN ? */
	if (dd->xrun) {
		/* make sure we only playback silence during an XRUN */
		if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK)
			/* fill buffer with silence */
			buffer_zero(dd->dma_buffer);

		return;
	}

	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		local_buffer = list_first_item(&dev->bsource_list,
					       struct comp_buffer, sink_list);

		dma_buffer_copy_to(local_buffer, dd->dma_buffer, dd->process,
				   bytes);

		buffer_ptr = local_buffer->r_ptr;
	} else {
		local_buffer = list_first_item(&dev->bsink_list,
					       struct comp_buffer, source_list);

		dma_buffer_copy_from(dd->dma_buffer, local_buffer, dd->process,
				     bytes);

		buffer_ptr = local_buffer->w_ptr;
	}

	/* update host position (in bytes offset) for drivers */
	dev->position += bytes;
	if (dd->dai_pos) {
		dd->dai_pos_blks += bytes;
		*dd->dai_pos = dd->dai_pos_blks +
			buffer_ptr - dd->dma_buffer->addr;
	}
}

static struct comp_dev *dai_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_dai *dai;
	struct sof_ipc_comp_dai *ipc_dai = (struct sof_ipc_comp_dai *)comp;
	struct dai_data *dd;
	uint32_t dir, caps, dma_dev;

	trace_dai("dai_new()");

	if (IPC_IS_SIZE_INVALID(ipc_dai->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_DAI, ipc_dai->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		COMP_SIZE(struct sof_ipc_comp_dai));
	if (!dev)
		return NULL;

	dai = (struct sof_ipc_comp_dai *)&dev->comp;
	assert(!memcpy_s(dai, sizeof(*dai), ipc_dai,
	   sizeof(struct sof_ipc_comp_dai)));

	dd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*dd));
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

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_DEV;
	config->src_width = comp_sample_bytes(dev);
	config->dest_width = comp_sample_bytes(dev);
	config->cyclic = 1;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->dest_dev = dai_get_handshake(dd->dai, dev->params.direction,
					     dd->stream_id);
	config->is_scheduling_source = comp_is_scheduling_source(dev);
	config->period = dev->pipeline->ipc_pipe.period;

	trace_dai_with_ids(dev, "dai_playback_params() "
			   "dest_dev = %d stream_id = %d "
			   "src_width = %d dest_width = %d",
			   config->dest_dev, dd->stream_id,
			   config->src_width, config->dest_width);

	if (!config->elem_array.elems) {
		fifo = dai_get_fifo(dd->dai, dev->params.direction,
				    dd->stream_id);

		trace_dai_with_ids(dev, "dai_playback_params() "
				   "fifo %X", fifo);

		err = dma_sg_alloc(&config->elem_array, RZONE_RUNTIME,
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

	/* set up DMA configuration */
	config->direction = DMA_DIR_DEV_TO_MEM;
	config->cyclic = 1;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->src_dev = dai_get_handshake(dd->dai, dev->params.direction,
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
		config->src_width = comp_sample_bytes(dev);
		config->dest_width = comp_sample_bytes(dev);
	}

	trace_dai_with_ids(dev, "dai_capture_params() "
			   "src_dev = %d stream_id = %d "
			   "src_width = %d dest_width = %d",
			   config->src_dev, dd->stream_id,
			   config->src_width, config->dest_width);

	if (!config->elem_array.elems) {
		fifo = dai_get_fifo(dd->dai, dev->params.direction,
				    dd->stream_id);

		trace_dai_with_ids(dev, "dai_capture_params() "
				   "fifo %X", fifo);

		err = dma_sg_alloc(&config->elem_array, RZONE_RUNTIME,
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

static int dai_params(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct sof_ipc_comp_config *dconfig = COMP_GET_CONFIG(dev);
	uint32_t period_count;
	uint32_t period_bytes;
	uint32_t buffer_size;
	uint32_t addr_align;
	uint32_t align;
	int err;

	trace_dai_with_ids(dev, "dai_params()");

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

	/* for DAI, we should configure its frame_fmt from topology */
	dev->params.frame_fmt = dconfig->frame_fmt;

	/* set processing function */
	if (dev->params.frame_fmt == SOF_IPC_FRAME_S16_LE)
		dd->process = buffer_copy_s16;
	else
		dd->process = buffer_copy_s32;

	/* calculate period size based on config */
	dd->frame_bytes = comp_frame_bytes(dev);
	if (!dd->frame_bytes) {
		trace_dai_error_with_ids(dev, "dai_params() error: "
					 "comp_frame_bytes() returned 0.");
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

	period_bytes = dev->frames * dd->frame_bytes;
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

	return dev->params.direction == SOF_IPC_STREAM_PLAYBACK ?
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

	switch (cmd) {
	case COMP_TRIGGER_START:
		trace_dai_with_ids(dev, "dai_comp_trigger(), START");

		/* only start the DAI if we are not XRUN handling
		 * and the pipeline is not preloaded as in this
		 * case start is deferred to the first copy call
		 */
		if (dd->xrun == 0 && !pipeline_is_preload(dev->pipeline)) {
			/* start the DAI */
			dai_trigger(dd->dai, cmd, dev->params.direction);
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
		if (dev->params.direction == SOF_IPC_STREAM_CAPTURE)
			buffer_zero(dd->dma_buffer);

		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun == 0) {
			/* recover valid start position */
			ret = dma_release(dd->chan);
			if (ret < 0)
				return ret;

			/* start the DAI */
			dai_trigger(dd->dai, cmd, dev->params.direction);
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
		dai_trigger(dd->dai, COMP_TRIGGER_STOP, dev->params.direction);
		break;
	default:
		break;
	}

	return ret;
}

/* check if xrun occurred */
static int dai_check_for_xrun(struct comp_dev *dev, uint32_t copy_bytes)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *local_buffer;

	/* data available for copy */
	if (copy_bytes)
		return 0;

	/* no data yet, we're just starting */
	if (dd->start_position == dev->position)
		return PPL_STATUS_PATH_STOP;

	/* xrun occurred */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		trace_dai_error_with_ids(dev, "dai_check_for_xrun() "
					 "error: underrun due to no data "
					 "available");
		local_buffer = list_first_item(&dev->bsource_list,
					       struct comp_buffer, sink_list);
		comp_underrun(dev, local_buffer, copy_bytes);
	} else {
		trace_dai_error_with_ids(dev, "dai_check_for_xrun() "
					 "error: overrun due to no data "
					 "available");
		local_buffer = list_first_item(&dev->bsink_list,
					       struct comp_buffer, source_list);
		comp_overrun(dev, local_buffer, copy_bytes);
	}

	return -ENODATA;
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *local_buffer;
	uint32_t avail_bytes = 0;
	uint32_t free_bytes = 0;
	uint32_t copy_bytes = 0;
	int ret = 0;

	tracev_dai_with_ids(dev, "dai_copy()");

	/* start DMA on preload */
	if (pipeline_is_preload(dev->pipeline)) {
		/* start the DAI */
		dai_trigger(dd->dai, COMP_TRIGGER_START,
			    dev->params.direction);
		ret = dma_start(dd->chan);
		if (ret < 0)
			return ret;
		dai_update_start_position(dev);

		return 0;
	}

	/* get data sizes from DMA */
	ret = dma_get_data_size(dd->chan, &avail_bytes, &free_bytes);
	if (ret < 0)
		return ret;

	/* calculate minimum size to copy */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		local_buffer = list_first_item(&dev->bsource_list,
					       struct comp_buffer, sink_list);
		copy_bytes = MIN(local_buffer->avail, free_bytes);
	} else {
		local_buffer = list_first_item(&dev->bsink_list,
					       struct comp_buffer, source_list);
		copy_bytes = MIN(avail_bytes, local_buffer->free);
	}

	tracev_dai_with_ids(dev, "dai_copy(), copy_bytes = 0x%x", copy_bytes);

	/* check for underrun or overrun */
	ret = dai_check_for_xrun(dev, copy_bytes);
	if (ret < 0 || ret == PPL_STATUS_PATH_STOP)
		return ret;

	ret = dma_copy(dd->chan, copy_bytes, 0);
	if (ret < 0)
		trace_dai_error("dai_copy() error: ret = %u", ret);

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
	int channel = 0;
	int i;

	trace_dai("config comp %d pipe %d dai %d type %d", dev->comp.id,
		  dev->comp.pipeline_id, config->dai_index, config->type);

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

		/* calc frame bytes */
		switch (config->ssp.sample_valid_bits) {
		case 16:
			dd->frame_bytes = 2 * config->ssp.tdm_slots;
			break;
		case 17 ... 32:
			dd->frame_bytes = 4 * config->ssp.tdm_slots;
			break;
		default:
			break;
		}
		break;
	case SOF_DAI_INTEL_DMIC:
		/* The frame bytes setting follows only FIFO A setting in
		 * this DMIC driver version.
		 */
		trace_dai_with_ids(dev, "dai_config(), config->type = "
				   "SOF_DAI_INTEL_DMIC");

		/* We can use always the largest burst length. */
		dd->config.burst_elems = 8;

		/* Set frame size in bytes to match the configuration. The
		 * actual width of FIFO appears in IPC always in fifo_bits_a
		 * for both FIFOs A and B.
		 */
		trace_dai_with_ids(dev, "dai_config(), "
				   "config->dmic.fifo_bits = %u; "
				   "config->dmic.num_pdm_active = %u;",
				   config->dmic.fifo_bits,
				   config->dmic.num_pdm_active);
		dd->frame_bytes = 0;
		for (i = 0; i < config->dmic.num_pdm_active; i++) {
			trace_dai_with_ids(dev, "dai_config, "
				"config->dmic.pdm[%u].enable_mic_a = %u; ",
				config->dmic.pdm[i].id,
				config->dmic.pdm[i].enable_mic_a);
			trace_dai_with_ids(dev, "dai_config, "
				"config->dmic.pdm[%u].enable_mic_b = %u; ",
				config->dmic.pdm[i].id,
				config->dmic.pdm[i].enable_mic_b);
			dd->frame_bytes += (config->dmic.fifo_bits >> 3) *
				(config->dmic.pdm[i].enable_mic_a +
				 config->dmic.pdm[i].enable_mic_b);
		}

		/* Packing of mono streams from several PDM controllers is not
		 * supported. In such cases the stream needs to be two
		 * channels.
		 */
		if (config->dmic.num_pdm_active > 1) {
			dd->frame_bytes = 2 * config->dmic.num_pdm_active *
				(config->dmic.fifo_bits >> 3);
		}

		trace_dai_with_ids(dev, "dai_config(), dd->frame_bytes = %u",
				   dd->frame_bytes);
		break;
	case SOF_DAI_INTEL_HDA:
		/* set to some non-zero value to satisfy the condition below,
		 * it is recalculated in dai_params() later
		 * this is temp until dai/hda model is changed.
		 */
		dd->frame_bytes = 4;
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
		/* set to some non-zero value to satisfy the condition below,
		 * it is recalculated in dai_params() later
		 */
		dd->frame_bytes = 4;

		/* SDW HW FIFO always requires 32bit MSB aligned sample data for
		 * all formats, such as 8/16/24/32 bits.
		 */
		dconfig->frame_fmt = SOF_IPC_FRAME_S32_LE;

		/* As with HDA, the DMA channel is assigned in runtime,
		 * not during topology parsing.
		 */
		channel = config->alh.stream_id;
		dd->stream_id = config->alh.stream_id;
		trace_dai_with_ids(dev, "dai_config(), channel = %d",
				   channel);
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

	if (!dd->frame_bytes) {
		trace_dai_error_with_ids(dev, "dai_config() error: "
					 "dd->frame_bytes == 0");
		return -EINVAL;
	}

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

		/* set up callback */
		dma_set_cb(dd->chan, DMA_CB_TYPE_COPY, dai_dma_cb, dev);
	}

	return 0;
}

static void dai_cache(struct comp_dev *dev, int cmd)
{
	struct dai_data *dd;

	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		trace_dai_with_ids(dev, "dai_cache(), CACHE_WRITEBACK_INV");

		dd = comp_get_drvdata(dev);

		dma_sg_cache_wb_inv(&dd->config.elem_array);

		dcache_writeback_invalidate_region(dd->dai, sizeof(*dd->dai));
		dcache_writeback_invalidate_region(dd->dma, sizeof(*dd->dma));
		dcache_writeback_invalidate_region(dd, sizeof(*dd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case CACHE_INVALIDATE:
		trace_dai_with_ids(dev, "dai_cache(), CACHE_INVALIDATE");

		dcache_invalidate_region(dev, sizeof(*dev));

		dd = comp_get_drvdata(dev);
		dcache_invalidate_region(dd, sizeof(*dd));
		dcache_invalidate_region(dd->dma, sizeof(*dd->dma));
		dcache_invalidate_region(dd->dai, sizeof(*dd->dai));

		dma_sg_cache_inv(&dd->config.elem_array);
		break;
	}
}

static struct comp_driver comp_dai = {
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

static void sys_comp_dai_init(void)
{
	comp_register(&comp_dai);
}

DECLARE_MODULE(sys_comp_dai_init);
