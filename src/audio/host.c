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
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/trace.h>
#include <reef/dma.h>
#include <reef/ipc.h>
#include <reef/wait.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <platform/dma.h>

#define trace_host(__e)	trace_event(TRACE_CLASS_HOST, __e)
#define tracev_host(__e)	tracev_event(TRACE_CLASS_HOST, __e)
#define trace_host_error(__e)	trace_error(TRACE_CLASS_HOST, __e)

struct hc_buf {
	/* host buffer info */
	struct list_item elem_list;
	struct list_item *current;
	uint32_t current_end;
};

struct host_data {
	/* local DMA config */
	struct dma *dma;
	int chan;
	struct dma_sg_config config;
	completion_t complete;
	struct period_desc *period;
	struct comp_buffer *dma_buffer;

	/* local and host DMA buffer info */
	struct hc_buf host;
	struct hc_buf local;
	uint32_t host_size;
	volatile uint32_t *host_pos;	/* points to mailbox */
	uint32_t host_pos_blks;		/* position in bytes (nearest block) */
	uint32_t host_period_bytes;	/* host period size in bytes */
	uint32_t host_period_pos;	/* position in current host perid */
	/* pointers set during params to host or local above */
	struct hc_buf *source;
	struct hc_buf *sink;

	/* stream info */
	struct stream_params params;
	struct comp_position cp;
};

static inline struct dma_sg_elem *next_buffer(struct hc_buf *hc)
{
	struct dma_sg_elem *elem;

	if (list_item_is_last(hc->current, &hc->elem_list))
		elem = list_first_item(&hc->elem_list, struct dma_sg_elem, list);
	else
		elem = list_first_item(hc->current, struct dma_sg_elem, list);

	hc->current = &elem->list;
	return elem;
}

/* this is called by DMA driver every time descriptor has completed */
static void host_dma_cb(void *data, uint32_t type)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *local_elem, *source_elem, *sink_elem;
	struct comp_buffer *dma_buffer;

	local_elem = list_first_item(&hd->config.elem_list,
		struct dma_sg_elem, list);

	/* new local period, update host buffer position blks */
	hd->host_pos_blks += hd->period->size;

	/* buffer overlap ? */
	if (hd->host_pos_blks >= hd->host_size)
		hd->host_pos_blks = 0;
	if (hd->host_pos)
		*hd->host_pos = hd->host_pos_blks;

	/* update source buffer elem and check for overflow */
	local_elem->src += hd->period->size;
	if (local_elem->src >= hd->source->current_end) {

		/* move onto next host side elem */
		source_elem = next_buffer(hd->source);
		hd->source->current_end = source_elem->src + source_elem->size;
		local_elem->src = source_elem->src;
	}

	/* update sink buffer elem and check for overflow */
	local_elem->dest += hd->period->size;
	if (local_elem->dest >= hd->sink->current_end) {

		/* move onto next host side elem */
		sink_elem = next_buffer(hd->sink);
		hd->sink->current_end = sink_elem->dest + sink_elem->size;
		local_elem->dest = sink_elem->dest;
	}

	/* update buffer positions */
	dma_buffer = hd->dma_buffer;
	if (hd->params.direction == STREAM_DIRECTION_PLAYBACK) {
		dma_buffer->w_ptr += hd->period->size;

		if (dma_buffer->w_ptr >= dma_buffer->end_addr)
			dma_buffer->w_ptr = dma_buffer->addr;
#if 0
		trace_value((uint32_t)(hd->dma_buffer->w_ptr - hd->dma_buffer->addr));
#endif
	} else {
		hd->dma_buffer->r_ptr += hd->period->size;

		if (dma_buffer->r_ptr >= dma_buffer->end_addr)
			dma_buffer->r_ptr = dma_buffer->addr;
#if 0
		trace_value((uint32_t)(hd->dma_buffer->r_ptr - hd->dma_buffer->addr));
#endif
	}

	/* recalc available buffer space */
	comp_update_buffer(hd->dma_buffer);

	/* send IPC message to driver if needed */
	hd->host_period_pos += hd->period->size;
	if (hd->host_period_pos >= hd->host_period_bytes) {
		hd->host_period_pos = 0;
		ipc_stream_send_notification(dev, &hd->cp);
	}

	/* let any waiters know we have completed */
	wait_completed(&hd->complete);
}

