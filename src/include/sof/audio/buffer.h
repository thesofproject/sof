/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_AUDIO_BUFFER_H__
#define __SOF_AUDIO_BUFFER_H__

#include <sof/audio/audio_stream.h>
#include <sof/audio/pipeline.h>
#include <sof/math/numbers.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <sof/audio/format.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

struct comp_dev;

/* buffer tracing */
#define trace_buffer(__e, ...) \
	trace_event(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)
#define trace_buffer_with_ids(buf_ptr, __e, ...) \
	trace_event_with_ids(TRACE_CLASS_BUFFER, (buf_ptr)->pipeline_id, \
			     (buf_ptr)->id, __e, ##__VA_ARGS__)

#define tracev_buffer(__e, ...) \
	tracev_event(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)
#define tracev_buffer_with_ids(buf_ptr, __e, ...) \
	tracev_event_with_ids(TRACE_CLASS_BUFFER, (buf_ptr)->pipeline_id, \
			      (buf_ptr)->id, __e, ##__VA_ARGS__)

#define trace_buffer_error(__e, ...) \
	trace_error(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)
#define trace_buffer_error_atomic(__e, ...) \
	trace_error_atomic(TRACE_CLASS_BUFFER, __e, ##__VA_ARGS__)
#define trace_buffer_error_with_ids(buf_ptr, __e, ...) \
	trace_error_with_ids(TRACE_CLASS_BUFFER, (buf_ptr)->pipeline_id, \
			     (buf_ptr)->id, __e, ##__VA_ARGS__)

/* buffer callback types */
#define BUFF_CB_TYPE_PRODUCE	BIT(0)
#define BUFF_CB_TYPE_CONSUME	BIT(1)

/* audio component buffer - connects 2 audio components together in pipeline */
struct comp_buffer {
	/* data buffer */
	struct audio_stream stream;

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

	/* runtime stream params */
	uint32_t buffer_fmt;	/**< enum sof_ipc_buffer_format */
	uint16_t chmap[SOF_IPC_MAX_CHANNELS];	/**< channel map - SOF_CHMAP_ */
};

struct buffer_cb_transact {
	struct comp_buffer *buffer;
	uint32_t transaction_amount;
	void *transaction_begin_address;
};

struct buffer_cb_free {
	struct comp_buffer *buffer;
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


typedef void (*cache_buff_op)(struct comp_buffer *, void *);

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
	tracev_buffer_with_ids(buffer, "stream_zero()");

	bzero(buffer->stream.addr, buffer->stream.size);
	if (buffer->caps & SOF_MEM_CAPS_DMA)
		dcache_writeback_region(buffer->stream.addr,
					buffer->stream.size);
}

static inline void comp_buffer_cache_wtb_inv(struct comp_buffer *buffer,
					     void *data)
{
	dcache_writeback_invalidate_region(buffer, sizeof(*buffer));
}

static inline void comp_buffer_cache_inv(struct comp_buffer *buffer, void *data)
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

static inline void buffer_reset_pos(struct comp_buffer *buffer, void *data)
{
	/* reset rw pointers and avail/free bytes counters */
	audio_stream_reset(&buffer->stream);

	/* clear buffer contents */
	buffer_zero(buffer);
}

static inline void buffer_init(struct comp_buffer *buffer, uint32_t size,
			       uint32_t caps)
{
	buffer->caps = caps;

	/* addr should be set in alloc function */
	audio_stream_init(&buffer->stream, buffer->stream.addr, size);
}

#endif /* __SOF_AUDIO_BUFFER_H__ */
