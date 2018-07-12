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

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sof/sof.h>
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/alloc.h>
#include <sof/debug.h>
#include <sof/ipc.h>
#include <platform/timer.h>
#include <platform/platform.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/buffer.h>

/* create a new component in the pipeline */
struct comp_buffer *buffer_new(struct sof_ipc_buffer *desc)
{
	struct comp_buffer *buffer;

	trace_buffer("new");

	/* validate request */
	if (desc->size == 0 || desc->size > HEAP_BUFFER_SIZE) {
		trace_buffer_error("ebg");
		trace_error_value(desc->size);
		return NULL;
	}

	/* allocate new buffer */
	buffer = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*buffer));
	if (buffer == NULL) {
		trace_buffer_error("ebN");
		return NULL;
	}

	buffer->addr = rballoc(RZONE_RUNTIME, desc->caps, desc->size);
	if (buffer->addr == NULL) {
		rfree(buffer);
		trace_buffer_error("ebm");
		return NULL;
	}

	bzero(buffer->addr, desc->size);
	memcpy(&buffer->ipc_buffer, desc, sizeof(*desc));

	buffer->size = buffer->alloc_size = desc->size;
	buffer->ipc_buffer = *desc;
	buffer->w_ptr = buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + buffer->ipc_buffer.size;
	buffer->free = buffer->ipc_buffer.size;
	buffer->avail = 0;
	buffer->connected = 0;

	spinlock_init(&buffer->lock);

	return buffer;
}

/* free component in the pipeline */
void buffer_free(struct comp_buffer *buffer)
{
	trace_buffer("BFr");

	list_item_del(&buffer->source_list);
	list_item_del(&buffer->sink_list);
	rfree(buffer->addr);
	rfree(buffer);
}

void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes)
{
	uint32_t flags;

	spin_lock_irq(&buffer->lock, flags);

	/*
	 * new data produce, handle consistency for buffer and cache:
	 * 1. source(DMA) --> buffer --> sink(non-DMA): invalidate cache.
	 * 2. source(non-DMA) --> buffer --> sink(DMA): write back to memory.
	 * 3. source(DMA) --> buffer --> sink(DMA): do nothing.
	 * 4. source(non-DMA) --> buffer --> sink(non-DMA): do nothing.
	 */
	if (buffer->source->is_dma_connected &&
	    !buffer->sink->is_dma_connected)
		/* need invalidate cache for sink component to use */
		dcache_invalidate_region(buffer->w_ptr, bytes);
	else if (!buffer->source->is_dma_connected &&
		 buffer->sink->is_dma_connected)
		/* need write back to memory for sink component to use */
		dcache_writeback_region(buffer->w_ptr, bytes);

	buffer->w_ptr += bytes;

	/* check for pointer wrap */
	if (buffer->w_ptr >= buffer->end_addr)
		buffer->w_ptr = buffer->addr + (buffer->w_ptr - buffer->end_addr);

	/* calculate available bytes */
	if (buffer->r_ptr < buffer->w_ptr)
		buffer->avail = buffer->w_ptr - buffer->r_ptr;
	else if (buffer->r_ptr == buffer->w_ptr)
		buffer->avail = buffer->size; /* full */
	else
		buffer->avail = buffer->size - (buffer->r_ptr - buffer->w_ptr);

	/* calculate free bytes */
	buffer->free = buffer->size - buffer->avail;

	spin_unlock_irq(&buffer->lock, flags);

	tracev_buffer("pro");
	tracev_value((buffer->avail << 16) | buffer->free);
	tracev_value((buffer->ipc_buffer.comp.id << 16) | buffer->size);
	tracev_value((buffer->r_ptr - buffer->addr) << 16 | (buffer->w_ptr - buffer->addr));
}

void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes)
{
	uint32_t flags;

	spin_lock_irq(&buffer->lock, flags);

	buffer->r_ptr += bytes;

	/* check for pointer wrap */
	if (buffer->r_ptr >= buffer->end_addr)
		buffer->r_ptr = buffer->addr + (buffer->r_ptr - buffer->end_addr);

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

	spin_unlock_irq(&buffer->lock, flags);

	tracev_buffer("con");
	tracev_value((buffer->avail << 16) | buffer->free);
	tracev_value((buffer->ipc_buffer.comp.id << 16) | buffer->size);
	tracev_value((buffer->r_ptr - buffer->addr) << 16 | (buffer->w_ptr - buffer->addr));
}
