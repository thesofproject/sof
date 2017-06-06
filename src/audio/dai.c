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
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/dai.h>
#include <reef/alloc.h>
#include <reef/dma.h>
#include <reef/wait.h>
#include <reef/stream.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <platform/dma.h>

#define DAI_PLAYBACK_STREAM	0
#define DAI_CAPTURE_STREAM	1

/* tracing */
#define trace_dai(__e) trace_event(TRACE_CLASS_DAI, __e)
#define trace_dai_error(__e)   trace_error(TRACE_CLASS_DAI, __e)
#define tracev_dai(__e)        tracev_event(TRACE_CLASS_DAI, __e)


struct dai_data {
	/* local DMA config */
	int chan;
	struct dma_sg_config config;

	int direction;
	uint32_t stream_format;
	struct dai *ssp;
	struct dma *dma;

	uint32_t last_bytes;    /* the last bytes(<period size) it copies. */
	uint32_t dai_pos_blks;	/* position in bytes (nearest block) */

	volatile uint64_t *dai_pos; /* host can read back this value without IPC */
};

static int dai_cmd(struct comp_dev *dev, int cmd, void *data);
/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_cb(void *data, uint32_t type, struct dma_sg_elem *next)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct period_desc *dma_period_desc;
	struct comp_buffer *dma_buffer;
	uint32_t copied_size;

	if (dd->direction == STREAM_DIRECTION_PLAYBACK) {
		dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);

		dma_period_desc = &dma_buffer->desc.sink_period;
		copied_size = dd->last_bytes ? dd->last_bytes : dma_period_desc->size;
		dma_buffer->r_ptr += copied_size;

		/* check for end of buffer */
		if (dma_buffer->r_ptr >= dma_buffer->end_addr) {
			dma_buffer->r_ptr = dma_buffer->addr;
			/* update host position(in bytes offset) for drivers */
			dd->dai_pos_blks += dma_buffer->desc.size;
		}

#if 0
		// TODO: move this to new trace mechanism
		trace_value((uint32_t)(dma_buffer->r_ptr - dma_buffer->addr));
#endif

		/* update host position(in bytes offset) for drivers */
		dd->dai_pos_blks += dma_period_desc->size;
		if (dd->dai_pos)
			*dd->dai_pos = dd->dai_pos_blks +
				dma_buffer->r_ptr - dma_buffer->addr;

		/* recalc available buffer space */
		comp_update_buffer_consume(dma_buffer);
	} else {
		dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);

		dma_period_desc = &dma_buffer->desc.source_period;
		dma_buffer->w_ptr += dma_period_desc->size;

		/* check for end of buffer */
		if (dma_buffer->w_ptr >= dma_buffer->end_addr) {
			dma_buffer->w_ptr = dma_buffer->addr;
			/* update host position(in bytes offset) for drivers */
			dd->dai_pos_blks += dma_buffer->desc.size;
		}

#if 0
		// TODO: move this to new trace mechanism
		trace_value((uint32_t)(dma_buffer->w_ptr - dma_buffer->addr));
#endif

		if (dd->dai_pos)
			*dd->dai_pos = dd->dai_pos_blks +
				dma_buffer->w_ptr - dma_buffer->addr;

		/* recalc available buffer space */
		comp_update_buffer_produce(dma_buffer);
	}

	if (dd->direction == STREAM_DIRECTION_PLAYBACK &&
				dma_buffer->avail < dma_period_desc->size) {
		/* end of stream, finish */
		if (dma_buffer->avail == 0) {
			dai_cmd(dev, COMP_CMD_STOP, NULL);

			/* stop dma immediatly */
			next->size = DMA_RELOAD_END;

			/* let any waiters know we have completed */
			wait_completed(&dev->pipeline->complete);

			return;
		} else {
			/* drain the last bytes */
			next->src = (uint32_t)dma_buffer->r_ptr;
			next->dest = dai_fifo(dd->ssp, dd->direction);
			next->size = dma_buffer->avail;

			dd->last_bytes = next->size;

			goto next_copy;

		}

	}
	/* notify pipeline that DAI needs it's buffer filled */
//	if (dev->state == COMP_STATE_RUNNING)
		pipeline_schedule_copy(dev->pipeline, dev);

next_copy:

	return;
}

