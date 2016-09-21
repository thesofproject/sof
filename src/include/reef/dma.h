/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
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
#define DMA_STATUS_PAUSING	7
#define DMA_STATUS_STOPPING	8

/* DMA IRQ types */
#define DMA_IRQ_TYPE_BLOCK	(1 << 0)
#define DMA_IRQ_TYPE_LLIST	(1 << 1)

struct dma;

struct dma_sg_elem {
	uint32_t src;
	uint32_t dest;
	uint32_t size;
	struct list_item list;
};

/* DMA physical SG params */
struct dma_sg_config {
	uint32_t src_width;
	uint32_t dest_width;
	uint32_t direction;
	uint32_t src_dev;
	uint32_t dest_dev;
	uint32_t cyclic;		/* circular buffer */
	struct list_item elem_list;	/* list of dma_sg elems */
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
	int (*stop)(struct dma *dma, int channel, int drain);
	int (*pause)(struct dma *dma, int channel);
	int (*release)(struct dma *dma, int channel);
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

static inline int dma_stop(struct dma *dma, int channel, int drain)
{
	return dma->ops->stop(dma, channel, drain);
}

static inline int dma_pause(struct dma *dma, int channel)
{
	return dma->ops->pause(dma, channel);
}

static inline int dma_release(struct dma *dma, int channel)
{
	return dma->ops->release(dma, channel);
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
	struct list_item *plist;
	uint32_t size = 0;

	list_for_item(plist, &sg->elem_list) {

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
