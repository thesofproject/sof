/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */
#ifndef __SOF_AUDIO_BUFFER__
#define __SOF_AUDIO_BUFFER__

#include <sof/common.h>
#include <ipc/topology.h>
#include <sof/coherent.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>

#define BUFFER_TYPE_LEGACY_BUFFER 1
#define BUFFER_TYPE_RING_BUFFER 2

/* forward def */
struct sof_audio_buffer;

struct audio_buffer_ops {
	/**
	 * @brief this method must free all structures allocated by buffer implementation
	 *	  and the buffer itself
	 *	  OBLIGATORY
	 */
	void (*free)(struct sof_audio_buffer *buffer);

	/**
	 * @brief clean all buffer data, set buffer positions to initial, leaving config as is
	 *	  the procedure is to be called only when buffer is not in use
	 *	  OPTIONAL
	 */
	void (*reset)(struct sof_audio_buffer *buffer);

	/**
	 * OPTIONAL: Notification to the sink implementation about changes in audio format
	 *
	 * Once any of *audio_stream_params elements changes, the implementation of
	 * sink may need to perform some extra operations.
	 * This callback will be called immediately after any change
	 *
	 * @retval 0 if success, negative if new parameters are not supported
	 */
	int (*on_audio_format_set)(struct sof_audio_buffer *buffer);

	/**
	 * OPTIONAL
	 * see sink_set_params comments
	 */
	int (*audio_set_ipc_params)(struct sof_audio_buffer *buffer,
				    struct sof_ipc_stream_params *params, bool force_update);

	/**
	 * OPTIONAL
	 * see comment for sink_set_alignment_constants
	 */
	int (*set_alignment_constants)(struct sof_audio_buffer *buffer,
				       const uint32_t byte_align,
				       const uint32_t frame_align_req);
};

/* base class for all buffers, all buffers must inherit from it */
struct sof_audio_buffer {
	CORE_CHECK_STRUCT_FIELD;

	/* type of the buffer BUFFER_TYPE_* */
	uint32_t buffer_type;

	bool is_shared;   /* buffer structure is shared between 2 cores */

#if CONFIG_PIPELINE_2_0
	/**
	 * sink API of an additional buffer
	 * of any type at data input
	 *
	 * to be removed when hybrid buffers are no longer needed
	 */
	struct sof_audio_buffer *secondary_buffer_sink;

	/**
	 * source API of an additional buffer
	 * at data output
	 */
	struct sof_audio_buffer *secondary_buffer_source;

#endif /* CONFIG_PIPELINE_2_0 */

	/* effective runtime stream params
	 * before pipelin2.0 is ready, stream params may be kept in audio_stream structure
	 * also for hybrid buffering audio params need to be shared between primary and secondary
	 * buffers
	 *
	 * So currently only a pointer to effective stream params is here.
	 * Note that the same pointer MUST be set in source and sink api (kept there for
	 * performance reasons)
	 */
	struct sof_audio_stream_params *audio_stream_params;


	/* private: */
	struct sof_source _source_api;  /**< src api handler */
	struct sof_sink _sink_api;      /**< sink api handler */

	/* virtual methods */
	const struct audio_buffer_ops *ops;

	/*
	 * legacy params, needed for pipeline binding/iterating, not for data buffering
	 * should not be in struct sof_audio_buffer at all, kept for pipeline2.0 transition
	 */
	bool walking;		/**< indicates if the buffer is being walked */
};

#if CONFIG_PIPELINE_2_0
/*
 * attach a secondary buffer (any type) before buffer (when at_input == true) or behind a buffer
 *
 * before buffer (at_input == true):
 *  2.0 mod ==> (sink_API) secondary buffer ==>
 *			==> comp_buffer (audio_stream or source API) ==> 1.0 mod
 *
 * after buffer (at_input == false):
 *  1.0 mod ==> (audio_stream or sink API) ==> comp_buffer ==>
 *			==> secondary buffer(source API) == 2.0 mod
 *
 * If a secondary buffer is attached, it replaces source or sink interface of audio_stream
 * allowing the module connected to it using all properties of secondary buffer (like
 * lockless cross-core connection in case of ring_buffer etc.) keeping legacy interface
 * to other modules
 *
 * buffer_sync_secondary_buffer must be called every 1 ms to move data to/from
 * secondary buffer to comp_buffer
 *
 * @param buffer pointer to a buffer
 * @param at_input true indicates that a secondary buffer is located at data input, replacing
 *			sink API of audio_stream
 *		   false indicates that a secondary buffer is located at data output, replacing
 *			source API of audio_stream
 * @param secondary_buffer pointer to a buffer to be attached
 *
 * to be removed when hybrid buffers are no longer needed
 */
int audio_buffer_attach_secondary_buffer(struct sof_audio_buffer *buffer, bool at_input,
					 struct sof_audio_buffer *secondary_buffer);

/*
 * move data from/to secondary buffer, must be called periodically as described above
 *
 * @param buffer pointer to a buffer
 * @param limit data copy limit. Indicates maximum amount of data that will be moved from/to
 *		secondary buffer in an operation
 *
 * to be removed when hybrid buffers are no longer needed
 */
int audio_buffer_sync_secondary_buffer(struct sof_audio_buffer *buffer, size_t limit);

