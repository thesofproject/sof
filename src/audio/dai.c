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
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <platform/dma.h>
#include <arch/cache.h>

#define DAI_PLAYBACK_STREAM	0
#define DAI_CAPTURE_STREAM	1

#define DAI_PTR_INIT_DAI	1	/* buffer ptr initialized by dai */
#define DAI_PTR_INIT_HOST	2	/* buffer ptr initialized by host */

/* tracing */
#define trace_dai(__e) trace_event(TRACE_CLASS_DAI, __e)
#define trace_dai_error(__e)   trace_error(TRACE_CLASS_DAI, __e)
#define tracev_dai(__e)        tracev_event(TRACE_CLASS_DAI, __e)


struct dai_data {
	/* local DMA config */
	int chan;
	struct dma_sg_config config;

	struct dai *dai;
	struct dma *dma;
	uint32_t period_bytes;
	int xrun;		/* true if we are doing xrun recovery */
	int pointer_init;	/* true if buffer pointer was initialized */

	uint32_t dai_pos_blks;	/* position in bytes (nearest block) */

	volatile uint64_t *dai_pos; /* host can read back this value without IPC */
	uint64_t wallclock;	/* wall clock at stream start */
};

static void dai_buffer_process(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *dma_buffer;
	void *buffer_ptr;

	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		dma_buffer = list_first_item(&dev->bsource_list,
					     struct comp_buffer, sink_list);

		/* recalc available buffer space */
		comp_update_buffer_consume(dma_buffer, dd->period_bytes);

		buffer_ptr = dma_buffer->r_ptr;

		/* make sure there is available bytes for next period */
		if (dma_buffer->avail < dd->period_bytes) {
			trace_dai_error("xru");
			comp_underrun(dev, dma_buffer, dd->period_bytes, 0);
		}
	} else {
		dma_buffer = list_first_item(&dev->bsink_list,
					     struct comp_buffer, source_list);

		/* recalc available buffer space */
		comp_update_buffer_produce(dma_buffer, dd->period_bytes);

		buffer_ptr = dma_buffer->w_ptr;

		/* make sure there is free bytes for next period */
		if (dma_buffer->free < dd->period_bytes) {
			trace_dai_error("xro");
			comp_overrun(dev, dma_buffer, dd->period_bytes, 0);
		}
	}

	/* update host position (in bytes offset) for drivers */
	dev->position += dd->period_bytes;
	if (dd->dai_pos) {
		dd->dai_pos_blks += dd->period_bytes;
		*dd->dai_pos = dd->dai_pos_blks +
			buffer_ptr - dma_buffer->addr;
	}
}

/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_cb(void *data, uint32_t type, struct dma_sg_elem *next)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *dma_buffer;

	tracev_dai("irq");

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
		if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {

			dma_buffer = list_first_item(&dev->bsource_list,
				struct comp_buffer, sink_list);

			/* fill buffer with silence */
			buffer_zero(dma_buffer);
		}
		return;
	}

	dai_buffer_process(dev);

	/* notify pipeline that DAI needs its buffer processed */
	if (dev->state == COMP_STATE_ACTIVE)
		pipeline_schedule_copy(dev->pipeline, 0);
}

static struct comp_dev *dai_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_dai *dai;
	struct sof_ipc_comp_dai *ipc_dai = (struct sof_ipc_comp_dai *)comp;
	struct dai_data *dd;
	uint32_t dir, caps, dma_dev;

	trace_dai("new");

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		COMP_SIZE(struct sof_ipc_comp_dai));
	if (dev == NULL)
		return NULL;

	dai = (struct sof_ipc_comp_dai *)&dev->comp;
	memcpy(dai, ipc_dai, sizeof(struct sof_ipc_comp_dai));

	dd = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*dd));
	if (dd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, dd);

	dd->dai = dai_get(dai->type, dai->dai_index);
	if (dd->dai == NULL) {
		trace_dai_error("eDg");
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
	if (dd->dma == NULL) {
		trace_dai_error("eDd");
		goto error;
	}

	dma_sg_init(&dd->config.elem_array);
	dd->dai_pos = NULL;
	dd->dai_pos_blks = 0;
	dd->xrun = 0;
	dd->pointer_init = 0;

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

	rfree(dd);
	rfree(dev);
}

