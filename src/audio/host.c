/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
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
#include <reef/wait.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <platform/dma.h>

#define HOST_PLAYBACK_STREAM	0
#define HOST_CAPTURE_STREAM	1

struct host_stream {
	/* local DMA config */
	int chan;
	struct dma_sg_config config;
	completion_t complete;

	/* host buffer info */
	struct list_head host_elem_list;
	struct dma_sg_elem *elem;
	struct list_head *current_host;
	uint32_t current_host_end;
};

struct host_data {
	struct dma *dma;
	struct host_stream s[2];	/* playback and capture streams */	
};

static inline struct dma_sg_elem *next_host(struct host_stream *hs)
{
	struct dma_sg_elem *host_elem;

	host_elem = list_first_entry(hs->current_host,
		struct dma_sg_elem, list);
	hs->current_host = &host_elem->list;

	return host_elem;
}

/* this is called by DMA driver every time descriptor has completed */
static void host_playback_dma_cb(void *data, uint32_t type)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs = &hd->s[HOST_PLAYBACK_STREAM];
	struct dma_sg_elem *elem = hs->elem, *host_elem;
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;
	struct dma_chan_status status;
	
	/*
	 * Recalculate host side buffer information.
	 */
	dma_buffer = list_first_entry(&dev->bsink_list,
		struct comp_buffer, source_list);
	dma_period_desc = &dma_buffer->desc.source_period;

	/* update host side source buffer elem and check for overflow */
	elem->src += dma_period_desc->size;
	if (elem->src >= hs->current_host_end) {

		/* move onto next host side elem */
		host_elem = next_host(hs);
		hs->current_host_end = host_elem->src + host_elem->size;
		elem->src = host_elem->src;
	}

	/* update host dest buffer position and check for overflow */
	elem->dest += dma_period_desc->size;
	if (elem->dest >= (uint32_t)dma_buffer->end_addr)
		elem->dest = (uint32_t)dma_buffer->addr;

	/* update local buffer position */
	dma_status(hd->dma, hs->chan, &status);
	dma_buffer->w_ptr = (void*)status.position;

	/* recalc available buffer space */
	comp_update_avail(dma_buffer);

	/* let any waiters know we have completed */
	wait_completed(&hs->complete);
}

static void host_capture_dma_cb(void *data, uint32_t type)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs = &hd->s[HOST_CAPTURE_STREAM];
	struct dma_sg_elem *elem = hs->elem, *host_elem;
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;
	struct dma_chan_status status;
	
	/*
	 * Recalculate host side buffer information.
	 */

	dma_buffer = list_first_entry(&dev->bsource_list,
		struct comp_buffer, sink_list);
	dma_period_desc = &dma_buffer->desc.sink_period;

	/* update host side buffer dest elem and check for overflow */
	elem->dest += dma_period_desc->size;
	if (elem->dest >= hs->current_host_end) {

		/* move onto next host side elem */
		host_elem = next_host(hs);
		hs->current_host_end = host_elem->dest + host_elem->size;
		elem->dest = host_elem->dest;
	}

	/* update host buffer source position and check for overflow */
	elem->src += dma_period_desc->size;
	if (elem->src >= (uint32_t)dma_buffer->end_addr)
		elem->src = (uint32_t)dma_buffer->addr;

	/* update local buffer position */
	dma_status(hd->dma, hs->chan, &status);
	dma_buffer->r_ptr = (void*)status.position;

	/* recalc available buffer space */
	comp_update_avail(dma_buffer);

	/* let any waiters know we have completed */
	wait_completed(&hs->complete);
}

static struct comp_dev *host_new(uint32_t type, uint32_t index)
{
	struct comp_dev *dev;
	struct host_data *hd;
	struct host_stream *hs;
	struct dma_sg_elem *elemp, *elemc;