/**
 * @brief return a handler to sink API of audio_buffer.
 *		  the handler may be used by helper functions defined in sink_api.h
 */
static inline
struct sof_sink *audio_buffer_get_sink(struct sof_audio_buffer *buffer)
{
	CORE_CHECK_STRUCT(buffer);
	return buffer->secondary_buffer_sink ?
			audio_buffer_get_sink(buffer->secondary_buffer_sink) :
			&buffer->_sink_api;
}

/**
 * @brief return a handler to source API of audio_buffer
 *		  the handler may be used by helper functions defined in source_api.h
 */
static inline
struct sof_source *audio_buffer_get_source(struct sof_audio_buffer *buffer)
{
	CORE_CHECK_STRUCT(buffer);
	return buffer->secondary_buffer_source ?
			audio_buffer_get_source(buffer->secondary_buffer_source) :
			&buffer->_source_api;
}

#else /* CONFIG_PIPELINE_2_0 */

/**
 * @brief return a handler to sink API of audio_buffer.
 *		  the handler may be used by helper functions defined in sink_api.h
 */
static inline
struct sof_sink *audio_buffer_get_sink(struct sof_audio_buffer *buffer)
{
	CORE_CHECK_STRUCT(buffer);
	return &buffer->_sink_api;
}

/**
 * @brief return a handler to source API of audio_buffer
 *		  the handler may be used by helper functions defined in source_api.h
 */
static inline
struct sof_source *audio_buffer_get_source(struct sof_audio_buffer *buffer)
{
	CORE_CHECK_STRUCT(buffer);
	return &buffer->_source_api;
}

#endif /* CONFIG_PIPELINE_2_0 */

static inline bool audio_buffer_is_shared(struct sof_audio_buffer *buffer)
{
	return buffer->is_shared;
}

static inline bool audio_buffer_hw_params_configured(struct sof_audio_buffer *buffer)
{
	return buffer->audio_stream_params->hw_params_configured;
}

static inline void audio_buffer_set_hw_params_configured(struct sof_audio_buffer *buffer)
{
	buffer->audio_stream_params->hw_params_configured = true;
}

static inline void audio_buffer_reset_params(struct sof_audio_buffer *buffer)
{
	buffer->audio_stream_params->hw_params_configured = false;
}

static inline uint16_t audio_buffer_get_chmap(struct sof_audio_buffer *buffer, size_t index)
{
	return buffer->audio_stream_params->chmap[index];
}

static inline void audio_buffer_set_chmap(struct sof_audio_buffer *buffer, size_t index,
					  uint16_t value)
{
	buffer->audio_stream_params->chmap[index] = value;
}

/**
 * @brief return a handler to stream params structure
 */
static inline
struct sof_audio_stream_params *audio_buffer_get_stream_params(struct sof_audio_buffer *buffer)
{
	return buffer->audio_stream_params;
}

/**
 * @brief return a pointer to struct sof_audio_buffer from sink pointer
 *	  NOTE! ensure that sink is really provided by sof_audio_buffer
 *	  otherwise a random value will be returned
 */
static inline struct sof_audio_buffer *sof_audio_buffer_from_sink(struct sof_sink *sink)
{
	return container_of(sink, struct sof_audio_buffer, _sink_api);
}

/**
 * @brief return a pointer to struct sof_audio_buffer from source pointer
 *	  NOTE! ensure that source is really provided by sof_audio_buffer
 *	  otherwise a random value will be returned
 */
static inline struct sof_audio_buffer *sof_audio_buffer_from_source(struct sof_source *source)
{
	return container_of(source, struct sof_audio_buffer, _source_api);
}

/**
 * @brief initialize audio buffer structures
 *
 * @param buffer pointer to the audio_buffer
 * @param buffer_type a type of the buffer, BUFFER_TYPE_*
 * @param is_shared indicates if the buffer will be shared between cores
 * @param source_ops pointer to virtual methods table for source API
 * @param sink_ops pointer to virtual methods table for sink API
 * @param audio_buffer_ops pointer to required buffer virtual methods implementation
 * @param audio_stream_params pointer to audio stream (currently kept in buffer implementation)
 *
 * in sink_ops and source_ops API if there's no implemented of following methods:
 *   on_audio_format_set
 *   audio_set_ipc_params
 *   set_alignment_constants
 *
 * default implementation will be used instead, redirecting those calls to proper
 * audio_buffer_ops operations
 */
void audio_buffer_init(struct sof_audio_buffer *buffer, uint32_t buffer_type, bool is_shared,
		       struct source_ops *source_ops, struct sink_ops *sink_ops,
		       const struct audio_buffer_ops *audio_buffer_ops,
		       struct sof_audio_stream_params *audio_stream_params);

/**
 * @brief free buffer and all allocated resources
 */
void audio_buffer_free(struct sof_audio_buffer *buffer);

/**
 * @brief clean all buffer data, set buffer positions to initial, leaving config as is
 *	  the procedure is to be called only when buffer is not in use
 */
static inline
void audio_buffer_reset(struct sof_audio_buffer *buffer)
{
	if (buffer->ops->reset)
		buffer->ops->reset(buffer);
}

#endif /* __SOF_AUDIO_BUFFER__ */