static struct comp_dev *dai_new_ssp(uint32_t type, uint32_t index,
	uint32_t direction)
{
	struct comp_dev *dev;
	struct dai_data *dd;

	dev = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	dd = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*dd));
	if (dd == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, dd);
	comp_set_dai_ep(dev);

	dd->ssp = dai_get(type, index);
	if (dd->ssp == NULL)
		goto error;
	dd->dma = dma_get(DMA_ID_DMAC1);
	if (dd->dma == NULL)
		goto error;

	list_init(&dd->config.elem_list);
	dd->dai_pos = NULL;
	dd->dai_pos_blks = 0;
	dd->last_bytes = 0;

	/* get DMA channel from DMAC1 */
	dd->chan = dma_channel_get(dd->dma);
	if (dd->chan < 0)
		goto error;

	dd->stream_format = PLATFORM_SSP_STREAM_FORMAT;

	/* set up callback */
	//if (dd->ssp->plat_data.flags & DAI_FLAGS_IRQ_CB)
		dma_set_cb(dd->dma, dd->chan, DMA_IRQ_TYPE_LLIST, dai_dma_cb, dev);

	return dev;

error:
	rfree(dd);
	rfree(dev);
	return NULL;
}

static struct comp_dev *dai_new_hda(uint32_t type, uint32_t index,
	uint32_t direction)
{
	return 0;
}

static void dai_free(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	dma_channel_put(dd->dma, dd->chan);

	rfree(dd);
	rfree(dev);
}

/* set component audio SSP and DMA configuration */
static int dai_playback_params(struct comp_dev *dev,
	struct stream_params *params)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	struct dma_sg_elem *elem;
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;
	struct list_item *elist, *tlist;
	int i;

	dd->direction = params->direction;

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_DEV;
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 1;
	config->dest_dev = dd->ssp->plat_data.fifo[0].handshake;

	/* set up local and host DMA elems to reset values */
	dma_buffer = list_first_item(&dev->bsource_list,
		struct comp_buffer, sink_list);
	dma_period_desc = &dma_buffer->desc.sink_period;
	dma_buffer->params = *params;

	/* set it to dai stream format, for volume func correct mapping */
	dma_buffer->params.pcm.format = dd->stream_format;

	if (list_is_empty(&config->elem_list)) {
		/* set up cyclic list of DMA elems */
		for (i = 0; i < dma_period_desc->number; i++) {

			elem = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*elem));
			if (elem == NULL)
				goto err_unwind;

			elem->size = dma_period_desc->size;
			elem->src = (uint32_t)(dma_buffer->r_ptr) +
				i * dma_period_desc->size;

			elem->dest = dai_fifo(dd->ssp, params->direction);

			list_item_append(&elem->list, &config->elem_list);
		}
	}

	/* set write pointer to start of buffer */
	dma_buffer->w_ptr = dma_buffer->addr;

	return 0;

err_unwind:
	list_for_item_safe(elist, tlist, &config->elem_list) {
		elem = container_of(elist, struct dma_sg_elem, list);
		list_item_del(&elem->list);
		rfree(elem);
	}
	return -ENOMEM;
}

static int dai_capture_params(struct comp_dev *dev,
	struct stream_params *params)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	struct dma_sg_elem *elem;
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;
	struct list_item *elist, *tlist;
	int i;

	dd->direction = params->direction;

	/* set up DMA configuration */
	config->direction = DMA_DIR_DEV_TO_MEM;
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 1;
	config->src_dev = dd->ssp->plat_data.fifo[1].handshake;

	/* set up local and host DMA elems to reset values */
	dma_buffer = list_first_item(&dev->bsink_list,
		struct comp_buffer, source_list);
	dma_period_desc = &dma_buffer->desc.source_period;
	dma_buffer->params = *params;

	/* set it to dai stream format, for volume func correct mapping */
	dma_buffer->params.pcm.format = dd->stream_format;

	if (list_is_empty(&config->elem_list)) {
		/* set up cyclic list of DMA elems */
		for (i = 0; i < dma_period_desc->number; i++) {

			elem = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*elem));
			if (elem == NULL)
				goto err_unwind;

			elem->size = dma_period_desc->size;
			elem->dest = (uint32_t)(dma_buffer->w_ptr) +
				i * dma_period_desc->size;
			elem->src = dai_fifo(dd->ssp, params->direction);
			list_item_append(&elem->list, &config->elem_list);
		}
	}

	/* set write pointer to start of buffer */
	dma_buffer->r_ptr = dma_buffer->addr;

	return 0;

err_unwind:
	list_for_item_safe(elist, tlist, &config->elem_list) {
		elem = container_of(elist, struct dma_sg_elem, list);
		list_item_del(&elem->list);
		rfree(elem);
	}
	return -ENOMEM;
}

static int dai_params(struct comp_dev *dev,
	struct stream_params *params)
{
	struct comp_buffer *dma_buffer;