/* set component audio SSP and DMA configuration */
static int dai_playback_params(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	struct sof_ipc_comp_config *source_config;
	struct comp_buffer *dma_buffer;
	int err;
	uint32_t buffer_size;

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_DEV;
	config->src_width = comp_sample_bytes(dev);
	config->dest_width = comp_sample_bytes(dev);
	config->cyclic = 1;
	config->dest_dev = dd->dai->plat_data.fifo[0].handshake;

	/* set up local and host DMA elems to reset values */
	dma_buffer = list_first_item(&dev->bsource_list,
		struct comp_buffer, sink_list);
	source_config = COMP_GET_CONFIG(dma_buffer->source);
	buffer_size = source_config->periods_sink * dd->period_bytes;

	/* resize the buffer if space is available to align with period size */
	err = buffer_set_size(dma_buffer, buffer_size);
	if (err < 0) {
		trace_dai_error("ep1");
		trace_error_value(source_config->periods_sink);
		trace_error_value(dd->period_bytes);
		trace_error_value(buffer_size);
		trace_error_value(dma_buffer->alloc_size);
		return err;
	}

	if (!config->elem_array.elems) {
		err = dma_sg_alloc(&config->elem_array, config->direction,
				   source_config->periods_sink,
				   dd->period_bytes,
				   (uintptr_t)(dma_buffer->r_ptr),
				   dai_fifo(dd->dai, SOF_IPC_STREAM_PLAYBACK));
		if (err < 0) {
			trace_dai_error("ep3");
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
	struct comp_buffer *dma_buffer;
	int err;
	uint32_t buffer_size;

	/* set up DMA configuration */
	config->direction = DMA_DIR_DEV_TO_MEM;
	config->src_width = comp_sample_bytes(dev);
	config->dest_width = comp_sample_bytes(dev);
	config->cyclic = 1;
	config->src_dev = dd->dai->plat_data.fifo[1].handshake;

	/* set up local and host DMA elems to reset values */
	dma_buffer = list_first_item(&dev->bsink_list,
		struct comp_buffer, source_list);
	sink_config = COMP_GET_CONFIG(dma_buffer->sink);
	buffer_size = sink_config->periods_source * dd->period_bytes;

	/* resize the buffer if space is available to align with period size */
	err = buffer_set_size(dma_buffer, buffer_size);
	if (err < 0) {
		trace_dai_error("ec1");
		trace_error_value(sink_config->periods_sink);
		trace_error_value(dd->period_bytes);
		trace_error_value(buffer_size);
		trace_error_value(dma_buffer->alloc_size);
		return err;
	}

	if (!config->elem_array.elems) {
		err = dma_sg_alloc(&config->elem_array, config->direction,
				   sink_config->periods_source,
				   dd->period_bytes,
				   (uintptr_t)(dma_buffer->w_ptr),
				   dai_fifo(dd->dai, SOF_IPC_STREAM_CAPTURE));
		if (err < 0) {
			trace_dai_error("ec3");
			return err;
		}
	}

	return 0;
}

static int dai_params(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *dma_buffer;
	struct sof_ipc_comp_config *dconfig = COMP_GET_CONFIG(dev);

	trace_dai("par");

	/* can set params on only init state */
	if (dev->state != COMP_STATE_READY) {
		trace_dai_error("wdp");
		return -EINVAL;
	}

	/* get DMA channel, once the stream_tag is known */
	dd->chan = dma_channel_get(dd->dma, dev->params.stream_tag);
	if (dd->chan < 0) {
		trace_dai_error("eDc");
		return -EINVAL;
	}

	/* set up callback */
	dma_set_cb(dd->dma, dd->chan, DMA_IRQ_TYPE_BLOCK |
				DMA_IRQ_TYPE_LLIST, dai_dma_cb, dev);
	dev->is_dma_connected = 1;

	/* for DAI, we should configure its frame_fmt from topology */
	dev->params.frame_fmt = dconfig->frame_fmt;

	/* calculate period size based on config */
	dev->frame_bytes = comp_frame_bytes(dev);
	if (dev->frame_bytes == 0) {
		trace_dai_error("ed1");
		return -EINVAL;
	}

	dd->period_bytes = dev->frames * dev->frame_bytes;
	if (dd->period_bytes == 0) {
		trace_dai_error("ed2");
		return -EINVAL;
	}

	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);
		dma_buffer->r_ptr = dma_buffer->addr;

		return dai_playback_params(dev);
	} else {
		dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);
		dma_buffer->w_ptr = dma_buffer->addr;

		return dai_capture_params(dev);
	}
}

