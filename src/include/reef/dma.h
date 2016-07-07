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
#include <reef/list.h>
#include <reef/lock.h>
#include <reef/reef.h>

/* DMA directions */
#define DMA_DIR_MEM_TO_MEM	0
#define DMA_DIR_MEM_TO_DEV	1
#define DMA_DIR_DEV_TO_MEM	2
#define DMA_DIR_DEV_TO_DEV	3

/* DMA status flags */
#define DMA_STATUS_FREE		0
#define DMA_STATUS_IDLE		1
#define DMA_STATUS_RUNNING	2
#define DMA_STATUS_DRAINING	4
#define DMA_STATUS_CLOSING	5
#define DMA_STATUS_PAUSED	6

/* DMA IRQ types */
#define DMA_IRQ_TYPE_BLOCK	(1 << 0)
#define DMA_IRQ_TYPE_LLIST	(1 << 1)

struct dma;

struct dma_sg_elem {
	uint32_t src;
	uint32_t dest;
	uint32_t size;
	struct list_head list;
};

/* DMA physical SG params */
struct dma_sg_config {	
	uint32_t src_width;
	uint32_t dest_width;
	uint32_t direction;
	uint32_t src_dev;
	uint32_t dest_dev;
	uint32_t cyclic;		/* circular buffer */
	struct list_head elem_list;	/* list of dma_sg elems */
};

struct dma_chan_status {
	uint32_t state;
	uint32_t flags;
	uint32_t w_pos;
	uint32_t r_pos;
	uint32_t timestamp;
};

/* DMA operations */ 
struct dma_ops {

	int (*channel_get)(struct dma *dma);
	void (*channel_put)(struct dma *dma, int channel);

	int (*start)(struct dma *dma, int channel);
	int (*stop)(struct dma *dma, int channel);
	int (*pause)(struct dma *dma, int channel);
	int (*release)(struct dma *dma, int channel);
	int (*drain)(struct dma *dma, int channel);
	int (*status)(struct dma *dma, int channel,
		struct dma_chan_status *status, uint8_t direction);
	
	int (*set_config)(struct dma *dma, int channel,
		struct dma_sg_config *config);

	void (*set_cb)(struct dma *dma, int channel, int type,
		void (*cb)(void *data, uint32_t type), void *data);
	
	int (*pm_context_restore)(struct dma *dma);
	int (*pm_context_store)(struct dma *dma);

	int (*probe)(struct dma *dma);
};

/* DMA platform data */
struct dma_plat_data {
	uint32_t id;
	uint32_t base;
	uint32_t channels; 
	uint32_t irq;
	void *drv_plat_data;
};

struct dma {
	struct dma_plat_data plat_data;
	spinlock_t lock;
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
 * 4) dma_start()
 *   ... DMA now running ...
 * 5) dma_stop()
 * 6) dma_channel_put()
 */

static inline int dma_channel_get(struct dma *dma)
{
	return dma->ops->channel_get(dma);
}

static inline void dma_channel_put(struct dma *dma, int channel)
{
	dma->ops->channel_put(dma, channel);
}

static inline void dma_set_cb(struct dma *dma, int channel, int type,
	void (*cb)(void *data, uint32_t type), void *data)
{
	dma->ops->set_cb(dma, channel, type, cb, data);
}

static inline int dma_start(struct dma *dma, int channel)
{
	return dma->ops->start(dma, channel);
}

static inline int dma_stop(struct dma *dma, int channel)
{
	return dma->ops->stop(dma, channel);
}

static inline int dma_pause(struct dma *dma, int channel)
{
	return dma->ops->pause(dma, channel);
}

static inline int dma_release(struct dma *dma, int channel)
{
	return dma->ops->release(dma, channel);
}

static inline int dma_drain(struct dma *dma, int channel)
{
	return dma->ops->drain(dma, channel);
}

static inline int dma_status(struct dma *dma, int channel,
	struct dma_chan_status *status, uint8_t direction)
{
	return dma->ops->status(dma, channel, status, direction);
}

static inline int dma_set_config(struct dma *dma, int channel,
	struct dma_sg_config *config)
{
	return dma->ops->set_config(dma, channel, config);
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

/* get the size of SG buffer */
static inline uint32_t dma_sg_get_size(struct dma_sg_config *sg)
{
	struct dma_sg_elem *sg_elem;
	struct list_head *plist;
	uint32_t size = 0;

	list_for_each(plist, &sg->elem_list) {

		sg_elem = container_of(plist, struct dma_sg_elem, list);
		size += sg_elem->size;
	}

	return size;
}

/* DMA copy data from host to DSP */
int dma_copy_from_host(struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);

/* DMA copy data from DSP to host */
int dma_copy_to_host(struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);

#endif
