// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/dma.h>
#include <stdint.h>

/* allocate next free DMA channel */
static int dummy_dma_channel_get(struct dma *dma, unsigned int req_chan)
{
	return 0;
}

/* channel must not be running when this is called */
static void dummy_dma_channel_put(struct dma *dma, unsigned int channel)
{
}

static int dummy_dma_start(struct dma *dma, unsigned int channel)
{
	return 0;
}


static int dummy_dma_release(struct dma *dma, unsigned int channel)
{
	return 0;
}

static int dummy_dma_pause(struct dma *dma, unsigned int channel)
{
	return 0;
}

static int dummy_dma_stop(struct dma *dma, unsigned int channel)
{
	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int dummy_dma_status(struct dma *dma, unsigned int channel,
			 struct dma_chan_status *status,
			 uint8_t direction)
{
	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int dummy_dma_set_config(struct dma *dma, unsigned int channel,
			     struct dma_sg_config *config)
{
	return 0;
}

/* restore DMA context after leaving D3 */
static int dummy_dma_pm_context_restore(struct dma *dma)
{
	return 0;
}

/* store DMA context after leaving D3 */
static int dummy_dma_pm_context_store(struct dma *dma)
{
	return 0;
}

static int dummy_dma_set_cb(struct dma *dma, unsigned int channel, int type,
		void (*cb)(void *data, uint32_t type, struct dma_cb_data *next),
		void *data)
{
	return 0;
}

static int dummy_dma_copy(struct dma *dma, unsigned int channel, int bytes,
			  uint32_t flags)
{
	return 0;
}

static int dummy_dma_probe(struct dma *dma)
{
	return 0;
}

static int dummy_dma_remove(struct dma *dma)
{
	return 0;
}

static int dummy_dma_get_data_size(struct dma *dma, unsigned int channel,
				   uint32_t *avail, uint32_t *free)
{
	return 0;
}

const struct dma_ops dummy_dma_ops = {
	.channel_get	= dummy_dma_channel_get,
	.channel_put	= dummy_dma_channel_put,
	.start		= dummy_dma_start,
	.stop		= dummy_dma_stop,
	.pause		= dummy_dma_pause,
	.release	= dummy_dma_release,
	.copy		= dummy_dma_copy,
	.status		= dummy_dma_status,
	.set_config	= dummy_dma_set_config,
	.set_cb		= dummy_dma_set_cb,
	.pm_context_restore		= dummy_dma_pm_context_restore,
	.pm_context_store		= dummy_dma_pm_context_store,
	.probe		= dummy_dma_probe,
	.remove		= dummy_dma_remove,
	.get_data_size	= dummy_dma_get_data_size,
};