static struct comp_dev *host_new(uint32_t type, uint32_t index,
	uint32_t direction)
{
	struct comp_dev *dev;
	struct host_data *hd;
	struct dma_sg_elem *elem;

	dev = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	hd = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(*hd));
	if (hd == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		return NULL;
	}

	elem = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(*elem));
	if (elem == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		rfree(RZONE_MODULE, RMOD_SYS, hd);
		return NULL;
	}

	comp_set_drvdata(dev, hd);
	comp_set_host_ep(dev);
	hd->dma = dma_get(DMA_ID_DMAC0);
	if (hd->dma == NULL)
		goto error;
	hd->host_size = 0;
	hd->host_period_bytes = 0;
	hd->host_pos = NULL;
	hd->source = NULL;
	hd->sink = NULL;

	/* init buffer elems */
	list_init(&hd->config.elem_list);
	list_init(&hd->host.elem_list);
	list_init(&hd->local.elem_list);
	list_item_prepend(&elem->list, &hd->config.elem_list);

	/* get DMA channel from DMAC0 */
	hd->chan = dma_channel_get(hd->dma);
	if (hd->chan < 0)
		goto error;

	/* set up callback */
	dma_set_cb(hd->dma, hd->chan, DMA_IRQ_TYPE_LLIST, host_dma_cb, dev);

	return dev;

error:
	rfree(RZONE_MODULE, RMOD_SYS, elem);
	rfree(RZONE_MODULE, RMOD_SYS, hd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
	return NULL;
}

static void host_free(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *elem;

	elem = list_first_item(&hd->config.elem_list,
		struct dma_sg_elem, list);
	dma_channel_put(hd->dma, hd->chan);

	rfree(RZONE_MODULE, RMOD_SYS, elem);
	rfree(RZONE_MODULE, RMOD_SYS, hd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
}

static int create_local_elems(struct comp_dev *dev,
	struct stream_params *params)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *e;
	struct list_item *elist, *tlist;
	int i;

	for (i = 0; i < hd->period->number; i++) {
		/* allocate new host DMA elem and add it to our list */
		e = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(*e));
		if (e == NULL)
			goto unwind;

		if (params->direction == STREAM_DIRECTION_PLAYBACK)
			e->dest = (uint32_t)(hd->dma_buffer->addr) +
				i * hd->period->size;
		else
			e->src = (uint32_t)(hd->dma_buffer->addr) +
				i * hd->period->size;

		e->size = hd->period->size;

		list_item_append(&e->list, &hd->local.elem_list);
	}

	return 0;

unwind:
	list_for_item_safe(elist, tlist, &hd->local.elem_list) {

		e = container_of(elist, struct dma_sg_elem, list);
		list_item_del(&e->list);
		rfree(RZONE_MODULE, RMOD_SYS, e);
	}
	return -ENOMEM;
}

/* configure the DMA params and descriptors for host buffer IO */
static int host_params(struct comp_dev *dev, struct stream_params *params)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &hd->config;
	struct dma_sg_elem *source_elem, *sink_elem, *local_elem;
	int err;

	/* set params */
	hd->params = *params;
	comp_set_sink_params(dev, params);

	/* determine source and sink buffer elems */
	if (params->direction == STREAM_DIRECTION_PLAYBACK) {
		hd->source = &hd->host;
		hd->sink = &hd->local;
		hd->dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);
		hd->period = &hd->dma_buffer->desc.source_period;
		config->direction = DMA_DIR_HMEM_TO_LMEM;
	} else {
		hd->source = &hd->local;
		hd->sink = &hd->host;
		hd->dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);
		hd->period = &hd->dma_buffer->desc.sink_period;
		config->direction = DMA_DIR_LMEM_TO_HMEM;
	}

	/* component buffer size must be divisor of host buffer size */
	if (hd->host_size % hd->period->size) {
		trace_comp_error("eHB");
		trace_value(hd->host_size);
		trace_value(hd->period->size);
		return -EINVAL;
	}

	/* create SG DMA elems for local DMA buffer */
	err = create_local_elems(dev, params);
	if (err < 0)
		return err;

	hd->dma_buffer->r_ptr = hd->dma_buffer->addr;
	hd->dma_buffer->w_ptr = hd->dma_buffer->addr;

	/* set up DMA configuration */
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 0;

	/* setup elem to point to first source elem */
	source_elem = list_first_item(&hd->source->elem_list,
		struct dma_sg_elem, list);
	hd->source->current = &source_elem->list;
	hd->source->current_end = source_elem->src + source_elem->size;

	/* setup elem to point to first sink elem */
	sink_elem = list_first_item(&hd->sink->elem_list,
		struct dma_sg_elem, list);
	hd->sink->current = &sink_elem->list;
	hd->sink->current_end = sink_elem->dest + sink_elem->size;

	/* local element */
	local_elem = list_first_item(&hd->config.elem_list,
		struct dma_sg_elem, list);
	local_elem->dest = sink_elem->dest;
	local_elem->size = hd->period->size;
	local_elem->src = source_elem->src;
	return 0;
}

/* preload the local buffers with available host data before start */
static int host_preload(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	int ret, i;

	trace_host("PrL");
	/* preload all periods */
	for (i = 0; i < PLAT_HOST_PERIODS; i++) {
		/* do DMA transfer */
		wait_init(&hd->complete);
		dma_set_config(hd->dma, hd->chan, &hd->config);
		dma_start(hd->dma, hd->chan);

		/* wait for DMA to finish */
		hd->complete.timeout = PLATFORM_DMA_TIMEOUT;
		ret = wait_for_completion_timeout(&hd->complete);
		if (ret < 0) {
			trace_comp_error("eHp");
			break;
		}
	}

	return ret;
}