static int dai_prepare(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *dma_buffer;
	int ret = 0;

	trace_dai("pre");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	dev->position = 0;

	if (!dd->config.elem_array.elems) {
		trace_dai_error("wdm");
		comp_set_state(dev, COMP_TRIGGER_RESET);
		return -EINVAL;
	}

	/* initialise buffers */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);
	} else {
		dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);
	}

	/* TODO: not sure what this wb is for? */
	/* write back buffer contents from cache */
	dcache_writeback_region(dma_buffer->addr, dma_buffer->size);

	dd->pointer_init = 0;

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

	trace_dai("res");

	dma_channel_put(dd->dma, dd->chan);

	dma_sg_free(&config->elem_array);

	dd->dai_pos_blks = 0;
	if (dd->dai_pos)
		*dd->dai_pos = 0;
	dd->dai_pos = NULL;
	dd->wallclock = 0;
	dev->position = 0;
	dd->xrun = 0;
	dd->pointer_init = 0;
	comp_set_state(dev, COMP_TRIGGER_RESET);

	return 0;
}

/* The playback source pipeline must be advanced by one period so that it
 * does not write to the period that DMA is reading. The configuration of the
 * upstream pipeline is unknown to the DAI but we can check if the source buffer
 * is shared with another DMA engine (which preloads the buffer by one period)
 * and only advance the write pointer when source component is not another
 * DMA engine.
 */
static void dai_pointer_init(struct comp_dev *dev)
{
	struct comp_buffer *dma_buffer;
	struct dai_data *dd = comp_get_drvdata(dev);

	dd->pointer_init = DAI_PTR_INIT_DAI;

	/* not required for capture streams */
	if (dev->params.direction == SOF_IPC_STREAM_PLAYBACK) {
		dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);

		switch (dma_buffer->source->comp.type) {
		case SOF_COMP_HOST:
		case SOF_COMP_SG_HOST:
			/* buffer is preloaded and advanced by host DMA engine */
			dd->pointer_init = DAI_PTR_INIT_HOST;
			break;
		default:
			/* advance source pipeline w_ptr by one period
			 * this places pipeline w_ptr in period before DAI r_ptr */
			comp_update_buffer_produce(dma_buffer, dd->period_bytes);
			break;
		}
	}
}

