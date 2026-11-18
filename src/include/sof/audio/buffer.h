/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * @file sof/audio/buffer.h
 * @brief Component interconnect buffer allocation and stream handling.
 * @ingroup audio_buffer_api
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

/** \addtogroup audio_buffer_api
 *  @{
 */

struct comp_dev;
struct buffer_cb_transact;

/** \name Trace macros
 *  @{
 */

/* buffer tracing */
extern struct tr_ctx buffer_tr;

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
/** \brief Retrieves trace context from the buffer */
#define trace_buf_get_tr_ctx(buf_ptr) (&(buf_ptr)->tctx)

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

/** \name Buffer callback types
 *  @{
 */
#define BUFF_CB_TYPE_PRODUCE	BIT(0)	/**< Callback triggered on buffer produce */
#define BUFF_CB_TYPE_CONSUME	BIT(1)	/**< Callback triggered on buffer consume */
/** @}*/

/** \name Buffer parameter update modes
 *  @{
 */
#define BUFFER_UPDATE_IF_UNSET	0	/**< Update buffer parameters only if currently unset */
#define BUFFER_UPDATE_FORCE	1	/**< Force update buffer parameters regardless of current state */
/** @}*/

/** \name Buffer parameter bitmasks
 *  @{
 */
#define BUFF_PARAMS_FRAME_FMT	BIT(0)	/**< Buffer parameter bitmask: frame format */
#define BUFF_PARAMS_BUFFER_FMT	BIT(1)	/**< Buffer parameter bitmask: buffer format */
#define BUFF_PARAMS_RATE	BIT(2)	/**< Buffer parameter bitmask: sample rate */
#define BUFF_PARAMS_CHANNELS	BIT(3)	/**< Buffer parameter bitmask: channels count */
/** @}*/

/** \name Buffer memory sharing usage
 *  @{
 */
#define BUFFER_USAGE_SHARED	true	/**< Buffer used by multiple DSP cores and/or HW blocks */
#define BUFFER_USAGE_NOT_SHARED false	/**< Buffer only used by one HW block */
/** @}*/

/**
 * \brief Audio component interconnect buffer connecting two pipeline components.
 */
struct comp_buffer {
	/* data buffer */
	struct sof_audio_buffer audio_buffer;	/**< Unified audio buffer base class */

	struct audio_stream stream;		/**< Legacy audio stream representation */

	/* configuration */
	uint32_t flags;				/**< Buffer flags */
	uint32_t core;				/**< DSP core ID managing this buffer */

#if !CONFIG_ZEPHYR_LOG
	struct tr_ctx tctx;			/**< Trace context */
#endif

	/* connected components */
	struct comp_dev *source;		/**< Source component feeding this buffer */
	struct comp_dev *sink;			/**< Sink component consuming from this buffer */

	/* lists */
	struct list_item source_list;		/**< List node in source component's buffer list */
	struct list_item sink_list;		/**< List node in sink component's buffer list */

#if CONFIG_PROBE
	/** probe produce callback, called on buffer produce */
	void (*probe_cb_produce)(void *arg, struct buffer_cb_transact *cb_data);
	/** probe free callback, called on buffer free */
	void (*probe_cb_free)(void *arg);
	/** opaque argument passed to probe callbacks */
	void *probe_cb_arg;
#endif
};

/**
 * \brief Get component providing data to the buffer.
 * \param[in] buffer Interconnect buffer.
 * \return Pointer to source component device, or NULL if none.
 */
static inline struct comp_dev *comp_buffer_get_source_component(const struct comp_buffer *buffer)
{
	return buffer->source;
}

/**
 * \brief Get component consuming data from the buffer.
 * \param[in] buffer Interconnect buffer.
 * \return Pointer to sink component device, or NULL if none.
 */
static inline struct comp_dev *comp_buffer_get_sink_component(const struct comp_buffer *buffer)
{
	return buffer->sink;
}

