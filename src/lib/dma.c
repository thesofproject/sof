// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/atomic.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

struct dma *dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags)
{
	const struct dma_info *info = dma_info_get();
	int users, ret;
	int min_users = INT32_MAX;
	struct dma *d = NULL, *dmin = NULL;

	if (!info->num_dmas) {
		trace_error(TRACE_CLASS_DMA, "dma_get(): No DMACs installed");
		return NULL;
	}

	/* find DMAC with free channels that matches request */
	for (d = info->dma_array; d < info->dma_array + info->num_dmas;
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

		for (d = info->dma_array;
		     d < info->dma_array + info->num_dmas;
		     d++) {
			trace_error(TRACE_CLASS_DMA, " DMAC ID %d users %d "
				    "busy channels %d", d->plat_data.id,
				    d->sref,
				    atomic_read(&d->num_channels_busy));
			trace_error(TRACE_CLASS_DMA, "  caps 0x%x dev 0x%x",
				    d->plat_data.caps, d->plat_data.devs);
		}

		platform_shared_commit(info->dma_array,
				       sizeof(struct dma) * info->num_dmas);

		return NULL;
	}

	platform_shared_commit(info->dma_array,
			       sizeof(struct dma) * info->num_dmas);

	/* return DMAC */
	tracev_event(TRACE_CLASS_DMA, "dma_get(), dma-probe id = %d",
		     dmin->plat_data.id);

	/* Shared DMA controllers with multiple channels
	 * may be requested many times, let the probe()
	 * do on-first-use initialization.
	 */
	spin_lock(&dmin->lock);

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

	platform_shared_commit(dmin, sizeof(*dmin));

	spin_unlock(&dmin->lock);
	return !ret ? dmin : NULL;
}

void dma_put(struct dma *dma)
{
	int ret;

	spin_lock(&dma->lock);
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
	platform_shared_commit(dma, sizeof(*dma));
	spin_unlock(&dma->lock);
}

int dma_sg_alloc(struct dma_sg_elem_array *elem_array,
		 enum mem_zone zone,
		 uint32_t direction,
		 uint32_t buffer_count, uint32_t buffer_bytes,
		 uintptr_t dma_buffer_addr, uintptr_t external_addr)
{
	int i;

	elem_array->elems = rzalloc(zone, 0, SOF_MEM_CAPS_RAM,
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

void dma_buffer_copy_from(struct comp_buffer *source, uint32_t source_bytes,
			  struct comp_buffer *sink, uint32_t sink_bytes,
			  dma_process_func process, uint32_t samples)
{
	struct audio_stream *istream = &source->stream;

	/* source buffer contains data copied by DMA */
	audio_stream_invalidate(istream, source_bytes);

	/* process data */
	process(istream, 0, &sink->stream, 0, samples);

	buffer_writeback(sink, sink_bytes);

	istream->r_ptr = (char *)istream->r_ptr + source_bytes;
	istream->r_ptr = audio_stream_wrap(istream, istream->r_ptr);

	comp_update_buffer_produce(sink, sink_bytes);
}

void dma_buffer_copy_to(struct comp_buffer *source, uint32_t source_bytes,
			struct comp_buffer *sink, uint32_t sink_bytes,
			dma_process_func process, uint32_t samples)
{
	struct audio_stream *ostream = &sink->stream;

	buffer_invalidate(source, source_bytes);

	/* process data */
	process(&source->stream, 0, ostream, 0, samples);

	/* sink buffer contains data meant to copied to DMA */
	audio_stream_writeback(ostream, sink_bytes);

	ostream->w_ptr = (char *)ostream->w_ptr + sink_bytes;
	ostream->w_ptr = audio_stream_wrap(ostream, ostream->w_ptr);

	comp_update_buffer_consume(source, source_bytes);
}
