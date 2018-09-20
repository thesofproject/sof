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

/**
 * \file include/sof/dma.h
 * \brief DMA Drivers definition
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_DMA_H__
#define __INCLUDE_DMA_H__

#include <stdint.h>
#include <sof/list.h>
#include <sof/lock.h>
#include <sof/sof.h>
#include <sof/wait.h>
#include <sof/bit.h>
#include <arch/atomic.h>

/** \addtogroup sof_dma_drivers DMA Drivers
 *  DMA Drivers API specification.
 *  @{
 */

/* DMA direction bitmasks used to define DMA copy direction */
#define DMA_DIR_MEM_TO_MEM	BIT(0) /**< local memory copy */
#define DMA_DIR_HMEM_TO_LMEM	BIT(1) /**< host memory to local mem copy */
#define DMA_DIR_LMEM_TO_HMEM	BIT(2) /**< local mem to host mem copy */
#define DMA_DIR_MEM_TO_DEV	BIT(3) /**< local mem to dev copy */
#define DMA_DIR_DEV_TO_MEM	BIT(4) /**< dev to local mem copy */
#define DMA_DIR_DEV_TO_DEV	BIT(5) /**< dev to dev copy */

/* DMA capabilities bitmasks used to define the type of DMA */
#define DMA_CAP_HDA		BIT(0) /**< HDA DMA */
#define DMA_CAP_GP_LP		BIT(1) /**< GP LP DMA */
#define DMA_CAP_GP_HP		BIT(2) /**< GP HP DMA */

/* DMA dev type bitmasks used to define the type of DMA */

#define DMA_DEV_HOST		BIT(0) /**< connectable to host */
#define DMA_DEV_HDA		BIT(1) /**< connectable to HD/A link */
#define DMA_DEV_SSP		BIT(2) /**< connectable to SSP fifo */
#define DMA_DEV_DMIC		BIT(3) /**< connectable to DMIC fifo */

/* DMA access privilege flag */
#define DMA_ACCESS_EXCLUSIVE	1
#define DMA_ACCESS_SHARED	0

/* DMA IRQ types */
#define DMA_IRQ_TYPE_BLOCK	BIT(0)
#define DMA_IRQ_TYPE_LLIST	BIT(1)

/* DMA copy flags */
#define DMA_COPY_PRELOAD	BIT(0)

/* We will use this macro in cb handler to inform dma that
 * we need to stop the reload for special purpose
 */
#define DMA_RELOAD_END	0
#define DMA_RELOAD_LLI	0xFFFFFFFF

struct dma;

/**
 *  \brief Element of SG list (as array item).
 */
struct dma_sg_elem {
	uint32_t src;	/**< source address */
	uint32_t dest;	/**< destination address */
	uint32_t size;	/**< size (in bytes) */
};

/**
 * \brief SG elem array.
 */
struct dma_sg_elem_array {
	uint32_t count;			/**< number of elements in elems */
	struct dma_sg_elem *elems;	/**< elements */
};

/* DMA physical SG params */
struct dma_sg_config {
	uint32_t src_width;			/* in bytes */
	uint32_t dest_width;			/* in bytes */
	uint32_t burst_elems;
	uint32_t direction;
	uint32_t src_dev;
	uint32_t dest_dev;
	uint32_t cyclic;			/* circular buffer */
	struct dma_sg_elem_array elem_array;	/* array of dma_sg elems */
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
	int (*copy)(struct dma *dma, int channel, int bytes, uint32_t flags);
	int (*pause)(struct dma *dma, int channel);
	int (*release)(struct dma *dma, int channel);
	int (*status)(struct dma *dma, int channel,
		struct dma_chan_status *status, uint8_t direction);

	int (*set_config)(struct dma *dma, int channel,
		struct dma_sg_config *config);

	int (*set_cb)(struct dma *dma, int channel, int type,
		void (*cb)(void *data, uint32_t type, struct dma_sg_elem *next),
		void *data);

	int (*pm_context_restore)(struct dma *dma);
	int (*pm_context_store)(struct dma *dma);

	int (*probe)(struct dma *dma);
};

/* DMA platform data */
struct dma_plat_data {
	uint32_t id;
	uint32_t dir; /* bitmask of supported copy directions */
	uint32_t caps; /* bitmask of supported capabilities */
	uint32_t devs; /* bitmask of supported devs */
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
	atomic_t num_channels_busy; /* number of busy channels */
	void *private;
	uint32_t private_size;
};

struct dma *dma_get(uint32_t dir, uint32_t caps, uint32_t dev, uint32_t flags);

/* initialize all platform DMAC's */
int dmac_init(void);

#define dma_set_drvdata(dma, data) \
	dma->private = data; \
	dma->private_size = sizeof(*data)
#define dma_get_drvdata(dma) \
	dma->private;
#define dma_base(dma) \
	dma->plat_data.base
#define dma_irq(dma, cpu) \
	(dma->plat_data.irq + (cpu << SOF_IRQ_CPU_SHIFT))
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

static inline int dma_set_cb(struct dma *dma, int channel, int type,
	void (*cb)(void *data, uint32_t type, struct dma_sg_elem *next), void *data)
{
	return dma->ops->set_cb(dma, channel, type, cb, data);
}

static inline int dma_start(struct dma *dma, int channel)
{
	return dma->ops->start(dma, channel);
}

static inline int dma_stop(struct dma *dma, int channel)
{
	return dma->ops->stop(dma, channel);
}

static inline int dma_copy(struct dma *dma, int channel, int bytes,
			   uint32_t flags)
{
	return dma->ops->copy(dma, channel, bytes, flags);
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

static inline void dma_sg_init(struct dma_sg_elem_array *ea)
{
	ea->count = 0;
	ea->elems = NULL;
}

int dma_sg_alloc(struct dma_sg_elem_array *ea, uint32_t direction,
		 uint32_t buffer_count, uint32_t buffer_bytes,
		 uintptr_t dma_buffer_addr, uintptr_t external_addr);

void dma_sg_free(struct dma_sg_elem_array *ea);

static inline void dma_sg_cache_wb_inv(struct dma_sg_elem_array *ea)
{
	dcache_writeback_invalidate_region(ea->elems,
					   ea->count *
					   sizeof(struct dma_sg_elem));
}

static inline void dma_sg_cache_inv(struct dma_sg_elem_array *ea)
{
	dcache_invalidate_region(ea->elems,
				 ea->count * sizeof(struct dma_sg_elem));
}

/**
 * \brief Get the total size of SG buffer
 *
 * \param ea Array of SG elements.
 * \return Size of the buffer.
 */
static inline uint32_t dma_sg_get_size(struct dma_sg_elem_array *ea)
{
	int i;
	uint32_t size = 0;

	for (i = 0 ; i < ea->count; i++)
		size += ea->elems[i].size;

	return size;
}

/* generic DMA DSP <-> Host copier */

struct dma_copy {
	int chan;
	struct dma *dmac;
	completion_t complete;
};

/* init dma copy context */
int dma_copy_new(struct dma_copy *dc);

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
int dma_copy_to_host_nowait(struct dma_copy *dc, struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);

int dma_copy_set_stream_tag(struct dma_copy *dc, uint32_t stream_tag);

/** @}*/

#endif