/**
 * \brief Set component providing data to the buffer.
 * \param[in,out] buffer Interconnect buffer.
 * \param[in] comp Source component device.
 */
static inline
void comp_buffer_set_source_component(struct comp_buffer *buffer, struct comp_dev *comp)
{
	buffer->source = comp;
}

/**
 * \brief Set component consuming data from the buffer.
 * \param[in,out] buffer Interconnect buffer.
 * \param[in] comp Sink component device.
 */
static inline
void comp_buffer_set_sink_component(struct comp_buffer *buffer, struct comp_dev *comp)
{
	buffer->sink = comp;
}

/**
 * \brief Reset buffer source list node.
 * \param[in,out] buffer Interconnect buffer.
 */
static inline void comp_buffer_reset_source_list(struct comp_buffer *buffer)
{
	list_init(&buffer->source_list);
}

/**
 * \brief Reset buffer sink list node.
 * \param[in,out] buffer Interconnect buffer.
 */
static inline void comp_buffer_reset_sink_list(struct comp_buffer *buffer)
{
	list_init(&buffer->sink_list);
}

/**
 * \brief Transaction descriptor passed to buffer probe callbacks.
 */
struct buffer_cb_transact {
	struct comp_buffer *buffer;		/**< Buffer being transacted */
	uint32_t transaction_amount;		/**< Number of bytes transacted */
	void *transaction_begin_address;	/**< Starting address of transaction */
};

/**
 * \brief Retrieve enclosing comp_buffer from a list node depending on traversal direction.
 * \param[in] ptr List item pointer.
 * \param[in] dir Pipeline traversal direction (PPL_DIR_DOWNSTREAM or PPL_DIR_UPSTREAM).
 * \return Pointer to enclosing struct comp_buffer.
 */
#define buffer_from_list(ptr, dir) \
	((dir) == PPL_DIR_DOWNSTREAM ? \
	 container_of(ptr, struct comp_buffer, source_list) : \
	 container_of(ptr, struct comp_buffer, sink_list))

/**
 * \brief Set transaction callback on a component buffer.
 * \param[in,out] buffer Target component buffer.
 * \param[in] func Callback function pointer.
 * \param[in] data Callback user data.
 * \param[in] type Callback trigger type (BUFF_CB_TYPE_*).
 */
#define buffer_set_cb(buffer, func, data, type) \
	do {				\
		buffer->cb = func;	\
		buffer->cb_data = data;	\
		buffer->cb_type = type;	\
	} while (0)

struct mod_alloc_ctx;

/**
 * \brief Allocate a pipeline component audio buffer with explicit size and alignment.
 * \param[in,out] alloc Allocation context (heap and optional vregion).
 * \param[in] size Required buffer capacity in bytes.
 * \param[in] flags Memory allocation flags (SOF_MEM_FLAG_...).
 * \param[in] align Memory alignment requirement in bytes.
 * \param[in] is_shared True if the buffer memory is shared across processing domains.
 * \return Pointer to allocated comp_buffer, or NULL on failure.
 */
struct comp_buffer *buffer_alloc(struct mod_alloc_ctx *alloc, size_t size, uint32_t flags,
				 uint32_t align, bool is_shared);

/**
 * \brief Allocate a pipeline component buffer with an elastic size range.
 * \param[in,out] alloc Allocation context.
 * \param[in] preferred_size Desired optimal buffer capacity in bytes.
 * \param[in] minimum_size Minimum acceptable buffer capacity in bytes.
 * \param[in] flags Memory allocation flags (SOF_MEM_FLAG_...).
 * \param[in] align Memory alignment requirement in bytes.
 * \param[in] is_shared True if the buffer memory is shared across processing domains.
 * \return Pointer to allocated comp_buffer, or NULL on failure.
 */
struct comp_buffer *buffer_alloc_range(struct mod_alloc_ctx *alloc, size_t preferred_size,
				       size_t minimum_size,
				       uint32_t flags, uint32_t align, bool is_shared);

