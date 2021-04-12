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
#include <sof/lib/uuid.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* bc3526a7-9b86-4ab4-84a5-2e02ae70cc10 */
DECLARE_SOF_UUID("dma", dma_uuid, 0xbc3526a7, 0x9b86, 0x4ab4,
		 0x84, 0xa5, 0x2e, 0x02, 0xae, 0x70, 0xcc, 0x10);

DECLARE_TR_CTX(dma_tr, SOF_UUID(dma_uuid), LOG_LEVEL_INFO);

struct dma *dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags)
{
	const struct dma_info *info = dma_info_get();
	int users, ret;
	int min_users = INT32_MAX;
	struct dma *d = NULL, *dmin = NULL;
	unsigned int flags_irq;

	if (!info->num_dmas) {
		tr_err(&dma_tr, "dma_get(): No DMACs installed");
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
		tr_err(&dma_tr, "No DMAC dir %d caps 0x%x dev 0x%x flags 0x%x",
		       dir, cap, dev, flags);

		for (d = info->dma_array;
		     d < info->dma_array + info->num_dmas;
		     d++) {
			tr_err(&dma_tr, " DMAC ID %d users %d busy channels %d",
			       d->plat_data.id, d->sref,
			       atomic_read(&d->num_channels_busy));
			tr_err(&dma_tr, "  caps 0x%x dev 0x%x",
			       d->plat_data.caps, d->plat_data.devs);
		}

		return NULL;
	}

	/* return DMAC */
	tr_dbg(&dma_tr, "dma_get(), dma-probe id = %d",
	       dmin->plat_data.id);

	/* Shared DMA controllers with multiple channels
	 * may be requested many times, let the probe()
	 * do on-first-use initialization.
	 */
	spin_lock_irq(&dmin->lock, flags_irq);

	ret = 0;
	if (!dmin->sref) {
		ret = dma_probe(dmin);
		if (ret < 0) {
			tr_err(&dma_tr, "dma_get(): dma-probe failed id = %d, ret = %d",
			       dmin->plat_data.id, ret);
		}
	}
	if (!ret)
		dmin->sref++;

	tr_info(&dma_tr, "dma_get() ID %d sref = %d busy channels %d",
		dmin->plat_data.id, dmin->sref,
		atomic_read(&dmin->num_channels_busy));

	spin_unlock_irq(&dmin->lock, flags_irq);
	return !ret ? dmin : NULL;
}

void dma_put(struct dma *dma)
{
	unsigned int flags_irq;
	int ret;

	spin_lock_irq(&dma->lock, flags_irq);
	if (--dma->sref == 0) {
		ret = dma_remove(dma);
		if (ret < 0) {
			tr_err(&dma_tr, "dma_put(): dma_remove() failed id  = %d, ret = %d",
			       dma->plat_data.id, ret);
		}
	}
	tr_info(&dma_tr, "dma_put(), dma = %p, sref = %d",
		dma, dma->sref);
	spin_unlock_irq(&dma->lock, flags_irq);
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

int dma_buffer_copy_from(struct comp_buffer *source, struct comp_buffer *sink,
			 dma_process_func process, uint32_t source_bytes)
{
	struct audio_stream *istream = &source->stream;
	uint32_t samples = source_bytes /
			   audio_stream_sample_bytes(istream);
	uint32_t sink_bytes = audio_stream_sample_bytes(&sink->stream) *
			      samples;
	int ret;

	/* source buffer contains data copied by DMA */
	audio_stream_invalidate(istream, source_bytes);

	/* process data */
	ret = process(istream, 0, &sink->stream, 0, samples);

	buffer_writeback(sink, sink_bytes);

	/*
	 * consume istream using audio_stream API because this buffer doesn't
	 * appear in topology so notifier event is not needed
	 */
	audio_stream_consume(istream, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);

	return ret;
}

int dma_buffer_copy_to(struct comp_buffer *source, struct comp_buffer *sink,
		       dma_process_func process, uint32_t sink_bytes)
{
	struct audio_stream *ostream = &sink->stream;
	uint32_t samples = sink_bytes /
			   audio_stream_sample_bytes(ostream);
	uint32_t source_bytes = audio_stream_sample_bytes(&source->stream) *
			      samples;
	int ret;

	buffer_invalidate(source, source_bytes);

	/* process data */
	ret = process(&source->stream, 0, ostream, 0, samples);

	/* sink buffer contains data meant to copied to DMA */
	audio_stream_writeback(ostream, sink_bytes);

	/*
	 * produce ostream using audio_stream API because this buffer doesn't
	 * appear in topology so notifier event is not needed
	 */
	audio_stream_produce(ostream, sink_bytes);
	comp_update_buffer_consume(source, source_bytes);

	return ret;
}
