/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_AUDIO_BUFFER_H__
#define __SOF_AUDIO_BUFFER_H__

#include <sof/audio/audio_stream.h>
#include <sof/audio/audio_buffer.h>
#include <sof/audio/pipeline.h>
#include <sof/math/numbers.h>
#include <sof/common.h>
#include <rtos/panic.h>
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

/** \brief Retrieves subid (comp id) from the buffer */
#define buf_get_id(buf_ptr) ((buf_ptr)->stream.runtime_stream_params.id)

#if defined(__ZEPHYR__) && defined(CONFIG_ZEPHYR_LOG)

#if CONFIG_IPC_MAJOR_4
#define __BUF_FMT "buf:%u %#x "
#else
#define __BUF_FMT "buf:%u.%u "
#endif

#define buf_err(buf_ptr, __e, ...) LOG_ERR(__BUF_FMT __e, buffer_pipeline_id(buf_ptr), \
					   buf_get_id(buf_ptr), ##__VA_ARGS__)

#define buf_warn(buf_ptr, __e, ...) LOG_WRN(__BUF_FMT __e, buffer_pipeline_id(buf_ptr), \
					    buf_get_id(buf_ptr), ##__VA_ARGS__)

#define buf_info(buf_ptr, __e, ...) LOG_INF(__BUF_FMT __e, buffer_pipeline_id(buf_ptr), \
					    buf_get_id(buf_ptr), ##__VA_ARGS__)

#define buf_dbg(buf_ptr, __e, ...) LOG_DBG(__BUF_FMT __e, buffer_pipeline_id(buf_ptr), \
					   buf_get_id(buf_ptr), ##__VA_ARGS__)