/**
 * \brief Instantiate a pipeline component buffer from an IPC configuration descriptor.
 * \param[in,out] alloc Allocation context.
 * \param[in] desc IPC buffer descriptor containing size, flags, and configuration.
 * \param[in] is_shared True if the buffer memory is shared across processing domains.
 * \return Pointer to newly instantiated comp_buffer, or NULL on failure.
 */
struct comp_buffer *buffer_new(struct mod_alloc_ctx *alloc, const struct sof_ipc_buffer *desc,
			       bool is_shared);

/**
 * \brief Resize an existing pipeline component buffer.
 * \param[in,out] buffer Pointer to comp_buffer to resize.
 * \param[in] size New buffer capacity in bytes.
 * \param[in] alignment Required memory alignment in bytes.
 * \return 0 on success, or negative error code on failure.
 */
int buffer_set_size(struct comp_buffer *buffer, uint32_t size, uint32_t alignment);

/**
 * \brief Resize a pipeline component buffer within a preferred-to-minimum range.
 * \param[in,out] buffer Pointer to comp_buffer to resize.
 * \param[in] preferred_size Desired optimal buffer capacity in bytes.
 * \param[in] minimum_size Minimum acceptable buffer capacity in bytes.
 * \param[in] alignment Required memory alignment in bytes.
 * \return 0 on success, or negative error code on failure.
 */
int buffer_set_size_range(struct comp_buffer *buffer, size_t preferred_size, size_t minimum_size,
			  uint32_t alignment);

/* legacy wrappers, to be removed. Don't use them if possible */

/**
 * \brief Retrieve enclosing comp_buffer from an audio source interface pointer.
 * \param[in] source Pointer to audio source interface.
 * \return Pointer to enclosing struct comp_buffer.
 */
static inline struct comp_buffer *comp_buffer_get_from_source(struct sof_source *source)
{
	struct sof_audio_buffer *audio_buffer = sof_audio_buffer_from_source(source);

	return container_of(audio_buffer, struct comp_buffer, audio_buffer);
}

/**
 * \brief Retrieve enclosing comp_buffer from an audio sink interface pointer.
 * \param[in] sink Pointer to audio sink interface.
 * \return Pointer to enclosing struct comp_buffer.
 */
static inline struct comp_buffer *comp_buffer_get_from_sink(struct sof_sink *sink)
{
	struct sof_audio_buffer *audio_buffer = sof_audio_buffer_from_sink(sink);

	return container_of(audio_buffer, struct comp_buffer, audio_buffer);
}

/**
 * \brief Free an interconnect component buffer and its underlying memory.
 * \param[in,out] buffer Component buffer to free.
 */
static inline void buffer_free(struct comp_buffer *buffer)
{
	audio_buffer_free(&buffer->audio_buffer);
}

/* end of legacy wrappers */

/**
 * \brief Zero-fill the entire payload of an interconnect buffer.
 * \param[in,out] buffer Component buffer to zero.
 */
void buffer_zero(struct comp_buffer *buffer);

/**
 * \brief Notify buffer that audio data has been produced by a component.
 * \param[in,out] buffer Component buffer written to.
 * \param[in] bytes Number of bytes produced.
 */
void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes);

/**
 * \brief Notify buffer that audio data has been consumed by a component.
 * \param[in,out] buffer Component buffer read from.
 * \param[in] bytes Number of bytes consumed.
 */
void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes);

/**
 * \brief Set audio stream parameters on a component buffer.
 * \param[in,out] buffer Component buffer to configure.
 * \param[in] params Stream parameters to apply.
 * \param[in] force_update If true, overwrite existing parameters unconditionally.
 * \return 0 on success, or negative error code on failure.
 */
int buffer_set_params(struct comp_buffer *buffer,
		      struct sof_ipc_stream_params *params, bool force_update);

/**
 * \brief Test if buffer stream parameters match requested parameters.
 * \param[in] buffer Component buffer to check.
 * \param[in] params Stream parameters to compare.
 * \param[in] flag Bitmask specifying which parameters to check (BUFF_PARAMS_*).
 * \return True if matching, false otherwise.
 */
