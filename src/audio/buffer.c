// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* create a new component in the pipeline */
struct comp_buffer *buffer_new(struct sof_ipc_buffer *desc)
{
	struct comp_buffer *buffer;

	trace_buffer("buffer_new()");

	/* validate request */
	if (desc->size == 0 || desc->size > HEAP_BUFFER_SIZE) {
		trace_buffer_error("buffer_new() error: "
				   "new size = %u is invalid", desc->size);
		return NULL;
	}

	/* allocate new buffer */
	buffer = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*buffer));
	if (!buffer) {
		trace_buffer_error("buffer_new() error: "
				   "could not alloc structure");
		return NULL;
	}

	buffer->addr = rballoc(RZONE_BUFFER, desc->caps, desc->size);
	if (!buffer->addr) {
		rfree(buffer);
		trace_buffer_error("buffer_new() error: "
				   "could not alloc size = %u "
				   "bytes of type = %u",
				   desc->size, desc->caps);
		return NULL;
	}

	buffer_init(buffer, desc->size, desc->caps);

	buffer->id = desc->comp.id;
	buffer->pipeline_id = desc->comp.pipeline_id;

	return buffer;
}

int buffer_set_size(struct comp_buffer *buffer, uint32_t size)
{
	void *new_ptr = NULL;

	/* validate request */
	if (size == 0 || size > HEAP_BUFFER_SIZE) {
		trace_buffer_error("resize error: size = %u is invalid", size);
		return -EINVAL;
	}

	if (size == buffer->size)
		return 0;

	new_ptr = rbrealloc(buffer->addr, RZONE_BUFFER, buffer->caps, size);

	/* we couldn't allocate bigger chunk */
	if (!new_ptr && size > buffer->size) {
		trace_buffer_error("resize error: can't alloc %u bytes type %u",
				   buffer->size, buffer->caps);
		return -ENOMEM;
	}

	/* use bigger chunk, else just use the old chunk but set smaller */
	if (new_ptr)
		buffer->addr = new_ptr;

	buffer_init(buffer, size, buffer->caps);

	return 0;
}

/* free component in the pipeline */
void buffer_free(struct comp_buffer *buffer)
{
	trace_buffer("buffer_free()");

	list_item_del(&buffer->source_list);
	list_item_del(&buffer->sink_list);
	rfree(buffer->addr);
	rfree(buffer);
}