	dev = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	hd = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*hd));
	if (hd == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		return NULL;
	}

	elemp = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*elemp));
	if (elemp == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		rfree(RZONE_MODULE, RMOD_SYS, hd);
		return NULL;
	}

	elemc = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*elemc));
	if (elemc == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, elemp);
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		rfree(RZONE_MODULE, RMOD_SYS, hd);
		return NULL;
	}

	comp_set_drvdata(dev, hd);
	hd->dma = dma_get(DMA_ID_DMAC0);

	/* init playback stream */
	hs = &hd->s[HOST_PLAYBACK_STREAM];
	list_init(&hs->config.elem_list);
	list_init(&hs->host_elem_list);
	list_add(&elemp->list, &hs->config.elem_list);
	hs->elem = elemp;

	/* get DMA channel from DMAC0 */
	hs->chan = dma_channel_get(hd->dma);
	if (hs->chan < 0)
		goto error;

	/* set up callback */
	dma_set_cb(hd->dma, hs->chan, host_playback_dma_cb, dev);

	/* init capture stream */
	hs = &hd->s[HOST_CAPTURE_STREAM];
	list_init(&hs->config.elem_list);
	list_init(&hs->host_elem_list);
	list_add(&elemc->list, &hs->config.elem_list);
	hs->elem = elemc;

	/* get DMA channel from DMAC0 */
	hs->chan = dma_channel_get(hd->dma);
	if (hs->chan < 0)
		goto capt_error;

	/* set up callback */
	dma_set_cb(hd->dma, hs->chan, host_capture_dma_cb, dev);

	return dev;

capt_error:
	hs = &hd->s[HOST_PLAYBACK_STREAM];
	dma_channel_put(hd->dma, hs->chan);
error:
	rfree(RZONE_MODULE, RMOD_SYS, elemp);
	rfree(RZONE_MODULE, RMOD_SYS, elemc);
	rfree(RZONE_MODULE, RMOD_SYS, hd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
	return NULL;
}

static void host_free(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs;

	hs = &hd->s[HOST_PLAYBACK_STREAM];
	dma_channel_put(hd->dma, hs->chan);
	rfree(RZONE_MODULE, RMOD_SYS, hs->elem);

	hs = &hd->s[HOST_CAPTURE_STREAM];
	dma_channel_put(hd->dma, hs->chan);
	rfree(RZONE_MODULE, RMOD_SYS, hs->elem);

	rfree(RZONE_MODULE, RMOD_SYS, hd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
}

static int host_params_playback(struct comp_dev *dev,
	struct stream_params *params)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs = &hd->s[HOST_PLAYBACK_STREAM];
	struct dma_sg_config *config = &hs->config;
	struct dma_sg_elem *elem, *host_elem;
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_MEM;
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 0;

	/* setup elem to point to first host page */
	host_elem = list_first_entry(&hs->host_elem_list,
		struct dma_sg_elem, list);
	hs->current_host = &host_elem->list;
	elem = hs->elem;

	/* set up local and host DMA elems to reset values */
	dma_buffer = list_first_entry(&dev->bsink_list,
		struct comp_buffer, source_list);
	dma_period_desc = &dma_buffer->desc.sink_period;

	hs->current_host_end = host_elem->src + host_elem->size;

	elem->src = host_elem->src;
	dma_buffer->w_ptr = dma_buffer->addr;

	/* component buffer size must be divisor of host buffer size */
	if (host_elem->size % dma_period_desc->size)
		return -EINVAL;

	/* element size */
	elem->size = dma_period_desc->size;

	return 0;
}

static int host_params_capture(struct comp_dev *dev,
	struct stream_params *params)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs = &hd->s[HOST_CAPTURE_STREAM];
	struct dma_sg_config *config = &hs->config;
	struct dma_sg_elem *elem, *host_elem;
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;

	//dev->params = *params;

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_MEM;
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 0;

	/* setup elem to point to first host page */
	host_elem = list_first_entry(&hs->host_elem_list,
		struct dma_sg_elem, list);
	hs->current_host = &host_elem->list;
	elem = hs->elem;

	/* set up local and host DMA elems to reset values */
	dma_buffer = list_first_entry(&dev->bsource_list,
		struct comp_buffer, sink_list);
	dma_period_desc = &dma_buffer->desc.source_period;

	hs->current_host_end = host_elem->dest + host_elem->size;
	elem->dest = host_elem->dest;
	dma_buffer->r_ptr = dma_buffer->addr;

	/* component buffer size must be divisor of host buffer size */
	if (host_elem->size % dma_period_desc->size)
		return -EINVAL;

	/* element size */
	elem->size = dma_period_desc->size;

	return 0;
}

