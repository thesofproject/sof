/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_AUDIO_BUFFER_H__
#define __SOF_AUDIO_BUFFER_H__

#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/list.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

struct comp_dev;

/* buffer tracing */
#define trace_buffer(__e, ...) \
	trace_event(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)
#define trace_buffer_error(__e, ...) \
	trace_error(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)
#define tracev_buffer(__e, ...) \
	tracev_event(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)
#define trace_buffer_error_atomic(__e, ...) \
	trace_error_atomic(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)

/* buffer callback types */
#define BUFF_CB_TYPE_PRODUCE	BIT(0)
#define BUFF_CB_TYPE_CONSUME	BIT(1)

/* audio component buffer - connects 2 audio components together in pipeline */
struct comp_buffer {

	/* runtime data */
	uint32_t size;	/* runtime buffer size in bytes (period multiple) */
	uint32_t alloc_size;	/* allocated size in bytes */
	uint32_t avail;		/* available bytes for reading */
	uint32_t free;		/* free bytes for writing */
	void *w_ptr;		/* buffer write pointer */
	void *r_ptr;		/* buffer read position */
	void *addr;		/* buffer base address */
	void *end_addr;		/* buffer end address */

	/* configuration */
	uint32_t id;
	uint32_t pipeline_id;
	uint32_t caps;

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
struct comp_buffer *buffer_alloc(uint32_t size, uint32_t caps, uint32_t align);
struct comp_buffer *buffer_new(struct sof_ipc_buffer *desc);
int buffer_set_size(struct comp_buffer *buffer, uint32_t size);
void buffer_free(struct comp_buffer *buffer);

/* called by a component after producing data into this buffer */
void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes);

/* called by a component after consuming data from this buffer */
void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes);

static inline void buffer_zero(struct comp_buffer *buffer)
{
	tracev_buffer("buffer_zero()");

	bzero(buffer->addr, buffer->size);
	if (buffer->caps & SOF_MEM_CAPS_DMA)
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

static inline void *buffer_get_frag(struct comp_buffer *buffer, void *ptr,
				    uint32_t idx, uint32_t size)
{
	void *current = ptr + (idx * size);

	/* check for pointer wrap */
	if (current >= buffer->end_addr)
		current = buffer->addr + (current - buffer->end_addr);

	return current;
}

static inline void buffer_init(struct comp_buffer *buffer, uint32_t size,
			       uint32_t caps)
{
	buffer->alloc_size = size;
	buffer->size = size;
	buffer->caps = caps;
	buffer->w_ptr = buffer->addr;
	buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + size;
	buffer->free = size;
	buffer->avail = 0;
	buffer_zero(buffer);
}
#endif /* __SOF_AUDIO_BUFFER_H__ */
