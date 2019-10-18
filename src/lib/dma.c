// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/atomic.h>
#include <sof/audio/buffer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/dma.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

struct dma_info {
	struct dma *dma_array;
	size_t num_dmas;
};

static struct dma_info lib_dma = {
	.dma_array = NULL,
	.num_dmas = 0
};

void dma_install(struct dma *dma_array, size_t num_dmas)
{
	lib_dma.dma_array = dma_array;
	lib_dma.num_dmas = num_dmas;
}

struct dma *dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags)
{
	int users, ret;
	int min_users = INT32_MAX;
	struct dma *d = NULL, *dmin = NULL;

	if (!lib_dma.num_dmas) {
		trace_error(TRACE_CLASS_DMA, "dma_get(): No DMACs installed");
		return NULL;
	}

	/* find DMAC with free channels that matches request */
	for (d = lib_dma.dma_array; d < lib_dma.dma_array + lib_dma.num_dmas;
	     d++) {
		/* skip if this DMAC does not support the requested dir */
		if (dir && (d->plat_data.dir & dir) == 0)
			continue;

		/* skip if this DMAC does not support the requested caps */
		if (cap && (d->plat_data.caps & cap) == 0)
			continue;

		/* skip if this DMAC does not support the requested dev */
		if (dev && (d->plat_data.devs & dev) == 0)
			continue;

		/* skip if this DMAC has 1 user per avail channel */
		/* TODO: this should be fixed in dai.c to allow more users */
		if (d->sref >= d->plat_data.channels)
			continue;

		/* if exclusive access is requested */
		if (flags & DMA_ACCESS_EXCLUSIVE) {
			/* ret DMA with no users */
			if (!d->sref) {
				dmin = d;
				break;
			}
		} else {
			/* get number of users for this DMAC*/
			users = d->sref;

			/* pick DMAC with the least num of users */
			if (users < min_users) {
				dmin = d;
				min_users = users;
			}
		}
	}

	if (!dmin) {
		trace_error(TRACE_CLASS_DMA, "No DMAC dir %d caps 0x%x "
			    "dev 0x%x flags 0x%x", dir, cap, dev, flags);

		for (d = lib_dma.dma_array;
		     d < lib_dma.dma_array + lib_dma.num_dmas;
		     d++) {
			trace_error(TRACE_CLASS_DMA, " DMAC ID %d users %d "
				    "busy channels %d", d->plat_data.id,
				    d->sref,
				    atomic_read(&d->num_channels_busy));
			trace_error(TRACE_CLASS_DMA, "  caps 0x%x dev 0x%x",
				    d->plat_data.caps, d->plat_data.devs);
		}
		return NULL;
	}

	/* return DMAC */
	tracev_event(TRACE_CLASS_DMA, "dma_get(), dma-probe id = %d",
		     dmin->plat_data.id);

	/* Shared DMA controllers with multiple channels
	 * may be requested many times, let the probe()
	 * do on-first-use initialization.
	 */
	spin_lock(dmin->lock);

	ret = 0;
	if (!dmin->sref) {
		ret = dma_probe(dmin);
		if (ret < 0) {
			trace_error(TRACE_CLASS_DMA,
				    "dma_get() error: dma-probe failed"
				    " id = %d, ret = %d",
				    dmin->plat_data.id, ret);
		}
	}
	if (!ret)
		dmin->sref++;

	trace_event(TRACE_CLASS_DMA, "dma_get() ID %d sref = %d "
		    "busy channels %d", dmin->plat_data.id, dmin->sref,
		    atomic_read(&dmin->num_channels_busy));

	spin_unlock(dmin->lock);
	return !ret ? dmin : NULL;
}

void dma_put(struct dma *dma)
{
	int ret;

	spin_lock(dma->lock);
	if (--dma->sref == 0) {
		ret = dma_remove(dma);
		if (ret < 0) {
			trace_error(TRACE_CLASS_DMA,
				    "dma_put() error: dma_remove() failed id"
				    " = %d, ret = %d", dma->plat_data.id, ret);
		}
	}
	trace_event(TRACE_CLASS_DMA, "dma_put(), dma = %p, sref = %d",
		   (uintptr_t)dma, dma->sref);
	spin_unlock(dma->lock);
}

int dma_sg_alloc(struct dma_sg_elem_array *elem_array,
		 int zone,
		 uint32_t direction,
		 uint32_t buffer_count, uint32_t buffer_bytes,
		 uintptr_t dma_buffer_addr, uintptr_t external_addr)
{
	int i;

	elem_array->elems = rzalloc(zone, SOF_MEM_CAPS_RAM,
				    sizeof(struct dma_sg_elem) * buffer_count);
	if (!elem_array->elems)
		return -ENOMEM;

	for (i = 0; i < buffer_count; i++) {
		elem_array->elems[i].size = buffer_bytes;
		// TODO: may count offsets once
		switch (direction) {
		case DMA_DIR_MEM_TO_DEV:
		case DMA_DIR_LMEM_TO_HMEM:
			elem_array->elems[i].src = dma_buffer_addr;
			elem_array->elems[i].dest = external_addr;
			break;
		default:
			elem_array->elems[i].src = external_addr;
			elem_array->elems[i].dest = dma_buffer_addr;
			break;
		}

		dma_buffer_addr += buffer_bytes;
	}
	elem_array->count = buffer_count;
	return 0;
}

void dma_sg_free(struct dma_sg_elem_array *elem_array)
{
	rfree(elem_array->elems);
	dma_sg_init(elem_array);
}

void dma_buffer_copy_from(struct comp_buffer *source, struct comp_buffer *sink,
	void (*process)(struct comp_buffer *, struct comp_buffer *, uint32_t),
	uint32_t bytes)
{
	uint32_t head = bytes;
	uint32_t tail = 0;

	/* source buffer contains data copied by DMA */
	if (source->r_ptr + bytes > source->end_addr) {
		head = source->end_addr - source->r_ptr;
		tail = bytes - head;
	}

	dcache_invalidate_region(source->r_ptr, head);
	if (tail)
		dcache_invalidate_region(source->addr, tail);

	/* process data */
	process(source, sink, bytes);

	source->r_ptr += bytes;

	/* check for pointer wrap */
	if (source->r_ptr >= source->end_addr)
		source->r_ptr = source->addr +
			(source->r_ptr - source->end_addr);

	comp_update_buffer_produce(sink, bytes);
}

void dma_buffer_copy_to(struct comp_buffer *source, struct comp_buffer *sink,
	void (*process)(struct comp_buffer *, struct comp_buffer *, uint32_t),
	uint32_t bytes)
{
	uint32_t head = bytes;
	uint32_t tail = 0;

	/* process data */
	process(source, sink, bytes);

	/* sink buffer contains data meant to copied to DMA */
	if (sink->w_ptr + bytes > sink->end_addr) {
		head = sink->end_addr - sink->w_ptr;
		tail = bytes - head;
	}

	dcache_writeback_region(sink->w_ptr, head);
	if (tail)
		dcache_writeback_region(sink->addr, tail);

	sink->w_ptr += bytes;

	/* check for pointer wrap */
	if (source->r_ptr >= sink->end_addr)
		sink->w_ptr = sink->addr +
			(sink->w_ptr - sink->end_addr);

	comp_update_buffer_consume(source, bytes);
}
