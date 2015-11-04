/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/dma.h>
#include <reef/byt-dma.h>
#include <reef/io.h>
#include <reef/stream.h>
#include <platform/dma.h>
#include <stdint.h>
#include <string.h>

static inline int byt_dma_channel_get(struct dma *dma)
{
	
	return 0;
}

static inline void byt_dma_channel_put(struct dma *dma, int channel)
{
	
}

static inline int byt_dma_start(struct dma *dma, int channel)
{
	
	return 0;
}

static inline int byt_dma_stop(struct dma *dma, int channel)
{
	
	return 0;
}

static inline int byt_dma_drain(struct dma *dma, int channel)
{
	
	return 0;
}

static inline int byt_dma_status(struct dma *dma, int channel,
	struct dma_chan_status *status)
{
	
	return 0;
}

static inline int byt_dma_set_config(struct dma *dma, int channel,
	struct dma_chan_config *config)
{	
	return 0;
}

static inline int byt_dma_set_desc(struct dma *dma, int channel,
	struct dma_desc *desc)
{
	return 0;
}

static inline int byt_dma_pm_context_restore(struct dma *dma)
{
	return 0;
}

static inline int byt_dma_pm_context_store(struct dma *dma)
{
	return 0;
}

const struct dma_ops byt_dma_ops = {
	.channel_get	= byt_dma_channel_get,
	.channel_put	= byt_dma_channel_put,
	.start		= byt_dma_start,
	.stop		= byt_dma_stop,
	.drain		= byt_dma_drain,
	.status		= byt_dma_status,
	.set_config	= byt_dma_set_config,
	.set_desc	= byt_dma_set_desc,
	.pm_context_restore		= byt_dma_pm_context_restore,
	.pm_context_store		= byt_dma_pm_context_store,
};

