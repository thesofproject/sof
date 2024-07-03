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

#define BUFFER_TYPE_LEGACY_BUFFER 1
#define BUFFER_TYPE_LEGACY_RING_HYBRID 2
#define BUFFER_TYPE_RING_BUFFER 3

/* base class for all buffers, all buffers must inherit from it */
struct sof_audio_buffer {
	CORE_CHECK_STRUCT_FIELD;

	/* type of the buffer BUFFER_TYPE_* */
	uint32_t buffer_type;

	/* runtime stream params */
	struct sof_audio_stream_params audio_stream_params;

	/* private: */
	struct sof_source _source_api;  /**< src api handler */
	struct sof_sink _sink_api;      /**< sink api handler */

	/* virtual methods */
	/**
	 * @brief this method must free all structures allocated by buffer implementation
	 *	  it must not free the buffer memory itself
	 */
	void (*free)(struct sof_audio_buffer *buffer);
};

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

/**
 * @brief return a pointer to struct sof_audio_buffer from sink pointer
 *	  NOTE! ensure that sink is really provided by sof_audio_buffer
 *	  otherwise a random value will be returned
 */
static inline struct sof_audio_buffer *sof_audo_buffer_from_sink(struct sof_sink *sink)
{
	return container_of(sink, struct sof_audio_buffer, _sink_api);
}

/**
 * @brief return a pointer to struct sof_audio_buffer from source pointer
 *	  NOTE! ensure that source is really provided by sof_audio_buffer
 *	  otherwise a random value will be returned
 */
static inline struct sof_audio_buffer *sof_audo_buffer_from_source(struct sof_source *source)
{
	return container_of(source, struct sof_audio_buffer, _source_api);
}

/**
 * @brief free buffer and all allocated resources
 */
static inline
void audio_buffer_free(struct sof_audio_buffer *buffer)
{
	if (!buffer)
		return;

	CORE_CHECK_STRUCT(buffer);
	if (buffer->free)
		buffer->free(buffer);
	rfree(buffer);
}

#endif /* __SOF_AUDIO_BUFFER__ */
