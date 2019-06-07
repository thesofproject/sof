// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/dma.h>
#include <sof/io.h>
#include <platform/dma.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <config.h>

static inline void edma_write(struct dma *dma, uint32_t reg, uint32_t value)
{
	io_reg_write(dma_base(dma) + reg, value);
}

static inline uint32_t edma_read(struct dma *dma, uint32_t reg)
{
	return io_reg_read(dma_base(dma) + reg);
}

static inline void edma_write16(struct dma *dma, uint32_t reg, uint16_t value)
{
	io_reg_write16(dma_base(dma) + reg, value);
}

static inline uint16_t edma_read16(struct dma *dma, uint32_t reg)
{
	return io_reg_read16(dma_base(dma) + reg);
}

static inline void edma_update_bits(struct dma *dma, uint32_t reg,
				    uint32_t mask, uint32_t value)
{
	io_reg_update_bits(dma_base(dma) + reg, mask, value);
}

/* acquire the specific DMA channel */
static int edma_channel_get(struct dma *dma, unsigned int req_chan)
{
	return 0;
}

/* channel must not be running when this is called */
static void edma_channel_put(struct dma *dma, unsigned int channel)
{
}

static int edma_start(struct dma *dma, unsigned int channel)
{
	return 0;
}

static int edma_release(struct dma *dma, unsigned int channel)
{
	return 0;
}

static int edma_pause(struct dma *dma, unsigned int channel)
{
	return 0;
}

static int edma_stop(struct dma *dma, unsigned int channel)
{
	return 0;
}

static int edma_status(struct dma *dma, unsigned int channel,
		       struct dma_chan_status *status, uint8_t direction)
{
	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int edma_set_config(struct dma *dma, unsigned int channel,
			     struct dma_sg_config *config)
{
	return 0;
}

/* restore DMA context after leaving D3 */
static int edma_pm_context_restore(struct dma *dma)
{
	return 0;
}

/* store DMA context after leaving D3 */
static int edma_pm_context_store(struct dma *dma)
{
	/* disable the DMA controller */
	return 0;
}

static int edma_set_cb(struct dma *dma, unsigned int channel, int type,
		void (*cb)(void *data, uint32_t type, struct dma_cb_data *next),
		void *data)
{
	return 0;
}

static int edma_probe(struct dma *dma)
{
	return 0;
}

static int edma_remove(struct dma *dma)
{
	return 0;
}

const struct dma_ops edma_ops = {
	.channel_get	= edma_channel_get,
	.channel_put	= edma_channel_put,
	.start		= edma_start,
	.stop		= edma_stop,
	.pause		= edma_pause,
	.release	= edma_release,
	.status		= edma_status,
	.set_config	= edma_set_config,
	.set_cb		= edma_set_cb,
	.pm_context_restore	= edma_pm_context_restore,
	.pm_context_store	= edma_pm_context_store,
	.probe		= edma_probe,
	.remove		= edma_remove,
};