static int host_prepare(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct comp_buffer *dma_buffer;

	if (hd->params.direction == STREAM_DIRECTION_PLAYBACK) {
		dma_buffer = list_first_item(&dev->bsink_list,
			struct comp_buffer, source_list);

		dma_buffer->r_ptr = dma_buffer->addr;
		dma_buffer->w_ptr = dma_buffer->addr;
	} else {
		dma_buffer = list_first_item(&dev->bsource_list,
			struct comp_buffer, sink_list);

		dma_buffer->r_ptr = dma_buffer->addr;
		dma_buffer->w_ptr = dma_buffer->addr;
	}

	if (hd->host_pos)
		*hd->host_pos = 0;
	hd->host_pos_blks = 0;
	hd->host_period_pos = 0;
	hd->host_period_bytes =
		hd->params.period_frames * hd->params.frame_size;

	if (hd->params.direction == STREAM_DIRECTION_PLAYBACK)
		host_preload(dev);

	dev->state = COMP_STATE_PREPARE;
	return 0;
}

static struct comp_dev* host_volume_component(struct comp_dev *host)
{
	struct comp_dev *comp_dev = NULL;
	struct list_item *clist;

	list_for_item(clist, &host->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer,
			source_list);

		if (buffer->sink->drv->type == COMP_TYPE_VOLUME) {
			comp_dev = buffer->sink;
			break;
		}
	}

	return comp_dev;
}

/* used to pass standard and bespoke commands (with data) to component */
static int host_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct comp_dev *vol_dev = NULL;
	int ret = 0;

	// TODO: align cmd macros.
	switch (cmd) {
	case COMP_CMD_PAUSE:
		/* channel is paused by DAI */
		dev->state = COMP_STATE_PAUSED;
		break;
	case COMP_CMD_STOP:
		/* stop any new DMA copies (let existing finish though) */
		dev->state = COMP_STATE_STOPPED;
		break;
	case COMP_CMD_RELEASE:
		/* channel is released by DAI */
		dev->state = COMP_STATE_RUNNING;
		break;
	case COMP_CMD_START:
		dma_set_config(hd->dma, hd->chan, &hd->config);
		dma_start(hd->dma, hd->chan);
		dev->state = COMP_STATE_RUNNING;
		break;
	case COMP_CMD_SUSPEND:
	case COMP_CMD_RESUME:
		break;
	case COMP_CMD_IPC_MMAP_RPOS:
		hd->host_pos = data;
		break;
	case COMP_CMD_VOLUME:
		vol_dev = host_volume_component(dev);
		if (vol_dev != NULL)
			ret = comp_cmd(vol_dev, COMP_CMD_VOLUME, data);
		break;
	default:
		break;
	}

	return ret;
}

static int host_buffer(struct comp_dev *dev, struct dma_sg_elem *elem,
		uint32_t host_size)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *e;

	/* allocate new host DMA elem and add it to our list */
	e = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(*e));
	if (e == NULL)
		return -ENOMEM;

	*e = *elem;
	hd->host_size = host_size;

	list_item_append(&e->list, &hd->host.elem_list);
	return 0;
}

static int host_reset(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct dma_sg_elem *e;
	struct list_item *elist, *tlist;

	/* free all host DMA elements */
	list_for_item_safe(elist, tlist, &hd->host.elem_list) {

		e = container_of(elist, struct dma_sg_elem, list);
		list_item_del(&e->list);
		rfree(RZONE_MODULE, RMOD_SYS, e);
	}

	/* free all local DMA elements */
	list_for_item_safe(elist, tlist, &hd->local.elem_list) {

		e = container_of(elist, struct dma_sg_elem, list);
		list_item_del(&e->list);
		rfree(RZONE_MODULE, RMOD_SYS, e);
	}

	hd->host_size = 0;
	if (hd->host_pos)
		*hd->host_pos = 0;
	hd->host_pos = NULL;
	hd->host_period_bytes = 0;
	hd->source = NULL;
	hd->sink = NULL;
	dev->state = COMP_STATE_INIT;

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int host_copy(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	if (dev->state != COMP_STATE_RUNNING)
		return 0;

	dma_set_config(hd->dma, hd->chan, &hd->config);
	dma_start(hd->dma, hd->chan);

	return 0;
}
struct comp_driver comp_host = {
	.type	= COMP_TYPE_HOST,
	.ops	= {
		.new		= host_new,
		.free		= host_free,
		.params		= host_params,
		.reset		= host_reset,
		.cmd		= host_cmd,
		.copy		= host_copy,
		.prepare	= host_prepare,
		.host_buffer	= host_buffer,
	},
};

void sys_comp_host_init(void)
{
	comp_register(&comp_host);
}
