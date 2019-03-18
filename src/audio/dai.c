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
#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/dai.h>
#include <sof/alloc.h>
#include <sof/dma.h>
#include <sof/wait.h>
#include <sof/stream.h>
#include <sof/ipc.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <platform/dma.h>
#include <arch/cache.h>

#include <sof/drivers/timer.h>

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
	int chan;
	struct dma_sg_config config;
	struct comp_buffer *dma_buffer;

	struct dai *dai;
	struct dma *dma;
	uint32_t period_bytes;
	int xrun;		/* true if we are doing xrun recovery */
	uint32_t cb_type;	/* last callback type */

	uint32_t dai_pos_blks;	/* position in bytes (nearest block) */

	volatile uint64_t *dai_pos; /* host can read back this value without IPC */
	uint64_t wallclock;	/* wall clock at stream start */
};

static void dai_buffer_process(struct comp_dev *dev, struct dma_sg_elem *next)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t bytes = next->size;
	void *buffer_ptr;

	tracev_dai_with_ids(dev, "dai_buffer_process()");

	/* reload lli if the last callback wasn't of llist type */
	next->size = dd->cb_type == DMA_CB_TYPE_LLIST ? DMA_RELOAD_IGNORE :
		DMA_RELOAD_LLI;

	/* stop dma copy for pause/stop/xrun */
	if (dev->state != COMP_STATE_ACTIVE || dd->xrun) {
		/* stop the DAI */
		dai_trigger(dd->dai, COMP_TRIGGER_STOP, dev->params.direction);

		/* tell DMA not to reload */
		next->size = DMA_RELOAD_END;
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
		/* make sure there are available bytes for next period */
		if (dd->dma_buffer->avail < bytes) {
			trace_dai_error_with_ids(dev, "dai_buffer_process() "
						 "error: Insufficient bytes for"
						 " next period. "
						 "comp_underrun()");
			comp_underrun(dev, dd->dma_buffer, bytes, 0);
		}

		/* recalc available buffer space */
		comp_update_buffer_consume(dd->dma_buffer, bytes);

		buffer_ptr = dd->dma_buffer->r_ptr;
	} else {
		/* make sure there are free bytes for next period */
		if (dd->dma_buffer->free < bytes) {
			trace_dai_error_with_ids(dev, "dai_buffer_process() "
						 "error: Insufficient free "
						 "bytes for next period. "
						 "comp_overrun()");
			comp_overrun(dev, dd->dma_buffer, bytes, 0);
		}

		/* recalc available buffer space */
		comp_update_buffer_produce(dd->dma_buffer, bytes);

		buffer_ptr = dd->dma_buffer->w_ptr;
	}

	/* update host position (in bytes offset) for drivers */
	dev->position += bytes;
	if (dd->dai_pos) {
		dd->dai_pos_blks += bytes;
		*dd->dai_pos = dd->dai_pos_blks +
			buffer_ptr - dd->dma_buffer->addr;
	}
}

/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_cb(void *data, uint32_t type, struct dma_sg_elem *next)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct dai_data *dd = comp_get_drvdata(dev);

	tracev_dai_with_ids(dev, "dai_dma_cb()");

	switch (type) {
	case DMA_CB_TYPE_BLOCK:
		dd->cb_type = type;
		next->size = DMA_RELOAD_IGNORE;
		pipeline_schedule_copy(dev->pipeline, 0);
		break;
	case DMA_CB_TYPE_LLIST:
		dd->cb_type = type;
		next->size = DMA_RELOAD_LLI;
		pipeline_schedule_copy(dev->pipeline, 0);
		break;
	case DMA_CB_TYPE_PROCESS:
		dai_buffer_process(dev, next);
		break;
	default:
		trace_dai_error_with_ids(dev, "dai_dma_cb() error: Wrong "
					 "callback type = %u", type);
		break;
	}
}

