/*
 * Copyright (c) 2017, Intel Corporation
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
 */

#ifndef __INCLUDE_AUDIO_BUFFER_H__
#define __INCLUDE_AUDIO_BUFFER_H__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/dma.h>
#include <reef/audio/component.h>
#include <reef/trace.h>
#include <reef/schedule.h>
#include <uapi/ipc.h>

/* pipeline tracing */
#define trace_buffer(__e)	trace_event(TRACE_CLASS_BUFFER, __e)
#define trace_buffer_error(__e)	trace_error(TRACE_CLASS_BUFFER, __e)
#define tracev_buffer(__e)	tracev_event(TRACE_CLASS_BUFFER, __e)

/* audio component buffer - connects 2 audio components together in pipeline */
struct comp_buffer {

	/* runtime data */
	uint32_t connected;	/* connected in path */
	uint32_t size;		/* size of buffer in bytes */
	uint32_t alloc_size;	/* allocated size in bytes */
	uint32_t avail;		/* available bytes for reading */
	uint32_t free;		/* free bytes for writing */
	void *w_ptr;		/* buffer write pointer */
	void *r_ptr;		/* buffer read position */
	void *addr;		/* buffer base address */
	void *end_addr;		/* buffer end address */

	/* IPC configuration */
	struct sof_ipc_buffer ipc_buffer;

	/* connected components */
	struct comp_dev *source;	/* source component */
	struct comp_dev *sink;		/* sink component */

	/* lists */
	struct list_item source_list;	/* list in comp buffers */
	struct list_item sink_list;	/* list in comp buffers */
};

/* pipeline buffer creation and destruction */
struct comp_buffer *buffer_new(struct sof_ipc_buffer *desc);
void buffer_free(struct comp_buffer *buffer);

/* called by a component after pruducing data into this buffer */
static inline void comp_update_buffer_produce(struct comp_buffer *buffer,
	uint32_t bytes)
{
	/* complain loudly if component tries to overrun buffer
	 * components MUST check for free space first !! */
	if (bytes > buffer->free) {
		trace_buffer_error("Xxo");
		trace_value(buffer->ipc_buffer.comp.id);
		return;
	}

	buffer->w_ptr += bytes;

	/* check for pointer wrap */
	if (buffer->w_ptr >= buffer->end_addr)
		buffer->w_ptr = buffer->addr;

	/* calculate available bytes */
	if (buffer->r_ptr < buffer->w_ptr)
		buffer->avail = buffer->w_ptr - buffer->r_ptr;
	else if (buffer->r_ptr == buffer->w_ptr)
		buffer->avail = buffer->size; /* full */
	else
		buffer->avail = buffer->size - (buffer->r_ptr - buffer->w_ptr);

	/* calculate free bytes */
	buffer->free = buffer->size - buffer->avail;

	tracev_buffer("pro");
	tracev_value((buffer->avail << 12) | buffer->free);
	tracev_value((buffer->ipc_buffer.comp.id << 16) | buffer->size);
}

/* called by a component after consuming data from this buffer */
static inline void comp_update_buffer_consume(struct comp_buffer *buffer,
	uint32_t bytes)
{
	/* complain loudly if component tries to underrun buffer
	 * components MUST check for avail space first !! */
	if (buffer->avail < bytes) {
		trace_buffer_error("Xxu");
		trace_value(buffer->ipc_buffer.comp.id);
		return;
	}

	buffer->r_ptr += bytes;

	/* check for pointer wrap */
	if (buffer->r_ptr >= buffer->end_addr)
		buffer->r_ptr = buffer->addr;

	/* calculate available bytes */
	if (buffer->r_ptr < buffer->w_ptr)
		buffer->avail = buffer->w_ptr - buffer->r_ptr;
	else if (buffer->r_ptr == buffer->w_ptr)
		buffer->avail = 0; /* empty */
	else
		buffer->avail = buffer->size - (buffer->r_ptr - buffer->w_ptr);

	/* calculate free bytes */
	buffer->free = buffer->size - buffer->avail;

	tracev_buffer("con");
	tracev_value((buffer->avail << 12) | buffer->free);
	tracev_value((buffer->ipc_buffer.comp.id << 16) | buffer->size);
}

/* get the max number of bytes that can be copied between sink and source */
static inline uint32_t comp_buffer_get_copy_bytes(struct comp_dev *dev,
	struct comp_buffer *source, struct comp_buffer *sink)
{
	uint32_t copy_bytes;

	/* Check that source has enough frames available and sink enough
	 * frames free.
	 */
	/* Run EQ if buffers have enough room */
	if (source->avail > sink->free)
		copy_bytes = sink->free;
	else
		copy_bytes = source->avail;

	return copy_bytes;
}

#endif
