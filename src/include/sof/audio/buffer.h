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
#include <sof/lock.h>
#include <sof/list.h>
#include <sof/stream.h>
#include <sof/dma.h>
#include <sof/audio/component.h>
#include <sof/trace.h>
#include <sof/schedule.h>
#include <sof/cache.h>
#include <uapi/ipc/topology.h>

/* pipeline tracing */
#define trace_buffer(__e, ...)	trace_event(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)
#define trace_buffer_error(__e, ...)	trace_error(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)
#define tracev_buffer(__e, ...)	tracev_event(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)

/* buffer callback types */
#define BUFF_CB_TYPE_PRODUCE	BIT(0)
#define BUFF_CB_TYPE_CONSUME	BIT(1)

/* audio component buffer - connects 2 audio components together in pipeline */
struct comp_buffer {

	/* runtime data */
	uint32_t size;		/* runtime buffer size in bytes (period multiple) */
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

	/* callbacks */
	void (*cb)(void *data, uint32_t bytes);
	void *cb_data;
	int cb_type;

	spinlock_t lock; /* component buffer spinlock */
};

#define buffer_comp_list(buffer, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? &buffer->source_list : \
	 &buffer->sink_list)

#define buffer_from_list(ptr, type, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? \
	 container_of(ptr, type, source_list) : \
	 container_of(ptr, type, sink_list))

#define buffer_get_comp(buffer, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? buffer->sink : \
	 buffer->source)

#define buffer_set_comp(buffer, comp, dir) \
	do {						\
		if (dir == PPL_CONN_DIR_COMP_TO_BUFFER)	\
			buffer->source = comp;		\
		else					\
			buffer->sink = comp;		\
	} while (0)					\

#define buffer_set_cb(buffer, func, data, type) \
	do {				\
		buffer->cb = func;	\
		buffer->cb_data = data;	\
		buffer->cb_type = type;	\
	} while (0)

#define buffer_read_frag(buffer, idx, size) \
	buffer_get_frag(buffer, buffer->r_ptr, idx, size)

#define buffer_read_frag_s16(buffer, idx) \
	buffer_get_frag(buffer, buffer->r_ptr, idx, sizeof(int16_t))

#define buffer_read_frag_s32(buffer, idx) \
	buffer_get_frag(buffer, buffer->r_ptr, idx, sizeof(int32_t))

#define buffer_write_frag(buffer, idx, size) \
	buffer_get_frag(buffer, buffer->w_ptr, idx, size)

#define buffer_write_frag_s16(buffer, idx) \
	buffer_get_frag(buffer, buffer->w_ptr, idx, sizeof(int16_t))

#define buffer_write_frag_s32(buffer, idx) \
	buffer_get_frag(buffer, buffer->w_ptr, idx, sizeof(int32_t))

typedef void (*cache_buff_op)(struct comp_buffer *);

/* pipeline buffer creation and destruction */
struct comp_buffer *buffer_new(struct sof_ipc_buffer *desc);
void buffer_free(struct comp_buffer *buffer);

/* called by a component after producing data into this buffer */
void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes);

/* called by a component after consuming data from this buffer */
void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes);

static inline void buffer_zero(struct comp_buffer *buffer)
{
	tracev_buffer("buffer_zero()");

	bzero(buffer->addr, buffer->size);
	if (buffer->ipc_buffer.caps & SOF_MEM_CAPS_DMA)
		dcache_writeback_region(buffer->addr, buffer->size);
}

/* get the max number of bytes that can be copied between sink and source */
static inline int comp_buffer_can_copy_bytes(struct comp_buffer *source,
					     struct comp_buffer *sink,
					     uint32_t bytes)
{
	/* check for underrun */
	if (source->avail < bytes)
		return -1;

	/* check for overrun */
	if (sink->free < bytes)
		return 1;

	/* we are good to copy */
	return 0;
}

static inline uint32_t comp_buffer_get_copy_bytes(struct comp_buffer *source,
						  struct comp_buffer *sink)
{
	if (source->avail > sink->free)
		return sink->free;
	else
		return source->avail;
}

static inline void comp_buffer_cache_wtb_inv(struct comp_buffer *buffer)
{
	dcache_writeback_invalidate_region(buffer, sizeof(*buffer));
}

static inline void comp_buffer_cache_inv(struct comp_buffer *buffer)
{
	dcache_invalidate_region(buffer, sizeof(*buffer));
}

static inline cache_buff_op comp_buffer_cache_op(int cmd)
{
	switch (cmd) {
	case CACHE_WRITEBACK_INV:
		return &comp_buffer_cache_wtb_inv;
	case CACHE_INVALIDATE:
		return &comp_buffer_cache_inv;
	default:
		trace_buffer_error("comp_buffer_cache_op() error: "
				   "invalid cmd = %u", cmd);
		return NULL;
	}
}

static inline void buffer_reset_pos(struct comp_buffer *buffer)
{
	/* reset read and write pointer to buffer bas */
	buffer->w_ptr = buffer->addr;
	buffer->r_ptr = buffer->addr;

	/* free space is buffer size */
	buffer->free = buffer->size;

	/* there are no avail samples at reset */
	buffer->avail = 0;

	/* clear buffer contents */
	buffer_zero(buffer);
}

/* set the runtime size of a buffer in bytes and improve the data cache */
/* performance by only using minimum space needed for runtime params */
static inline int buffer_set_size(struct comp_buffer *buffer, uint32_t size)
{
	if (size > buffer->alloc_size)
		return -ENOMEM;
	if (size == 0)
		return -EINVAL;

	buffer->end_addr = buffer->addr + size;
	buffer->size = size;
	return 0;
}

static inline void *buffer_get_frag(struct comp_buffer *buffer, void *ptr,
				    uint32_t idx, uint32_t size)
{
	void *current = ptr + (idx * size);

	/* check for pointer wrap */
	if (current >= buffer->end_addr)
		current = buffer->addr + (current - buffer->end_addr);

	return current;
}

#endif
