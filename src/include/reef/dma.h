/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_DMA_H__
#define __INCLUDE_DMA_H__

#include <stdint.h>

/* DMA directions */
#define DMA_DIR_DEV_TO_MEM	0
#define DMA_DIR_MEM_TO_DEV	1
#define DMA_DIR_MEM_TO_MEM	2

/* DMA status flags - only DRAINING can be combined with others. */
#define DMA_STATUS_FREE		0
#define DMA_STATUS_IDLE		1
#define DMA_STATUS_RUNNING	2
#define DMA_STATUS_DRAINING	4

struct dma;
struct dma_desc;

struct dma_desc {
	uint32_t src;
	uint32_t dest;
	uint32_t size;
	struct dma_desc *next;
};

struct dma_chan_config {
	uint16_t direction;
	uint16_t src_width;
	uint16_t dest_width;
	uint16_t irq;
	uint16_t burst_size;
};

struct dma_chan_status {
	uint16_t state;
	uint16_t flags;
	uint32_t position;
	uint32_t timestamp;
};

/* DMA operations */ 
struct dma_ops {

	int (*channel_get)(struct dma *dma);
	void (*channel_put)(struct dma *dma, int channel);

	int (*start)(struct dma *dma, int channel);
	int (*stop)(struct dma *dma, int channel);
	int (*drain)(struct dma *dma, int channel);
	int (*status)(struct dma *dma, int channel,
		struct dma_chan_status *status);
	
	int (*set_desc)(struct dma *dma, int channel, struct dma_desc *desc);
	int (*set_config)(struct dma *dma, int channel,
		struct dma_chan_config *config);
	
	int (*pm_context_restore)(struct dma *dma);
	int (*pm_context_store)(struct dma *dma);
};

struct dma {
	uint32_t base;
	uint16_t channels; 
	uint16_t desc_per_chan;
	const struct dma_ops *ops;
	void *data;
};

struct dma *dma_get(int dmac_id);

/* DMA API
 * Programming flow is :-
 *
 * 1) dma_channel_get()
 * 2) dma_set_config()
 * 3) dma_set_desc()
 * 4) dma_start()
 *   ... DMA now running ...
 * 5) dma_stop()
 * 6) dma_channel_put()
 */

static inline int dma_channel_get(struct dma *dma)
{
	if (dma->ops->channel_get)
		return dma->ops->channel_get(dma);
	return 0;
}

static inline void dma_channel_put(struct dma *dma, int channel)
{
	if (dma->ops->channel_put)
		dma->ops->channel_put(dma, channel);
}

static inline int dma_start(struct dma *dma, int channel)
{
	if (dma->ops->start)
		return dma->ops->start(dma, channel);
	return 0;
}

static inline int dma_stop(struct dma *dma, int channel)
{
	if (dma->ops->stop)
		return dma->ops->stop(dma, channel);
	return 0;
}

static inline int dma_drain(struct dma *dma, int channel)
{
	if (dma->ops->drain)
		return dma->ops->drain(dma, channel);
	return 0;
}

static inline int dma_status(struct dma *dma, int channel,
	struct dma_chan_status *status)
{
	if (dma->ops->status)
		return dma->ops->status(dma, channel, status);
	return 0;
}

static inline int dma_set_config(struct dma *dma, int channel,
	struct dma_chan_config *config)
{
	if (dma->ops->set_config)
		return dma->ops->set_config(dma, channel, config);
	return 0;
}

static inline int dma_set_desc(struct dma *dma, int channel,
	struct dma_desc *desc)
{
	if (dma->ops->set_desc)
		return dma->ops->set_desc(dma, channel, desc);
	return 0;
}

static inline int dma_pm_context_restore(struct dma *dma)
{
	if (dma->ops->pm_context_restore)
		return dma->ops->pm_context_restore(dma);
	return 0;
}

static inline int dma_pm_context_store(struct dma *dma)
{
	if (dma->ops->pm_context_store)
		return dma->ops->pm_context_store(dma);
	return 0;
}

#endif
