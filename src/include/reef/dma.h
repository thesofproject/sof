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

	void (*set_cb)(struct dma *dma, int channel,
		void (*cb)(void *data), void *data);
	
	int (*pm_context_restore)(struct dma *dma);
	int (*pm_context_store)(struct dma *dma);

	int (*probe)(struct dma *dma);
};

/* DMA platform data */
struct dma_plat_data {
	uint32_t base;
	uint16_t channels; 
	uint16_t desc_per_chan;
	uint16_t irq;
	uint16_t capabilities;
};

struct dma {
	struct dma_plat_data plat_data;
	const struct dma_ops *ops;
	void *private;
};

struct dma *dma_get(int dmac_id);

#define dma_set_drvdata(dma, data) \
	dma->private = data
#define dma_get_drvdata(dma) \
	dma->private;
#define dma_base(dma) \
	dma->plat_data.base
#define dma_irq(dma) \
	dma->plat_data.irq

/* DMA API
 * Programming flow is :-
 *
 * 1) dma_channel_get()
 * 2) dma_set_cb()
 * 3) dma_set_config()
 * 4) dma_set_desc()
 * 5) dma_start()
 *   ... DMA now running ...
 * 6) dma_stop()
 * 7) dma_channel_put()
 */

static inline int dma_channel_get(struct dma *dma)
{
	return dma->ops->channel_get(dma);
}

static inline void dma_channel_put(struct dma *dma, int channel)
{
	dma->ops->channel_put(dma, channel);
}

static inline void dma_set_cb(struct dma *dma, int channel,
	void (*cb)(void *data), void *data)
{
	dma->ops->set_cb(dma, channel, cb, data);
}

static inline int dma_start(struct dma *dma, int channel)
{
	return dma->ops->start(dma, channel);
}

static inline int dma_stop(struct dma *dma, int channel)
{
	return dma->ops->stop(dma, channel);
}

static inline int dma_drain(struct dma *dma, int channel)
{
	return dma->ops->drain(dma, channel);
}

static inline int dma_status(struct dma *dma, int channel,
	struct dma_chan_status *status)
{
	return dma->ops->status(dma, channel, status);
}

static inline int dma_set_config(struct dma *dma, int channel,
	struct dma_chan_config *config)
{
	return dma->ops->set_config(dma, channel, config);
}

static inline int dma_set_desc(struct dma *dma, int channel,
	struct dma_desc *desc)
{
	return dma->ops->set_desc(dma, channel, desc);
}

static inline int dma_pm_context_restore(struct dma *dma)
{
	return dma->ops->pm_context_restore(dma);
}

static inline int dma_pm_context_store(struct dma *dma)
{
	return dma->ops->pm_context_store(dma);
}

static inline int dma_probe(struct dma *dma)
{
	return dma->ops->probe(dma);
}

#endif