/* configure the DMA params and descriptors for host buffer IO */
static int host_params(struct comp_dev *dev, struct stream_params *params)
{
	/* set up local and host DMA elems to reset values */
	if (params->direction == STREAM_DIRECTION_PLAYBACK)
		return host_params_playback(dev, params);
	else
		return host_params_capture(dev, params);
}

/* preload the local buffers with available host data before start */
static int host_preload(struct comp_dev *dev, int count)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs = &hd->s[HOST_PLAYBACK_STREAM];
	int ret, i;

	for (i = 0; i < count; i++) {

		/* do DMA transfer */
		wait_init(&hs->complete);
		dma_set_config(hd->dma, hs->chan, &hs->config);
		dma_start(hd->dma, hs->chan);

		/* wait 1 msecs for DMA to finish */
		hs->complete.timeout = 1;
		ret = wait_for_completion_timeout(&hs->complete);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int host_prepare(struct comp_dev *dev, struct stream_params *params)
{
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;
	int ret = 0;

	/* preload PCM data */
	/* TODO: determine how much pre-loading we can do */
	if (params->direction == STREAM_DIRECTION_PLAYBACK) {

		dma_buffer = list_first_entry(&dev->bsource_list,
			struct comp_buffer, sink_list);
		dma_period_desc = &dma_buffer->desc.sink_period;

		ret = host_preload(dev, dma_period_desc->number - 1);
	}
	
	return ret;
}

/* used to pass standard and bespoke commands (with data) to component */
static int host_cmd(struct comp_dev *dev, struct stream_params *params,
	int cmd, void *data)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs = &hd->s[params->direction];

	switch (cmd) {
	case PIPELINE_CMD_PAUSE:
		dma_pause(hd->dma, hs->chan);
		break;
	case PIPELINE_CMD_STOP:
		dma_stop(hd->dma, hs->chan);
		break;
	case PIPELINE_CMD_RELEASE:
		dma_release(hd->dma, hs->chan);
		break;
	case PIPELINE_CMD_START:
		break;
	case PIPELINE_CMD_DRAIN:
		dma_drain(hd->dma, hs->chan);
		break;
	case PIPELINE_CMD_SUSPEND:
	case PIPELINE_CMD_RESUME:
	default:
		return -EINVAL;
	}

	return 0;
}

static int host_buffer(struct comp_dev *dev, struct stream_params *params,
	struct dma_sg_elem *elem)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs = &hd->s[params->direction];
	struct dma_sg_elem *e;

	/* allocate new host DMA elem and add it to our list */
	e = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*e));
	if (e == NULL)
		return -ENOMEM;

	*e = *elem;
	list_add(&e->list, &hs->host_elem_list);

	return 0;
}

static int host_reset(struct comp_dev *dev, struct stream_params *params)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs = &hd->s[params->direction];
	struct dma_sg_elem *e;
	struct list_head *elist, *tlist;

	/* free all host DMA elements */
	list_for_each_safe(elist, tlist, &hs->host_elem_list) {

		e = container_of(elist, struct dma_sg_elem, list);
		list_del(&e->list);
		rfree(RZONE_MODULE, RMOD_SYS, e);
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int host_copy(struct comp_dev *dev, struct stream_params *params)
{
	struct host_data *hd = comp_get_drvdata(dev);
	struct host_stream *hs = &hd->s[params->direction];
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;

	/* we can only copy new data is previous DMA request has completed */
	if (!wait_is_completed(&hs->complete))
		return 0;

	/* is there enough space in the buffer ? */
	if (params->direction == STREAM_DIRECTION_PLAYBACK) {

		dma_buffer = list_first_entry(&dev->bsink_list,
			struct comp_buffer, source_list);
		dma_period_desc = &dma_buffer->desc.source_period;
	} else {

		dma_buffer = list_first_entry(&dev->bsource_list,
			struct comp_buffer, sink_list);
		dma_period_desc = &dma_buffer->desc.sink_period;
	}

	/* start the DMA if there is enough local free space
	   and previous DMA has completed */
	if (dma_buffer->avail > dma_period_desc->size) {
		/* do DMA transfer */
		wait_init(&hs->complete);
		dma_set_config(hd->dma, hs->chan, &hs->config);
		dma_start(hd->dma, hs->chan);
	}

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