#else
/** \brief Trace error message from buffer */
#define buf_err(buf_ptr, __e, ...)						\
	trace_dev_err(trace_buf_get_tr_ctx, buffer_pipeline_id,			\
		      buf_get_id,					\
		      (__sparse_force const struct comp_buffer *)buf_ptr,	\
		      __e, ##__VA_ARGS__)

/** \brief Trace warning message from buffer */
#define buf_warn(buf_ptr, __e, ...)						\
	trace_dev_warn(trace_buf_get_tr_ctx, buffer_pipeline_id,			\
		       buf_get_id,					\
		       (__sparse_force const struct comp_buffer *)buf_ptr,	\
		        __e, ##__VA_ARGS__)

/** \brief Trace info message from buffer */
#define buf_info(buf_ptr, __e, ...)						\
	trace_dev_info(trace_buf_get_tr_ctx, buffer_pipeline_id,			\
		       buf_get_id,					\
		       (__sparse_force const struct comp_buffer *)buf_ptr,	\
		       __e, ##__VA_ARGS__)

/** \brief Trace debug message from buffer */
#if defined(CONFIG_LIBRARY)
#define buf_dbg(buf_ptr, __e, ...)
#else
#define buf_dbg(buf_ptr, __e, ...)						\
	trace_dev_dbg(trace_buf_get_tr_ctx, buffer_pipeline_id,			\
		      buf_get_id,					\
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
 * this is a legacy component connector for pipeline 1.0
 *
 */
struct comp_buffer {
	/* data buffer */
	struct sof_audio_buffer audio_buffer;

	struct audio_stream stream;

	/* configuration */
	uint32_t caps;
	uint32_t core;
	struct tr_ctx tctx;			/* trace settings */

	/* connected components */
	struct comp_dev *source;	/* source component */
	struct comp_dev *sink;		/* sink component */

	/* lists */
	struct list_item source_list;	/* list in comp buffers */
	struct list_item sink_list;	/* list in comp buffers */

	/* list of buffers, to be used i.e. in raw data processing mode*/
	struct list_item buffers_list;
};

/*
 * get a component providing data to the buffer
 */
static inline struct comp_dev *comp_buffer_get_source_component(const struct comp_buffer *buffer)
{
	return buffer->source;
}

/*
 * get a component consuming data from the buffer
 */
static inline struct comp_dev *comp_buffer_get_sink_component(const struct comp_buffer *buffer)
{
	return buffer->sink;
}

/* Only to be used for synchronous same-core notifications! */
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

#define buffer_from_list(ptr, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? \
	 container_of(ptr, struct comp_buffer, source_list) : \
	 container_of(ptr, struct comp_buffer, sink_list))

#define buffer_set_cb(buffer, func, data, type) \
	do {				\
		buffer->cb = func;	\
		buffer->cb_data = data;	\
		buffer->cb_type = type;	\
	} while (0)

/* pipeline buffer creation and destruction */
struct comp_buffer *buffer_alloc(size_t size, uint32_t caps, uint32_t flags, uint32_t align,
				 bool is_shared);
struct comp_buffer *buffer_alloc_range(size_t preferred_size, size_t minimum_size, uint32_t caps,
				       uint32_t flags, uint32_t align, bool is_shared);
struct comp_buffer *buffer_new(const struct sof_ipc_buffer *desc, bool is_shared);

int buffer_set_size(struct comp_buffer *buffer, uint32_t size, uint32_t alignment);
int buffer_set_size_range(struct comp_buffer *buffer, size_t preferred_size, size_t minimum_size,
			  uint32_t alignment);

/* legacy wrappers, to be removed. Don't use them if possible */
static inline struct comp_buffer *comp_buffer_get_from_source(struct sof_source *source)
{
	struct sof_audio_buffer *audio_buffer = sof_audio_buffer_from_source(source);

	return container_of(audio_buffer, struct comp_buffer, audio_buffer);
}

static inline struct comp_buffer *comp_buffer_get_from_sink(struct sof_sink *sink)
{
	struct sof_audio_buffer *audio_buffer = sof_audio_buffer_from_sink(sink);

	return container_of(audio_buffer, struct comp_buffer, audio_buffer);
}

static inline void buffer_free(struct comp_buffer *buffer)
{
	audio_buffer_free(&buffer->audio_buffer);
}

/* end of legacy wrappers */

void buffer_zero(struct comp_buffer *buffer);

/* called by a component after producing data into this buffer */
void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes);

/* called by a component after consuming data from this buffer */
void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes);

int buffer_set_params(struct comp_buffer *buffer,
		      struct sof_ipc_stream_params *params, bool force_update);

bool buffer_params_match(struct comp_buffer *buffer,
			 struct sof_ipc_stream_params *params, uint32_t flag);

static inline void buffer_stream_invalidate(struct comp_buffer *buffer, uint32_t bytes)
{
#if CONFIG_INCOHERENT
	if (audio_buffer_is_shared(&buffer->audio_buffer))
		audio_stream_invalidate(&buffer->stream, bytes);
#endif
}

static inline void buffer_stream_writeback(struct comp_buffer *buffer, uint32_t bytes)
{
#if CONFIG_INCOHERENT
	if (audio_buffer_is_shared(&buffer->audio_buffer))
		audio_stream_writeback(&buffer->stream, bytes);
#endif
}


/*
 * Attach a new buffer at the beginning of the list. Note, that "head" must
 * really be the head of the list, not a list head within another buffer. We
 * don't synchronise its cache, so it must not be embedded in an object, using
 * the coherent API. The caller takes care to protect list heads.
 */
void buffer_attach(struct comp_buffer *buffer, struct list_item *head, int dir);

/*
 * Detach a buffer from anywhere in the list. "head" is again the head of the
 * list, we need it to verify, whether this buffer was the first or the last on
 * the list. Again it is assumed that the head's cache doesn't need to be
 * synchronized. The caller takes care to protect list heads.
 */
void buffer_detach(struct comp_buffer *buffer, struct list_item *head, int dir);

static inline struct comp_dev *buffer_get_comp(struct comp_buffer *buffer, int dir)
{
	struct comp_dev *comp = (dir == PPL_DIR_DOWNSTREAM) ?
					comp_buffer_get_sink_component(buffer) :
					comp_buffer_get_source_component(buffer);
	return comp;
}

static inline uint32_t buffer_pipeline_id(const struct comp_buffer *buffer)
{
	return buffer->stream.runtime_stream_params.pipeline_id;
}

/* Run-time buffer re-configuration calls this too, so it must use cached access */
static inline void buffer_init_stream(struct comp_buffer *buffer, size_t size)
{
	/* addr should be set in alloc function */
	audio_stream_init(&buffer->stream, buffer->stream.addr, size);
}

#endif /* __SOF_AUDIO_BUFFER_H__ */