	/* can set params on only init state */
	if (dev->state != COMP_STATE_INIT) {
		trace_dai_error("wdp");
		return -EINVAL;
	}
	if (params->direction == STREAM_DIRECTION_PLAYBACK) {
		dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);
		dma_buffer->r_ptr = dma_buffer->addr;

		return dai_playback_params(dev, params);
	} else {
		dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);
		dma_buffer->w_ptr = dma_buffer->addr;

		return dai_capture_params(dev, params);
	}
}

static int dai_prepare(struct comp_dev *dev)
{
	int ret = 0;
	struct dai_data *dd = comp_get_drvdata(dev);

	if (list_is_empty(&dd->config.elem_list)) {
		trace_dai_error("wdm");
		return -EINVAL;
	}

	ret = dma_set_config(dd->dma, dd->chan, &dd->config);
	dev->state = COMP_STATE_PREPARE;
	return ret;
}

static int dai_reset(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	struct list_item *elist, *tlist;
	struct dma_sg_elem *elem;

	list_for_item_safe(elist, tlist, &config->elem_list) {
		elem = container_of(elist, struct dma_sg_elem, list);
		list_item_del(&elem->list);
		rfree(elem);
	}

	dev->state = COMP_STATE_INIT;
	dd->dai_pos_blks = 0;
	if (dd->dai_pos)
		*dd->dai_pos = 0;
	dd->dai_pos = NULL;
	dd->last_bytes = 0;

	return 0;
}

/* used to pass standard and bespoke commandd (with data) to component */
static int dai_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	int ret;

	switch (cmd) {

	case COMP_CMD_PAUSE:
		if (dev->state == COMP_STATE_RUNNING) {
			dma_pause(dd->dma, dd->chan);
			dai_trigger(dd->ssp, cmd, dd->direction);
			dev->state = COMP_STATE_PAUSED;
		}
		break;
	case COMP_CMD_STOP:
		switch (dev->state) {
		case COMP_STATE_RUNNING:
		case COMP_STATE_PAUSED:
			dma_stop(dd->dma, dd->chan,
				dev->state == COMP_STATE_RUNNING ? 1 : 0);
			/* need stop ssp */
			dai_trigger(dd->ssp, cmd, dd->direction);
		/* go through */
		case COMP_STATE_PREPARE:
			dd->last_bytes = 0;
			dev->state = COMP_STATE_SETUP;
			break;
		}
		break;
	case COMP_CMD_RELEASE:
		/* only release from paused*/
		if (dev->state == COMP_STATE_PAUSED) {
			dai_trigger(dd->ssp, cmd, dd->direction);
			dma_release(dd->dma, dd->chan);
			dev->state = COMP_STATE_RUNNING;
		}
		break;
	case COMP_CMD_START:
		/* only start from prepared*/
		if (dev->state == COMP_STATE_PREPARE) {
			ret = dma_start(dd->dma, dd->chan);
			if (ret < 0)
				return ret;
			dai_trigger(dd->ssp, cmd, dd->direction);
			dev->state = COMP_STATE_RUNNING;
		}
		break;
	case COMP_CMD_SUSPEND:
	case COMP_CMD_RESUME:
		break;
	case COMP_CMD_IPC_MMAP_PPOS:
		dd->dai_pos = data;
		if (dd->dai_pos)
			*dd->dai_pos = 0;
		break;
	default:
		break;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *dev)
{
	return 0;
}

/* source component will preload dai */
static int dai_preload(struct comp_dev *dev)
{
	return 0;
}

static int dai_config(struct comp_dev *dev, struct dai_config *dai_config)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	return dai_set_config(dd->ssp, dai_config);
}

static int dai_set_loopback(struct comp_dev *dev, uint32_t lbm)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	return dai_set_loopback_mode(dd->ssp, lbm);
}

static struct comp_driver comp_dai_ssp = {
	.type	= COMP_TYPE_DAI_SSP,
	.ops	= {
		.new		= dai_new_ssp,
		.free		= dai_free,
		.params		= dai_params,
		.cmd		= dai_cmd,
		.copy		= dai_copy,
		.prepare	= dai_prepare,
		.reset		= dai_reset,
		.dai_config	= dai_config,
		.preload	= dai_preload,
		.dai_set_loopback = dai_set_loopback,
	},
};

static struct comp_driver comp_dai_hda = {
	.type	= COMP_TYPE_DAI_HDA,
	.ops	= {
		.new		= dai_new_hda,
		.free		= dai_free,
		.params		= dai_params,
		.cmd		= dai_cmd,
		.copy		= dai_copy,
		.prepare	= dai_prepare,
		.preload	= dai_preload,
	},
};

void sys_comp_dai_init(void)
{
	comp_register(&comp_dai_ssp);
	comp_register(&comp_dai_hda);
}
