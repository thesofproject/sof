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
#include <reef/wait.h>

/* DMA directions */
#define DMA_DIR_MEM_TO_MEM	0	/* local memcpy */
#define DMA_DIR_HMEM_TO_LMEM	1	/* host to local memcpy */
#define DMA_DIR_LMEM_TO_HMEM	2	/* local to host memcpy */
#define DMA_DIR_MEM_TO_DEV	3
#define DMA_DIR_DEV_TO_MEM	4
#define DMA_DIR_DEV_TO_DEV	5

/* DMA IRQ types */
#define DMA_IRQ_TYPE_BLOCK	(1 << 0)
#define DMA_IRQ_TYPE_LLIST	(1 << 1)


/* We will use this macro in cb handler to inform dma that
 * we need to stop the reload for special purpose
 */
#define DMA_RELOAD_END	0
#define DMA_RELOAD_LLI	0xFFFFFFFF

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
	uint32_t burst_elems;
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

	int (*channel_get)(struct dma *dma, int req_channel);
	void (*channel_put)(struct dma *dma, int channel);

	int (*start)(struct dma *dma, int channel);
	int (*stop)(struct dma *dma, int channel);
	int (*copy)(struct dma *dma, int channel, int bytes);
	int (*pause)(struct dma *dma, int channel);
	int (*release)(struct dma *dma, int channel);
	int (*status)(struct dma *dma, int channel,
		struct dma_chan_status *status, uint8_t direction);

	int (*set_config)(struct dma *dma, int channel,
		struct dma_sg_config *config);

	void (*set_cb)(struct dma *dma, int channel, int type,
		void (*cb)(void *data, uint32_t type, struct dma_sg_elem *next),
		void *data);

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
	uint32_t chan_size;
	void *drv_plat_data;
};

struct dma {
	struct dma_plat_data plat_data;
	spinlock_t lock;
	const struct dma_ops *ops;
	void *private;
};

struct dma_int {
	struct dma *dma;
	uint32_t channel;
	uint32_t irq;
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
#define dma_chan_size(dma) \
	dma->plat_data.chan_size
#define dma_chan_base(dma, chan) \
	(dma->plat_data.base + chan * dma->plat_data.chan_size)

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

static inline int dma_channel_get(struct dma *dma, int req_channel)
{
	return dma->ops->channel_get(dma, req_channel);
}

static inline void dma_channel_put(struct dma *dma, int channel)
{
	dma->ops->channel_put(dma, channel);
}

static inline void dma_set_cb(struct dma *dma, int channel, int type,
	void (*cb)(void *data, uint32_t type, struct dma_sg_elem *next), void *data)
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

static inline int dma_copy(struct dma *dma, int channel, int bytes)
{
	return dma->ops->copy(dma, channel, bytes);
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

/* generic DMA DSP <-> Host copier */

struct dma_copy {
	int chan;
	struct dma *dmac;
	completion_t complete;
};

/* init dma copy context */
int dma_copy_new(struct dma_copy *dc, int dmac);

/* free dma copy context resources */
static inline void dma_copy_free(struct dma_copy *dc)
{
	dma_channel_put(dc->dmac, dc->chan);
}

/* DMA copy data from host to DSP */
int dma_copy_from_host(struct dma_copy *dc, struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);
int dma_copy_from_host_nowait(struct dma_copy *dc, struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);

/* DMA copy data from DSP to host */
int dma_copy_to_host(struct dma_copy *dc, struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);
int dma_copy_to_host_nowait(struct dma_copy *dc, struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);

#endif
