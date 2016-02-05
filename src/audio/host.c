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
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/trace.h>
#include <reef/dma.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <platform/dma.h>

struct host_data {
	struct dma *dma;
	int chan;
};

#if 0
/* this is called by DMA driver every time descriptor has completed */
static void host_dma_cb(void *data, uint32_t type)
{
	/* TODO: update the buffer rx/tx pointers and avail */
}
#endif

static struct comp_dev *host_new(struct comp_desc *desc)
{
	struct comp_dev *dev;
	struct host_data *hd;

	dev = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	hd = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*hd));
	if (hd == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		return NULL;
	}

	comp_set_drvdata(dev, hd);

	hd->dma = dma_get(DMA_ID_DMAC0);

	/* get DMA channel from DMAC0 */
//	hd->chan = dma_channel_get(hd->dma);
//	if (hd->chan < 0)
//		goto error;

	/* set up callback */
//	dma_set_cb(hd->dma, hd->chan, host_dma_cb, dev);
	return dev;

//error:
	rfree(RZONE_MODULE, RMOD_SYS, hd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
	return NULL;
}

static void host_free(struct comp_dev *dev)
{
	struct host_data *hd = comp_get_drvdata(dev);

	dma_channel_put(hd->dma, hd->chan);
	rfree(RZONE_MODULE, RMOD_SYS, hd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
}

/* configure the DMA params and descriptors for host buffer IO */
static int host_params(struct comp_dev *dev, struct stream_params *params)
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

static int host_prepare(struct comp_dev *dev)
{
	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int host_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct host_data *hd = comp_get_drvdata(dev);

	switch (cmd) {
	case PIPELINE_CMD_PAUSE:
	case PIPELINE_CMD_STOP:
		dma_stop(hd->dma, hd->chan);
		break;
	case PIPELINE_CMD_RELEASE:
	case PIPELINE_CMD_START:
		dma_start(hd->dma, hd->chan);
		break;
	case PIPELINE_CMD_DRAIN:
		dma_drain(hd->dma, hd->chan);
		break;
	case PIPELINE_CMD_SUSPEND:
	case PIPELINE_CMD_RESUME:
	default:
		return -EINVAL;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int host_copy(struct comp_dev *dev)
{
	/* nothing todo here since DMA does our copies */
	return 0;
}

struct comp_driver comp_host = {
	.uuid	= COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_HOST),
	.ops	= {
		.new		= host_new,
		.free		= host_free,
		.params		= host_params,
		.cmd		= host_cmd,
		.copy		= host_copy,
		.prepare	= host_prepare,
	},
};

void sys_comp_host_init(void)
{
	comp_register(&comp_host);
}