static struct comp_dev *dai_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_dai *dai;
	struct sof_ipc_comp_dai *ipc_dai = (struct sof_ipc_comp_dai *)comp;
	struct dai_data *dd;
	uint32_t dir, caps, dma_dev;
	int err;

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
	err = memcpy_s(dai, sizeof(*dai), ipc_dai,
	   sizeof(struct sof_ipc_comp_dai));
	if (err) {
		trace_dai_error("dai_new() error: 0x%x "
				"could not coppy data",
				err);
		rfree(dev);
		return NULL;
	}

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
	/* TODO: hda: retrieve req'ed caps from the dai,
	 * dmas are not cross-compatible.
	 */
	switch (dai->type) {
	case SOF_DAI_INTEL_HDA:
		dir = dai->direction == SOF_IPC_STREAM_PLAYBACK ?
				DMA_DIR_MEM_TO_DEV : DMA_DIR_DEV_TO_MEM;
		caps = DMA_CAP_HDA;
		dma_dev = DMA_DEV_HDA;
		break;
	case SOF_DAI_INTEL_SSP:
	case SOF_DAI_INTEL_DMIC:
	default:
		dir = DMA_DIR_MEM_TO_DEV | DMA_DIR_DEV_TO_MEM;
		caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP;
		dma_dev = DMA_DEV_SSP | DMA_DEV_DMIC;
		break;
	}
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
	dd->cb_type = 0;
	dd->chan = DMA_CHAN_INVALID;

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

	dma_channel_put(dd->dma, dd->chan);
	dma_put(dd->dma);

	dai_put(dd->dai);

	rfree(dd);
	rfree(dev);
}

/* set component audio SSP and DMA configuration */
static int dai_playback_params(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	struct sof_ipc_comp_config *source_config;
	int err;
	uint32_t buffer_size;

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_DEV;
	config->src_width = comp_sample_bytes(dev);
	config->dest_width = comp_sample_bytes(dev);
	config->cyclic = 1;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->dest_dev = dd->dai->plat_data.fifo[0].handshake;

	/* set up local and host DMA elems to reset values */
	source_config = COMP_GET_CONFIG(dd->dma_buffer->source);
	buffer_size = source_config->periods_sink * dd->period_bytes;

	/* resize the buffer if space is available to align with period size */
	err = buffer_set_size(dd->dma_buffer, buffer_size);
	if (err < 0) {
		trace_dai_error_with_ids(dev, "dai_playback_params() error: "
					 "buffer_set_size() failed to resize "
					 "buffer. source_config->periods_sink ="
					 " %u; dd->period_bytes = %u; "
					 "buffer_size = %u; "
					 "dd->dma_buffer->alloc_size = %u",
					 source_config->periods_sink,
					 dd->period_bytes, buffer_size,
					 dd->dma_buffer->alloc_size);
		return err;
	}

	if (!config->elem_array.elems) {
		err = dma_sg_alloc(&config->elem_array, RZONE_RUNTIME,
				   config->direction,
				   source_config->periods_sink,
				   dd->period_bytes,
				   (uintptr_t)(dd->dma_buffer->r_ptr),
				   dai_fifo(dd->dai, SOF_IPC_STREAM_PLAYBACK));
		if (err < 0) {
			trace_dai_error_with_ids(dev, "dai_playback_params() "
						 "error: dma_sg_alloc() failed "
						 "with err = %d", err);
			return err;
		}
	}

	return 0;
}

