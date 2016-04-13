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

struct dai_data {
	/* local DMA config */
	int chan;
	struct dma_sg_config config;

	int direction;
	struct dai *ssp;
	struct dma *dma;

	uint32_t dai_pos_blks;		/* position in bytes (nearest block) */

	volatile uint32_t *dai_pos;
};

/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_cb(void *data, uint32_t type)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct period_desc *dma_period_desc;
	struct comp_buffer *dma_buffer;
	struct dma_chan_status status;

	/* update local buffer position */
	dma_status(dd->dma, dd->chan, &status, dd->direction);

#if 0
	trace_comp("CDs");
#endif

	if (dd->direction == STREAM_DIRECTION_PLAYBACK) {
		dma_buffer = list_first_entry(&dev->bsource_list,
			struct comp_buffer, sink_list);
		dma_buffer->r_ptr = (void*)status.r_pos;
		dma_period_desc = &dma_buffer->desc.sink_period;
#if 0
		// TODO: move this to new trace mechanism
		trace_value((uint32_t)(dma_buffer->r_ptr - dma_buffer->addr));
#endif

		/* check for end of buffer */
		if (dma_buffer->r_ptr >= dma_buffer->end_addr)
			dma_buffer->r_ptr = dma_buffer->addr;

		/* update host position(in bytes offset) for drivers */
		dd->dai_pos_blks += dma_period_desc->size;
		if (dd->dai_pos)
			*dd->dai_pos = dd->dai_pos_blks +
				dma_buffer->r_ptr - dma_buffer->addr;

	} else {
		dma_buffer = list_first_entry(&dev->bsink_list,
			struct comp_buffer, source_list);
		dma_buffer->w_ptr = (void*)status.w_pos;
		dma_period_desc = &dma_buffer->desc.source_period;

#if 0
		// TODO: move this to new trace mechanism
		trace_value((uint32_t)(dma_buffer->w_ptr - dma_buffer->addr));
#endif

		/* check for end of buffer */
		if (dma_buffer->w_ptr >= dma_buffer->end_addr)
			dma_buffer->w_ptr = dma_buffer->addr;

		/* update host position(in bytes offset) for drivers */
		dd->dai_pos_blks += dma_period_desc->size;
		if (dd->dai_pos)
			*dd->dai_pos = dd->dai_pos_blks +
				dma_buffer->w_ptr - dma_buffer->addr;
	}

	/* recalc available buffer space */
	comp_update_buffer(dma_buffer);

	if (dd->direction == STREAM_DIRECTION_PLAYBACK) {
		/* notify pipeline that DAI needs it's buffer filled */
		pipeline_fill_buffer(dev->pipeline, dma_buffer);
	} else {
		/* notify pipeline that DAI needs it's buffer emptied */
		pipeline_empty_buffer(dev->pipeline, dma_buffer);
	}
}

static struct comp_dev *dai_new_ssp(uint32_t type, uint32_t index,
	uint8_t direction)
{
	struct comp_dev *dev;
	struct dai_data *dd;

	dev = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	dd = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*dd));
	if (dd == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		return NULL;
	}

	comp_set_drvdata(dev, dd);
	comp_set_dai_ep(dev);
	dd->ssp = dai_get(type, 2); /* TODO: get from IPC use ssp2 for MB */
	dd->dma = dma_get(DMA_ID_DMAC1);
	list_init(&dd->config.elem_list);
	dd->dai_pos = NULL;

	/* get DMA channel from DMAC1 */
	dd->chan = dma_channel_get(dd->dma);
	if (dd->chan < 0)
		goto error;

	/* set up callback */
	dma_set_cb(dd->dma, dd->chan, DMA_IRQ_TYPE_BLOCK, dai_dma_cb, dev);

	return dev;

error:
	rfree(RZONE_MODULE, RMOD_SYS, dd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
	return NULL;
}

static struct comp_dev *dai_new_hda(uint32_t type, uint32_t index,
	uint8_t direction)
{
	return 0;
}

static void dai_free(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	dma_channel_put(dd->dma, dd->chan);
	
	rfree(RZONE_MODULE, RMOD_SYS, dd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
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
	struct list_head *elist, *tlist;
	int i;

	dd->direction = params->direction;

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_DEV;
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 1;
	config->dest_dev = dd->ssp->plat_data.fifo[0].handshake;

	/* set up local and host DMA elems to reset values */
	dma_buffer = list_first_entry(&dev->bsource_list,
		struct comp_buffer, sink_list);
	dma_period_desc = &dma_buffer->desc.sink_period;
	dma_buffer->params = *params;

	/* set up cyclic list of DMA elems */
	for (i = 0; i < dma_period_desc->number; i++) {

		elem = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*elem));
		if (elem == NULL)
			goto err_unwind;

		elem->size = dma_period_desc->size;
		elem->src = (uint32_t)(dma_buffer->r_ptr) +
			i * dma_period_desc->size;

		elem->dest = dai_fifo(dd->ssp, params->direction);

		list_add_tail(&elem->list, &config->elem_list);
	}

	/* set write pointer to start of buffer */
	dma_buffer->w_ptr = dma_buffer->addr;

	return 0;