void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes)
{
	uint32_t flags;
	uint32_t head = bytes;
	uint32_t tail = 0;

	/* return if no bytes */
	if (!bytes) {
		trace_buffer("comp_update_buffer_produce(), "
			     "no bytes to produce, source->comp.id = %u, "
			     "source->comp.type = %u, sink->comp.id = %u, "
			     "sink->comp.type = %u", buffer->source->comp.id,
			     buffer->source->comp.type, buffer->sink->comp.id,
			     buffer->sink->comp.type);
		return;
	}

	irq_local_disable(flags);

	/* calculate head and tail size for dcache circular wrap ops */
	if (buffer->w_ptr + bytes > buffer->end_addr) {
		head = buffer->end_addr - buffer->w_ptr;
		tail = bytes - head;
	}

	/*
	 * new data produce, handle consistency for buffer and cache:
	 * 1. source(DMA) --> buffer --> sink(non-DMA): invalidate cache.
	 * 2. source(non-DMA) --> buffer --> sink(DMA): write back to memory.
	 * 3. source(DMA) --> buffer --> sink(DMA): do nothing.
	 * 4. source(non-DMA) --> buffer --> sink(non-DMA): do nothing.
	 */
	if (buffer->source->is_dma_connected &&
	    !buffer->sink->is_dma_connected) {
		/* need invalidate cache for sink component to use */
		dcache_invalidate_region(buffer->w_ptr, head);
		if (tail)
			dcache_invalidate_region(buffer->addr, tail);
	} else if (!buffer->source->is_dma_connected &&
		   buffer->sink->is_dma_connected) {
		/* need write back to memory for sink component to use */
		dcache_writeback_region(buffer->w_ptr, head);
		if (tail)
			dcache_writeback_region(buffer->addr, tail);
	}

	buffer->w_ptr += bytes;

	/* check for pointer wrap */
	if (buffer->w_ptr >= buffer->end_addr)
		buffer->w_ptr = buffer->addr +
			(buffer->w_ptr - buffer->end_addr);

	/* "overwrite" old data in circular wrap case */
	if (bytes > buffer->free)
		buffer->r_ptr = buffer->w_ptr;

	/* calculate available bytes */
	if (buffer->r_ptr < buffer->w_ptr)
		buffer->avail = buffer->w_ptr - buffer->r_ptr;
	else if (buffer->r_ptr == buffer->w_ptr)
		buffer->avail = buffer->size; /* full */
	else
		buffer->avail = buffer->size - (buffer->r_ptr - buffer->w_ptr);

	/* calculate free bytes */
	buffer->free = buffer->size - buffer->avail;

	if (buffer->cb && buffer->cb_type & BUFF_CB_TYPE_PRODUCE)
		buffer->cb(buffer->cb_data, bytes);

	irq_local_enable(flags);

	tracev_buffer("comp_update_buffer_produce(), ((buffer->avail << 16) | "
		      "buffer->free) = %08x, ((buffer->id << 16) | "
		      "buffer->size) = %08x",
		      (buffer->avail << 16) | buffer->free,
		      (buffer->id << 16) | buffer->size);
	tracev_buffer("comp_update_buffer_produce(), ((buffer->r_ptr - buffer"
		      "->addr) << 16 | (buffer->w_ptr - buffer->addr)) = %08x",
		      (buffer->r_ptr - buffer->addr) << 16 |
		      (buffer->w_ptr - buffer->addr));
}

void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes)
{
	uint32_t flags;

	/* return if no bytes */
	if (!bytes) {
		trace_buffer("comp_update_buffer_consume(), "
			     "no bytes to consume, source->comp.id = %u, "
			     "source->comp.type = %u, sink->comp.id = %u, "
			     "sink->comp.type = %u", buffer->source->comp.id,
			     buffer->source->comp.type, buffer->sink->comp.id,
			     buffer->sink->comp.type);
		return;
	}

	irq_local_disable(flags);

	buffer->r_ptr += bytes;

	/* check for pointer wrap */
	if (buffer->r_ptr >= buffer->end_addr)
		buffer->r_ptr = buffer->addr +
			(buffer->r_ptr - buffer->end_addr);

	/* calculate available bytes */
	if (buffer->r_ptr < buffer->w_ptr)
		buffer->avail = buffer->w_ptr - buffer->r_ptr;
	else if (buffer->r_ptr == buffer->w_ptr)
		buffer->avail = 0; /* empty */
	else
		buffer->avail = buffer->size - (buffer->r_ptr - buffer->w_ptr);

	/* calculate free bytes */
	buffer->free = buffer->size - buffer->avail;

	if (buffer->sink->is_dma_connected &&
	    !buffer->source->is_dma_connected)
		dcache_writeback_region(buffer->r_ptr, bytes);

	if (buffer->cb && buffer->cb_type & BUFF_CB_TYPE_CONSUME)
		buffer->cb(buffer->cb_data, bytes);

	irq_local_enable(flags);

	tracev_buffer("comp_update_buffer_consume(), "
		      "(buffer->avail << 16) | buffer->free = %08x, "
		      "(buffer->id << 16) | buffer->size = %08x, "
		      "(buffer->r_ptr - buffer->addr) << 16 | "
		      "(buffer->w_ptr - buffer->addr)) = %08x",
		      (buffer->avail << 16) | buffer->free,
		      (buffer->id << 16) | buffer->size,
		      (buffer->r_ptr - buffer->addr) << 16 |
		      (buffer->w_ptr - buffer->addr));
}
