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
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/dai.h>
#include <reef/alloc.h>
#include <reef/dma.h>
#include <reef/stream.h>
#include <reef/audio/component.h>
#include <platform/dma.h>

struct dai_data {
	struct dma *dma;
	int chan;
	struct dai *ssp;
};

/* this is called by DMA driver every time descriptor has completed */
static void dai_dma_cb(void *data)
{
	/* TODO: update the buffer rx/tx pointers and avail */
}

static struct comp_dev *dai_new_ssp(struct comp_desc *desc)
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

	dd->ssp = dai_get(COMP_UUID(COMP_VENDOR_INTEL, desc->id));

	comp_set_drvdata(dev, dd);

	dd->dma = dma_get(DMA_ID_DMAC1);

	/* get DMA channel from DMAC1 */
	dd->chan = dma_channel_get(dd->dma);
	if (dd->chan < 0)
		goto error;

	/* set up callback */
	dma_set_cb(dd->dma, dd->chan, dai_dma_cb, dev);
	dev->id = desc->id;
	return dev;

error:
	rfree(RZONE_MODULE, RMOD_SYS, dd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
	return NULL;
}

static struct comp_dev *dai_new_hda(struct comp_desc *desc)
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
static int dai_params(struct comp_dev *dev, struct stream_params *params)
{
#if 0
		/* set up DMA configuration */
	config.direction = DMA_DIR_MEM_TO_MEM;
	config.src_width = 4;
	config.dest_width = 4;
	//config.irq = ??  do we need this and burst size ??
	//config.burst_size = ??
	ret = dma_set_config(dma, chan, &config);
	if (ret < 0)
		goto out;

	/* set up DMA desciptor */
	desc.dest = (uint32_t)_ipc->page_table;
	desc.src = ring->ring_pt_address;
	desc.size = IPC_INTEL_PAGE_TABLE_SIZE;
	desc.next = NULL;
	ret = dma_set_desc(dma, chan, &desc);
	if (ret < 0)
		goto out;
#endif
	return 0;
}

static int dai_prepare(struct comp_dev *dev)
{
	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int dai_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct dai_data *dd = comp_get_drvdata(dev);

	switch (cmd) {
	case PIPELINE_CMD_PAUSE:
	case PIPELINE_CMD_STOP:
		dai_trigger(dd->ssp, cmd, dev->is_playback);
		dma_stop(dd->dma, dd->chan);
		break;
	case PIPELINE_CMD_RELEASE:
	case PIPELINE_CMD_START:
		dma_start(dd->dma, dd->chan);
		dai_trigger(dd->ssp, cmd, dev->is_playback);
		break;
	case PIPELINE_CMD_DRAIN:
		dai_trigger(dd->ssp, cmd, dev->is_playback);
		dma_drain(dd->dma, dd->chan);
		break;
	case PIPELINE_CMD_SUSPEND:
	case PIPELINE_CMD_RESUME:
	default:
		return -EINVAL;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int dai_copy(struct comp_dev *dev)
{
	/* nothing todo here since DMA does our copies */
	return 0;
}

static struct comp_driver comp_dai_ssp = {
	.uuid	= COMP_UUID(COMP_VENDOR_INTEL, COMP_TYPE_DAI_SSP),
	.ops	= {
		.new		= dai_new_ssp,
		.free		= dai_free,
		.params		= dai_params,
		.cmd		= dai_cmd,
		.copy		= dai_copy,
		.prepare	= dai_prepare,
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
	.uuid	= COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_DAI_HDA),
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