err_unwind:
	list_for_each_safe(elist, tlist, &config->elem_list) {
		elem = container_of(elist, struct dma_sg_elem, list);
		list_del(&elem->list);
		rfree(RZONE_MODULE, RMOD_SYS, elem);
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
	struct list_head *elist, *tlist;
	int i;

	dd->direction = params->direction;

	/* set up DMA configuration */
	config->direction = DMA_DIR_DEV_TO_MEM;
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 1;
	config->src_dev = dd->ssp->plat_data.fifo[1].handshake;

	/* set up local and host DMA elems to reset values */
	dma_buffer = list_first_entry(&dev->bsink_list,
		struct comp_buffer, source_list);
	dma_period_desc = &dma_buffer->desc.source_period;
	dma_buffer->params = *params;

	/* set up cyclic list of DMA elems */
	for (i = 0; i < dma_period_desc->number; i++) {

		elem = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*elem));
		if (elem == NULL)
			goto err_unwind;

		elem->size = dma_period_desc->size;
		elem->dest = (uint32_t)(dma_buffer->w_ptr) +
			i * dma_period_desc->size;
		elem->src = dai_fifo(dd->ssp, params->direction);
		list_add_tail(&elem->list, &config->elem_list);
	}

	/* set write pointer to start of buffer */
	dma_buffer->r_ptr = dma_buffer->addr;

	return 0;

err_unwind:
	list_for_each_safe(elist, tlist, &config->elem_list) {
		elem = container_of(elist, struct dma_sg_elem, list);
		list_del(&elem->list);
		rfree(RZONE_MODULE, RMOD_SYS, elem);
	}
	return -ENOMEM;
}

static int dai_params(struct comp_dev *dev,
	struct stream_params *params)
{
	if (params->direction == STREAM_DIRECTION_PLAYBACK)
		return dai_playback_params(dev, params);
	else
		return dai_capture_params(dev, params);
}

static int dai_prepare(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct comp_buffer *dma_buffer;

	if (dd->direction == STREAM_DIRECTION_PLAYBACK) {
		dma_buffer = list_first_entry(&dev->bsource_list,
			struct comp_buffer, sink_list);
		dma_buffer->r_ptr = dma_buffer->addr;

	} else {
		dma_buffer = list_first_entry(&dev->bsink_list,
			struct comp_buffer, source_list);
		dma_buffer->w_ptr = dma_buffer->addr;
	}

	dd->dai_pos_blks = 0;
	if (dd->dai_pos)
		*dd->dai_pos = 0;

	return dma_set_config(dd->dma, dd->chan, &dd->config);
}

static int dai_reset(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_sg_config *config = &dd->config;
	struct list_head *elist, *tlist;
	struct dma_sg_elem *elem;

	list_for_each_safe(elist, tlist, &config->elem_list) {
		elem = container_of(elist, struct dma_sg_elem, list);
		list_del(&elem->list);
		rfree(RZONE_MODULE, RMOD_SYS, elem);
	}
	dev->state = COMP_STATE_INIT;

	return 0;
}

/* used to pass standard and bespoke commandd (with data) to component */
static int dai_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	// TODO: wait on pause/stop/drain completions before SSP ops.

	switch (cmd) {
	case COMP_CMD_PAUSE:
		dma_pause(dd->dma, dd->chan);
		dai_trigger(dd->ssp, cmd, dd->direction);
		break;
	case COMP_CMD_STOP:
		dma_stop(dd->dma, dd->chan);
		dai_trigger(dd->ssp, cmd, dd->direction);
		dev->state = COMP_STATE_STOPPED;
		break;
	case COMP_CMD_RELEASE:
		dma_release(dd->dma, dd->chan);
		dai_trigger(dd->ssp, cmd, dd->direction);
		break;
	case COMP_CMD_START:
		dma_start(dd->dma, dd->chan);
		dai_trigger(dd->ssp, cmd, dd->direction);
		dev->state = COMP_STATE_RUNNING;
		break;
	case COMP_CMD_DRAIN:
		dma_drain(dd->dma, dd->chan);
		dai_trigger(dd->ssp, cmd, dd->direction);
		break;
	case COMP_CMD_SUSPEND:
	case COMP_CMD_RESUME:
		break;
	case COMP_CMD_IPC_MMAP_PPOS:
		dd->dai_pos = data;
		break;
	default:
		break;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dma_chan_status status;
	struct comp_buffer *dma_buffer;

	/* update host position(in bytes offset) for drivers */
	if (dd->dai_pos) {
		/* update local buffer position */
		dma_status(dd->dma, dd->chan, &status, dd->direction);

		if (dd->direction == STREAM_DIRECTION_PLAYBACK) {
			dma_buffer = list_first_entry(&dev->bsource_list,
				struct comp_buffer, sink_list);
		
			*dd->dai_pos = dd->dai_pos_blks +
				status.r_pos - (uint32_t)dma_buffer->addr;
		} else {
			dma_buffer = list_first_entry(&dev->bsink_list,
				struct comp_buffer, source_list);

			*dd->dai_pos = dd->dai_pos_blks +
				status.w_pos - (uint32_t)dma_buffer->addr;
		}
	}

	return 0;
}

static int dai_config(struct comp_dev *dev, struct dai_config *dai_config)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	return dai_set_config(dd->ssp, dai_config);
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
	},
	.caps	= {
		.source = {
			.formats	= STREAM_FORMAT_S16_LE,
			.min_rate	= 8000,
			.max_rate	= 192000,
			.min_channels	= 1,
			.max_channels	= 2,
		},
		.sink = {
			.formats	= STREAM_FORMAT_S16_LE,
			.min_rate	= 8000,
			.max_rate	= 192000,
			.min_channels	= 1,
			.max_channels	= 2,
		},
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
	},
};

void sys_comp_dai_init(void)
{
	comp_register(&comp_dai_ssp);
	comp_register(&comp_dai_hda);
}

