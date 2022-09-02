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
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/coherent.h>
#include <sof/math/numbers.h>
#include <rtos/spinlock.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <sof/audio/format.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct comp_dev;

/** \name Trace macros
 *  @{
 */

/* buffer tracing */
extern struct tr_ctx buffer_tr;

/** \brief Retrieves trace context from the buffer */
#define trace_buf_get_tr_ctx(buf_ptr) (&(buf_ptr)->tctx)

/** \brief Retrieves id (pipe id) from the buffer */
#define trace_buf_get_id(buf_ptr) ((buf_ptr)->pipeline_id)

/** \brief Retrieves subid (comp id) from the buffer */
#define trace_buf_get_subid(buf_ptr) ((buf_ptr)->id)

#if defined(__ZEPHYR__) && defined(CONFIG_ZEPHYR_LOG)

#define buf_err(buf_ptr, __e, ...) LOG_ERR(__e, ##__VA_ARGS__)

#define buf_warn(buf_ptr, __e, ...) LOG_WRN(__e, ##__VA_ARGS__)

#define buf_info(buf_ptr, __e, ...) LOG_INF(__e, ##__VA_ARGS__)

#define buf_dbg(buf_ptr, __e, ...) LOG_DBG(__e, ##__VA_ARGS__)

#else
/** \brief Trace error message from buffer */
#define buf_err(buf_ptr, __e, ...)						\
	trace_dev_err(trace_buf_get_tr_ctx, trace_buf_get_id,			\
		      trace_buf_get_subid,					\
		      (__sparse_force const struct comp_buffer *)buf_ptr,	\
		      __e, ##__VA_ARGS__)

/** \brief Trace warning message from buffer */
#define buf_warn(buf_ptr, __e, ...)						\
	trace_dev_warn(trace_buf_get_tr_ctx, trace_buf_get_id,			\
		       trace_buf_get_subid,					\
		       (__sparse_force const struct comp_buffer *)buf_ptr,	\
		        __e, ##__VA_ARGS__)

/** \brief Trace info message from buffer */
#define buf_info(buf_ptr, __e, ...)						\
	trace_dev_info(trace_buf_get_tr_ctx, trace_buf_get_id,			\
		       trace_buf_get_subid,					\
		       (__sparse_force const struct comp_buffer *)buf_ptr,	\
		       __e, ##__VA_ARGS__)

/** \brief Trace debug message from buffer */
#if defined(CONFIG_LIBRARY)
#define buf_dbg(buf_ptr, __e, ...)
#else
#define buf_dbg(buf_ptr, __e, ...)						\
	trace_dev_dbg(trace_buf_get_tr_ctx, trace_buf_get_id,			\
		      trace_buf_get_subid,					\
		      (__sparse_force const struct comp_buffer *)buf_ptr,	\
		      __e, ##__VA_ARGS__)
#endif

#endif /* #if defined(__ZEPHYR__) && defined(CONFIG_ZEPHYR_LOG) */

/** @}*/

/* buffer callback types */
#define BUFF_CB_TYPE_PRODUCE	BIT(0)
#define BUFF_CB_TYPE_CONSUME	BIT(1)

#define BUFFER_UPDATE_IF_UNSET	0
#define BUFFER_UPDATE_FORCE	1

/* buffer parameters */
#define BUFF_PARAMS_FRAME_FMT	BIT(0)
#define BUFF_PARAMS_BUFFER_FMT	BIT(1)
#define BUFF_PARAMS_RATE	BIT(2)
#define BUFF_PARAMS_CHANNELS	BIT(3)

/*
 * audio component buffer - connects 2 audio components together in pipeline.
 *
 * The buffer is a hot structure that must be shared on certain cache
 * incoherent architectures.
 *
 * Access flow (on cache incoherent architectures only)
 * 1) buffer acquired by using uncache cache coherent pointer.
 * 2) buffer is invalidated after lock acquired.
 * 3) buffer is safe to use cached pointer for access.
 * 4) release buffer cached pointer
 * 5) write back cached data and release lock using uncache pointer.
 */
struct comp_buffer {
	struct coherent c;

	/* data buffer */
	struct audio_stream stream;

	/* configuration */
	uint32_t id;
	uint32_t pipeline_id;
	uint32_t caps;
	uint32_t core;
	struct tr_ctx tctx;			/* trace settings */

	/* connected components */
	struct comp_dev *source;	/* source component */
	struct comp_dev *sink;		/* sink component */

	/* lists */
	struct list_item source_list;	/* list in comp buffers */
	struct list_item sink_list;	/* list in comp buffers */

	/* runtime stream params */
	uint32_t buffer_fmt;	/**< enum sof_ipc_buffer_format */
	uint16_t chmap[SOF_IPC_MAX_CHANNELS];	/**< channel map - SOF_CHMAP_ */

	bool hw_params_configured; /**< indicates whether hw params were set */
	bool walking;		/**< indicates if the buffer is being walked */
};

/* Only to be used for synchronous same-core notifications! */
struct buffer_cb_transact {
	struct comp_buffer __sparse_cache *buffer;
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

#define buffer_set_cb(buffer, func, data, type) \
	do {				\
		buffer->cb = func;	\
		buffer->cb_data = data;	\
		buffer->cb_type = type;	\
	} while (0)

/* pipeline buffer creation and destruction */
struct comp_buffer *buffer_alloc(uint32_t size, uint32_t caps, uint32_t align);
struct comp_buffer *buffer_new(const struct sof_ipc_buffer *desc);
int buffer_set_size(struct comp_buffer __sparse_cache *buffer, uint32_t size);
void buffer_free(struct comp_buffer *buffer);
void buffer_zero(struct comp_buffer __sparse_cache *buffer);

/* called by a component after producing data into this buffer */
void comp_update_buffer_produce(struct comp_buffer __sparse_cache *buffer, uint32_t bytes);

/* called by a component after consuming data from this buffer */
void comp_update_buffer_consume(struct comp_buffer __sparse_cache *buffer, uint32_t bytes);

int buffer_set_params(struct comp_buffer __sparse_cache *buffer,
		      struct sof_ipc_stream_params *params, bool force_update);

bool buffer_params_match(struct comp_buffer __sparse_cache *buffer,
			 struct sof_ipc_stream_params *params, uint32_t flag);

static inline void buffer_stream_invalidate(struct comp_buffer __sparse_cache *buffer,
					    uint32_t bytes)
{
	if (!is_coherent_shared(buffer, c))
		return;

	audio_stream_invalidate(&buffer->stream, bytes);
}

static inline void buffer_stream_writeback(struct comp_buffer __sparse_cache *buffer,
					   uint32_t bytes)
{
	if (!is_coherent_shared(buffer, c))
		return;

	audio_stream_writeback(&buffer->stream, bytes);
}

__must_check static inline struct comp_buffer __sparse_cache *buffer_acquire(
	struct comp_buffer *buffer)
{
	struct coherent __sparse_cache *c = coherent_acquire_thread(&buffer->c, sizeof(*buffer));

	return attr_container_of(c, struct comp_buffer __sparse_cache, c, __sparse_cache);
}

static inline void buffer_release(struct comp_buffer __sparse_cache *buffer)
{
	coherent_release_thread(&buffer->c, sizeof(*buffer));
}

static inline struct comp_dev *buffer_get_comp(struct comp_buffer *buffer, int dir)
{
	struct comp_buffer __sparse_cache *buffer_c = buffer_acquire(buffer);
	struct comp_dev *comp = dir == PPL_DIR_DOWNSTREAM ? buffer_c->sink :
		buffer_c->source;
	buffer_release(buffer_c);
	return comp;
}

static inline void buffer_reset_pos(struct comp_buffer __sparse_cache *buffer, void *data)
{
	/* reset rw pointers and avail/free bytes counters */
	audio_stream_reset(&buffer->stream);

	/* clear buffer contents */
	buffer_zero(buffer);
}

/* Run-time buffer re-configuration calls this too, so it must use cached access */
static inline void buffer_init(struct comp_buffer __sparse_cache *buffer,
			       uint32_t size, uint32_t caps)
{
	buffer->caps = caps;

	/* addr should be set in alloc function */
	audio_stream_init(&buffer->stream, buffer->stream.addr, size);
}

static inline void buffer_reset_params(struct comp_buffer __sparse_cache *buffer, void *data)
{
	buffer->hw_params_configured = false;
}

#endif /* __SOF_AUDIO_BUFFER_H__ */