static int dai_capture_params(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	struct sof_ipc_comp_config *sink_config;
	int err;
	uint32_t buffer_size;

	/* set up DMA configuration */
	config->direction = DMA_DIR_DEV_TO_MEM;
	config->cyclic = 1;
	config->irq_disabled = pipeline_is_timer_driven(dev->pipeline);
	config->src_dev = dd->dai->plat_data.fifo[1].handshake;

	/* TODO: Make this code platform-specific or move it driver callback */
	if (dd->dai->type == SOF_DAI_INTEL_DMIC) {
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

	/* set up local and host DMA elems to reset values */
	sink_config = COMP_GET_CONFIG(dd->dma_buffer->sink);
	buffer_size = sink_config->periods_source * dd->period_bytes;

	/* resize the buffer if space is available to align with period size */
	err = buffer_set_size(dd->dma_buffer, buffer_size);
	if (err < 0) {
		trace_dai_error_with_ids(dev, "dai_capture_params() error: "
					 "buffer_set_size() failed to resize "
					 "buffer. sink_config->periods_sink = "
					 "%u; dd->period_bytes = %u; "
					 "buffer_size = %u; "
					 "dd->dma_buffer->alloc_size = %u",
					 sink_config->periods_sink,
					 dd->period_bytes, buffer_size,
					 dd->dma_buffer->alloc_size);
		return err;
	}

	if (!config->elem_array.elems) {
		err = dma_sg_alloc(&config->elem_array, RZONE_RUNTIME,
				   config->direction,
				   sink_config->periods_source,
				   dd->period_bytes,
				   (uintptr_t)(dd->dma_buffer->w_ptr),
				   dai_fifo(dd->dai, SOF_IPC_STREAM_CAPTURE));
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

	trace_dai_with_ids(dev, "dai_params()");

	/* check if already configured */
	if (dev->state == COMP_STATE_PREPARE) {
		trace_dai_with_ids(dev, "dai_params() component has been"
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

	/* calculate period size based on config */
	dev->frame_bytes = comp_frame_bytes(dev);
	if (dev->frame_bytes == 0) {
		trace_dai_error_with_ids(dev, "dai_params() error: "
					 "comp_frame_bytes() returned 0.");
		return -EINVAL;
	}

	dd->period_bytes = dev->frames * dev->frame_bytes;
	if (dd->period_bytes == 0) {
		trace_dai_error_with_ids(dev, "dai_params() error: device has "
					 "no bytes (no frames to copy to sink).");
		return -EINVAL;
	}

	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		dd->dma_buffer = list_first_item(&dev->bsource_list,
						 struct comp_buffer,
						 sink_list);
		dd->dma_buffer->r_ptr = dd->dma_buffer->addr;

		return dai_playback_params(dev);
	}

	dd->dma_buffer = list_first_item(&dev->bsink_list,
					 struct comp_buffer, source_list);
	dd->dma_buffer->w_ptr = dd->dma_buffer->addr;

	return dai_capture_params(dev);

}

static int dai_prepare(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	int ret = 0;

	trace_dai_with_ids(dev, "dai_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATE_ALREADY_SET)
		return PPL_PATH_STOP;

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

	/* set default callback type if irq disabled */
	if (config->irq_disabled)
		dd->cb_type = DMA_CB_TYPE_BLOCK;

	/* dma reconfig not required if XRUN handling */
	if (dd->xrun) {
		/* after prepare, we have recovered from xrun */
		dd->xrun = 0;
		return ret;
	}

	ret = dma_set_config(dd->dma, dd->chan, &dd->config);
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

	dd->dai_pos_blks = 0;
	if (dd->dai_pos)
		*dd->dai_pos = 0;
	dd->dai_pos = NULL;
	dd->wallclock = 0;
	dev->position = 0;
	dd->xrun = 0;
	dd->cb_type = 0;
	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
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

	if (ret == COMP_STATE_ALREADY_SET)
		return PPL_PATH_STOP;

	switch (cmd) {
	case COMP_TRIGGER_START:
		trace_dai_with_ids(dev, "dai_comp_trigger(), START");

		/* only start the DAI if we are not XRUN handling
		 * and the pipeline is not preloaded as in this
		 * case start is deferred to the first copy call
		 */
		if (dd->xrun == 0 && !pipeline_is_preload(dev->pipeline)) {
			/* start the DAI */
			ret = dma_start(dd->dma, dd->chan);
			if (ret < 0)
				return ret;
			dai_trigger(dd->dai, cmd, dev->params.direction);
		} else {
			dd->xrun = 0;
		}

		/* update starting wallclock */
		platform_dai_wallclock(dev, &dd->wallclock);
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
			ret = dma_release(dd->dma, dd->chan);
			if (ret < 0)
				return ret;

			/* start the DAI */
			ret = dma_start(dd->dma, dd->chan);
			if (ret < 0)
				return ret;
			dai_trigger(dd->dai, cmd, dev->params.direction);
		} else {
			dd->xrun = 0;
		}

		/* update starting wallclock */
		platform_dai_wallclock(dev, &dd->wallclock);
		break;
	case COMP_TRIGGER_XRUN:
		trace_dai_with_ids(dev, "dai_comp_trigger(), XRUN");
		dd->xrun = 1;

		/* fallthrough */
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
		trace_dai_with_ids(dev, "dai_comp_trigger(), PAUSE/STOP");
		ret = dma_stop(dd->dma, dd->chan);
		dai_trigger(dd->dai, COMP_TRIGGER_STOP, dev->params.direction);
		break;
	default:
		break;
	}

	return ret;
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	uint32_t flags = 0;
	uint32_t avail_bytes = 0;
	uint32_t free_bytes = 0;
	uint32_t copy_bytes = 0;
	int ret = 0;

	tracev_dai_with_ids(dev, "dai_copy()");

	/* start DMA on preload */
	if (pipeline_is_preload(dev->pipeline)) {
		/* start the DAI */
		ret = dma_start(dd->dma, dd->chan);
		if (ret < 0)
			return ret;
		dai_trigger(dd->dai, COMP_TRIGGER_START,
			    dev->params.direction);
		platform_dai_wallclock(dev, &dd->wallclock);

		/* let's not copy further */
		return 1;
	}

	/* get data sizes from DMA */
	ret = dma_get_data_size(dd->dma, dd->chan, &avail_bytes, &free_bytes);
	if (ret < 0)
		return ret;

	/* calculate minimum size to copy */
	copy_bytes = dev->params.direction == SOF_IPC_STREAM_PLAYBACK ?
		MIN(dd->dma_buffer->avail, free_bytes) :
		MIN(avail_bytes, dd->dma_buffer->free);

	tracev_dai_with_ids(dev, "dai_copy(), copy_bytes = 0x%x", copy_bytes);

	if (dd->cb_type & DMA_CB_TYPE_BLOCK)
		flags |= DMA_COPY_BLOCK;

	if (dd->cb_type & DMA_CB_TYPE_LLIST)
		flags |= DMA_COPY_LLIST;

	ret = dma_copy(dd->dma, dd->chan, copy_bytes, flags);
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
	struct dai_data *dd = comp_get_drvdata(dev);
	int channel = 0;
	int i;

	trace_dai("config comp %d pipe %d dai %d type %d", dev->comp.id,
		  dev->comp.pipeline_id, config->dai_index, config->type);

	switch (config->type) {
	case SOF_DAI_INTEL_SSP:
		/* set dma burst elems to slot number */
		dd->config.burst_elems = config->ssp.tdm_slots;

		/* calc frame bytes */
		switch (config->ssp.sample_valid_bits) {
		case 16:
			dev->frame_bytes = 2 * config->ssp.tdm_slots;
			break;
		case 17 ... 32:
			dev->frame_bytes = 4 * config->ssp.tdm_slots;
			break;
		default:
			break;
		}
		break;
	case SOF_DAI_INTEL_DMIC:
		/* The frame bytes setting follows only FIFO A setting in
		 * this DMIC driver version.
		 */
		trace_dai_with_ids(dev, "dai_config(), config->type = SOF_DAI_INTEL_DMIC");

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
		dev->frame_bytes = 0;
		for (i = 0; i < config->dmic.num_pdm_active; i++) {
			trace_dai_with_ids(dev, "dai_config, "
				"config->dmic.pdm[%u].enable_mic_a = %u; ",
				config->dmic.pdm[i].id,
				config->dmic.pdm[i].enable_mic_a);
			trace_dai_with_ids(dev, "dai_config, "
				"config->dmic.pdm[%u].enable_mic_b = %u; ",
				config->dmic.pdm[i].id,
				config->dmic.pdm[i].enable_mic_b);
			dev->frame_bytes += (config->dmic.fifo_bits >> 3) *
				(config->dmic.pdm[i].enable_mic_a +
				 config->dmic.pdm[i].enable_mic_b);
		}

		/* Packing of mono streams from several PDM controllers is not
		 * supported. In such cases the stream needs to be two
		 * channels.
		 */
		if (config->dmic.num_pdm_active > 1) {
			dev->frame_bytes = 2 * config->dmic.num_pdm_active *
				(config->dmic.fifo_bits >> 3);
		}

		trace_dai_with_ids(dev, "dai_config(), dev->frame_bytes = %u",
				   dev->frame_bytes);
		break;
	case SOF_DAI_INTEL_HDA:
		/* set to some non-zero value to satisfy the condition below,
		 * it is recalculated in dai_params() later
		 * this is temp until dai/hda model is changed.
		 */
		dev->frame_bytes = 4;
		channel = config->hda.link_dma_ch;
		break;
	default:
		/* other types of DAIs not handled for now */
		trace_dai_error_with_ids(dev, "dai_config() error: Handling of "
					 "DAIs other than SOF_DAI_INTEL_SSP, "
					 "SOF_DAI_INTEL_DMIC or "
					 "SOF_DAI_INTEL_HDA is not handled for "
					 "now.");
		break;
	}

	if (dev->frame_bytes == 0) {
		trace_dai_error_with_ids(dev, "dai_config() error: "
					 "dev->frame_bytes == 0");
		return -EINVAL;
	}

	if (dd->chan == DMA_CHAN_INVALID)
		/* get dma channel at first config only */
		dd->chan = dma_channel_get(dd->dma, channel);

	if (dd->chan < 0) {
		trace_dai_error_with_ids(dev, "dai_config() error: "
					 "dma_channel_get() failed");
		return -EIO;
	}

	/* set up callback */
	dma_set_cb(dd->dma, dd->chan, DMA_CB_TYPE_BLOCK | DMA_CB_TYPE_LLIST |
		   DMA_CB_TYPE_PROCESS, dai_dma_cb, dev);

	dev->is_dma_connected = 1;

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

DECLARE_COMPONENT(sys_comp_dai_init);
