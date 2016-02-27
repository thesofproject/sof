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
#include <platform/dma.h>

#define DAI_PLAYBACK_STREAM	0
#define DAI_CAPTURE_STREAM	1

struct dai_stream {
	/* local DMA config */
	int chan;
	struct dma_sg_config config;
	completion_t complete;
};

struct dai_data {
	struct dai_stream s[2];
	struct dai *ssp;
	struct dma *dma;
};

/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_playback_cb(void *data, uint32_t type)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_stream *ds = &dd->s[DAI_PLAYBACK_STREAM];
	struct comp_buffer *dma_buffer;
	struct dma_chan_status status;

	dma_buffer = list_first_entry(&dev->bsink_list,
		struct comp_buffer, source_list);

	/* update local buffer position */
	dma_status(dd->dma, ds->chan, &status);
	dma_buffer->r_ptr = (void*)status.position;

	// TODO: update presentation position for host

	/* recalc available buffer space */
	comp_update_avail(dma_buffer);

	/* let any waiters know we have completed */
	wait_completed(&ds->complete);
}

static void dai_dma_capture_cb(void *data, uint32_t type)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_stream *ds = &dd->s[DAI_CAPTURE_STREAM];
	struct comp_buffer *dma_buffer;
	struct dma_chan_status status;
	
	dma_buffer = list_first_entry(&dev->bsink_list,
		struct comp_buffer, source_list);

	/* update local buffer position */
	dma_status(dd->dma, ds->chan, &status);
	dma_buffer->w_ptr = (void*)status.position;

	/* recalc available buffer space */
	comp_update_avail(dma_buffer);

	/* let any waiters know we have completed */
	wait_completed(&ds->complete);
}

static struct comp_dev *dai_new_ssp(uint32_t type, uint32_t index)
{
	struct comp_dev *dev;
	struct dai_data *dd;
	struct dai_stream *ds;

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
	dd->ssp = dai_get(type, index);
	dd->dma = dma_get(DMA_ID_DMAC1);

	ds = &dd->s[DAI_PLAYBACK_STREAM];
	list_init(&ds->config.elem_list);

	/* get DMA channel from DMAC1 */
	ds->chan = dma_channel_get(dd->dma);
	if (ds->chan < 0)
		goto error;

	/* set up callback */
	dma_set_cb(dd->dma, ds->chan, dai_dma_playback_cb, dev);

	ds = &dd->s[DAI_CAPTURE_STREAM];
	list_init(&ds->config.elem_list);

	/* get DMA channel from DMAC1 */
	ds->chan = dma_channel_get(dd->dma);
	if (ds->chan < 0)
		goto error;

	/* set up callback */
	dma_set_cb(dd->dma, ds->chan, dai_dma_capture_cb, dev);

	return dev;

error:
	rfree(RZONE_MODULE, RMOD_SYS, dd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
	return NULL;
}

static struct comp_dev *dai_new_hda(uint32_t type, uint32_t index)
{
	return 0;
}

static void dai_free(struct comp_dev *dev)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_stream *ds;

	ds = &dd->s[DAI_PLAYBACK_STREAM];
	dma_channel_put(dd->dma, ds->chan);
	
	ds = &dd->s[DAI_CAPTURE_STREAM];
	dma_channel_put(dd->dma, ds->chan);
	
	rfree(RZONE_MODULE, RMOD_SYS, dd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
}

/* set component audio SSP and DMA configuration */
static int dai_playback_params(struct comp_dev *dev,
	struct stream_params *params)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_stream *ds = &dd->s[DAI_PLAYBACK_STREAM];
	struct dma_sg_config *config = &ds->config;
	struct dma_sg_elem *elem;
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;
	struct list_head *elist, *tlist;
	int i;

	/* set up DMA configuration */
	config->direction = DMA_DIR_MEM_TO_DEV;
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 1;

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

		list_add(&elem->list, &config->elem_list);
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
	struct dai_stream *ds = &dd->s[DAI_CAPTURE_STREAM];
	struct dma_sg_config *config = &ds->config;
	struct dma_sg_elem *elem;
	struct comp_buffer *dma_buffer;
	struct period_desc *dma_period_desc;
	struct list_head *elist, *tlist;
	int i;

	/* set up DMA configuration */
	config->direction = DMA_DIR_DEV_TO_MEM;
	config->src_width = sizeof(uint32_t);
	config->dest_width = sizeof(uint32_t);
	config->cyclic = 1;

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
		list_add(&elem->list, &config->elem_list);
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

static int dai_prepare(struct comp_dev *dev, struct stream_params *params)
{
	return 0;
}

static int dai_reset(struct comp_dev *dev, struct stream_params *params)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_stream *ds = &dd->s[params->direction];
	struct dma_sg_config *config = &ds->config;
	struct list_head *elist, *tlist;
	struct dma_sg_elem *elem;

	list_for_each_safe(elist, tlist, &config->elem_list) {
		elem = container_of(elist, struct dma_sg_elem, list);
		list_del(&elem->list);
		rfree(RZONE_MODULE, RMOD_SYS, elem);
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int dai_cmd(struct comp_dev *dev, struct stream_params *params,
	int cmd, void *data)
{
	struct dai_data *dd = comp_get_drvdata(dev);
	struct dai_stream *ds = &dd->s[params->direction];

	// TODO: wait on pause/stop/drain completions before SSP ops.

	switch (cmd) {
	case PIPELINE_CMD_PAUSE:
		dma_pause(dd->dma, ds->chan);
		dai_trigger(dd->ssp, cmd, params);
		break;
	case PIPELINE_CMD_STOP:
		dma_stop(dd->dma, ds->chan);
		dai_trigger(dd->ssp, cmd, params);
		break;
	case PIPELINE_CMD_RELEASE:
		dma_release(dd->dma, ds->chan);
		dai_trigger(dd->ssp, cmd, params);
		break;
	case PIPELINE_CMD_START:
		dma_start(dd->dma, ds->chan);
		dai_trigger(dd->ssp, cmd, params);
		break;
	case PIPELINE_CMD_DRAIN:
		dma_drain(dd->dma, ds->chan);
		dai_trigger(dd->ssp, cmd, params);
		break;
	case PIPELINE_CMD_SUSPEND:
	case PIPELINE_CMD_RESUME:
	default:
		return -EINVAL;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *dev, struct stream_params *params)
{
	/* nothing todo here since DMA does our copies */
	return 0;
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