/* used to pass standard and bespoke command (with data) to component */
static int dai_comp_trigger(struct comp_dev *dev, int cmd)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *dma_buffer;
	int ret;

	trace_dai("trg");
	tracev_value(cmd);

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	switch (cmd) {
	case COMP_TRIGGER_START:
		trace_dai("tsa");
		if (!dd->pointer_init)
			dai_pointer_init(dev);
		/* only start the DAI if we are not XRUN handling
		 * and the ptr is not initialized by the host as in this
		 * case start is deferred to the first copy call as the buffer
		 * is populated by the host only then
		 */
		if (dd->xrun == 0 && dd->pointer_init != DAI_PTR_INIT_HOST) {
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
		if (dev->params.direction == SOF_IPC_STREAM_CAPTURE) {
			dma_buffer = list_first_item(&dev->bsink_list,
						     struct comp_buffer,
						     source_list);
			buffer_zero(dma_buffer);
		}

		/* only start the DAI if we are not XRUN handling */
		if (dd->xrun == 0) {
			/* set valid buffer pointer */
			dai_buffer_process(dev);

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
		trace_dai("txr");
		dd->xrun = 1;

		/* fallthrough */
	case COMP_TRIGGER_PAUSE:
	case COMP_TRIGGER_STOP:
		trace_dai("tsp");
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
	int ret;

	if (dd->pointer_init == DAI_PTR_INIT_HOST) {
		/* start the DAI */
		ret = dma_start(dd->dma, dd->chan);
		if (ret < 0)
			return ret;
		dai_trigger(dd->dai, COMP_TRIGGER_START, dev->params.direction);
		dd->pointer_init = DAI_PTR_INIT_DAI; /* next copy just quits */
		platform_dai_wallclock(dev, &dd->wallclock);
	}
	return 0;
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
		trace_dai("did");

		/* We can use always the largest burst length. */
		dd->config.burst_elems = 8;

		/* Set frame size in bytes to match the configuration. */
		if (config->dmic.num_pdm_active > 1) {
			/* For two or more controllers capture from each
			 * controller must be stereo.
			 */
			dev->frame_bytes = 2 * config->dmic.num_pdm_active *
				config->dmic.fifo_bits_a >> 3;
		} else {
			/* For one controller the capture can be mono or
			 * stereo. In mono configuration only one of mic A or B
			 * is enabled.
			 */
			if (config->dmic.pdm[0].enable_mic_a +
				config->dmic.pdm[0].enable_mic_b == 1)
				dev->frame_bytes =
					config->dmic.fifo_bits_a >> 3;
			else
				dev->frame_bytes = 2 *
					config->dmic.fifo_bits_a >> 3;
		}
		trace_value(config->dmic.fifo_bits_a);
		trace_value(config->dmic.num_pdm_active);
		trace_value(config->dmic.pdm[0].enable_mic_a);
		trace_value(config->dmic.pdm[0].enable_mic_b);
		trace_value(dev->frame_bytes);
		break;
	case SOF_DAI_INTEL_HDA:
		/* set to some non-zero value to satisfy the condition below,
		 * it is recalculated in dai_params() later
		 * this is temp until dai/hda model is changed.
		 */
		dev->frame_bytes = 4;
		break;
	default:
		/* other types of DAIs not handled for now */
		trace_dai_error("de2");
		break;
	}

	if (dev->frame_bytes == 0) {
		trace_dai_error("de1");
		return -EINVAL;
	}

	return 0;
}

static void dai_cache(struct comp_dev *dev, int cmd)
{
	struct dai_data *dd;

	switch (cmd) {
	case COMP_CACHE_WRITEBACK_INV:
		trace_dai("wtb");

		dd = comp_get_drvdata(dev);

		dma_sg_cache_wb_inv(&dd->config.elem_array);

		dcache_writeback_invalidate_region(dd->dai, sizeof(*dd->dai));
		dcache_writeback_invalidate_region(dd->dai->private,
						   dd->dai->private_size);
		dcache_writeback_invalidate_region(dd->dma, sizeof(*dd->dma));
		dcache_writeback_invalidate_region(dd->dma->private,
						   dd->dma->private_size);
		dcache_writeback_invalidate_region(dd, sizeof(*dd));
		dcache_writeback_invalidate_region(dev, sizeof(*dev));
		break;

	case COMP_CACHE_INVALIDATE:
		trace_dai("inv");

		dcache_invalidate_region(dev, sizeof(*dev));

		dd = comp_get_drvdata(dev);
		dcache_invalidate_region(dd, sizeof(*dd));
		dcache_invalidate_region(dd->dma, sizeof(*dd->dma));
		dcache_invalidate_region(dd->dma->private,
					 dd->dma->private_size);
		dcache_invalidate_region(dd->dai, sizeof(*dd->dai));
		dcache_invalidate_region(dd->dai->private,
					 dd->dai->private_size);

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

void sys_comp_dai_init(void)
{
	comp_register(&comp_dai);
}