bool buffer_params_match(struct comp_buffer *buffer,
			 struct sof_ipc_stream_params *params, uint32_t flag);

/**
 * \brief Invalidate cache for a shared buffer stream if incoherent.
 * \param[in,out] buffer Component buffer.
 * \param[in] bytes Number of bytes to invalidate.
 */
static inline void buffer_stream_invalidate(struct comp_buffer *buffer, uint32_t bytes)
{
#if CONFIG_INCOHERENT
	if (audio_buffer_is_shared(&buffer->audio_buffer))
		audio_stream_invalidate(&buffer->stream, bytes);
#endif
}

/**
 * \brief Write back cache for a shared buffer stream if incoherent.
 * \param[in] buffer Component buffer.
 * \param[in] bytes Number of bytes to write back.
 */
static inline void buffer_stream_writeback(struct comp_buffer *buffer, uint32_t bytes)
{
#if CONFIG_INCOHERENT
	if (audio_buffer_is_shared(&buffer->audio_buffer))
		audio_stream_writeback(&buffer->stream, bytes);
#endif
}

/**
 * \brief Attach a new buffer at the beginning of a component buffer list.
 *
 * Locking: Must be called with interrupts disabled (or userspace LL system mutex held).
 * The caller takes care to protect list heads. "head" must really be the head of the
 * list, not a list node within another buffer, as its cache is not synchronized and
 * must not be embedded in an object using the coherent API.
 *
 * \param[in,out] buffer Component buffer to attach.
 * \param[in,out] head List head to prepend buffer to.
 * \param[in] dir Traversal direction (PPL_DIR_DOWNSTREAM or PPL_DIR_UPSTREAM).
 * \return 0 on success, or -EALREADY if the buffer is already linked in this direction.
 */
int buffer_attach(struct comp_buffer *buffer, struct list_item *head, int dir);

/**
 * \brief Detach a buffer from anywhere in a component buffer list.
 *
 * Locking: Must be called with interrupts disabled (or userspace LL system mutex held).
 * "head" is the head of the list, assumed not to require cache synchronization.
 * The caller takes care to protect list heads.
 *
 * \param[in,out] buffer Component buffer to detach.
 * \param[in,out] head List head buffer is attached to.
 * \param[in] dir Traversal direction (PPL_DIR_DOWNSTREAM or PPL_DIR_UPSTREAM).
 */
void buffer_detach(struct comp_buffer *buffer, struct list_item *head, int dir);

/**
 * \brief Get component connected to buffer in specified traversal direction.
 * \param[in] buffer Component buffer.
 * \param[in] dir Traversal direction (PPL_DIR_DOWNSTREAM or PPL_DIR_UPSTREAM).
 * \return Pointer to connected component device.
 */
static inline struct comp_dev *buffer_get_comp(struct comp_buffer *buffer, int dir)
{
	struct comp_dev *comp = (dir == PPL_DIR_DOWNSTREAM) ?
					comp_buffer_get_sink_component(buffer) :
					comp_buffer_get_source_component(buffer);
	return comp;
}

/**
 * \brief Retrieve pipeline ID associated with a component buffer.
 * \param[in] buffer Component buffer.
 * \return Pipeline ID.
 */
static inline uint32_t buffer_pipeline_id(const struct comp_buffer *buffer)
{
	return buffer->stream.runtime_stream_params.pipeline_id;
}

/**
 * \brief Initialize circular stream buffer descriptor for a component buffer.
 * \param[in,out] buffer Component buffer to initialize stream for.
 * \param[in] size Capacity of the circular stream in bytes.
 */
static inline void buffer_init_stream(struct comp_buffer *buffer, size_t size)
{
	/* addr should be set in alloc function */
	audio_stream_init(&buffer->stream, buffer->stream.addr, size);
}

/** @}*/

#endif /* __SOF_AUDIO_BUFFER_H__ */
