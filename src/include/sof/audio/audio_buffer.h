/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */
/**
 * @file sof/audio/audio_buffer.h
 * @brief Audio buffer operations, descriptors, and unified audio buffer interfaces.
 * @ingroup audio_buffer_api
 */

#ifndef __SOF_AUDIO_BUFFER__
#define __SOF_AUDIO_BUFFER__

#include <sof/common.h>
#include <ipc/topology.h>
#include <sof/coherent.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>

/** \addtogroup audio_buffer_api
 *  @{
 */

/** \name Buffer type identifiers
 *  @{
 */
#define BUFFER_TYPE_LEGACY_BUFFER 1	/**< Legacy comp_buffer type (pipeline 1.0) */
#define BUFFER_TYPE_RING_BUFFER 2	/**< Lockless async circular ring buffer type (cross-core) */
/** @}*/

/* forward def */
struct sof_audio_buffer;

/**
 * \brief Audio buffer virtual operations table.
 */
struct audio_buffer_ops {
	/**
	 * \brief Free all structures allocated by buffer implementation and the buffer itself.
	 *
	 * MANDATORY: audio_buffer_free() invokes this callback unconditionally.
	 *
	 * \param[in,out] buffer Buffer to free.
	 */
	void (*free)(struct sof_audio_buffer *buffer);

	/**
	 * \brief Clean all buffer data and reset positions to initial state, keeping configuration.
	 *
	 * OPTIONAL: procedure is to be called only when buffer is not in use.
	 *
	 * \param[in,out] buffer Buffer to reset.
	 */
	void (*reset)(struct sof_audio_buffer *buffer);

	/**
	 * \brief Notification callback invoked on audio format modification.
	 * \param[in,out] buffer Buffer whose format changed.
	 * \retval 0 On success.
	 * \retval negative If new parameters are not supported.
	 */
	int (*on_audio_format_set)(struct sof_audio_buffer *buffer);

	/**
	 * \brief Configure IPC stream parameters on buffer.
	 * \param[in,out] buffer Buffer to configure.
	 * \param[in] params Stream parameters to apply.
	 * \param[in] force_update If true, overwrite parameters unconditionally.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*audio_set_ipc_params)(struct sof_audio_buffer *buffer,
				    struct sof_ipc_stream_params *params, bool force_update);

	/**
	 * \brief Configure alignment constraints on buffer.
	 * \param[in,out] buffer Buffer to configure.
	 * \param[in] byte_align Byte alignment requirement in bytes.
	 * \param[in] frame_align_req Frame alignment requirement in frames.
	 * \return 0 on success, negative error code on failure.
	 */
	int (*set_alignment_constants)(struct sof_audio_buffer *buffer,
				       const uint32_t byte_align,
				       const uint32_t frame_align_req);
};

struct mod_alloc_ctx;

/**
 * \brief Base class for all audio buffers. All audio buffers must inherit from it.
 */
struct sof_audio_buffer {
	CORE_CHECK_STRUCT_FIELD;

	uint32_t buffer_type;	/**< Type of the buffer (BUFFER_TYPE_*) */

	bool is_shared;		/**< Buffer structure is shared between 2 cores */

#if CONFIG_PIPELINE_2_0
	/**
	 * Sink API of an additional buffer of any type at data input.
	 * To be removed when hybrid buffers are no longer needed.
	 */
	struct sof_audio_buffer *secondary_buffer_sink;

	/**
	 * Source API of an additional buffer at data output.
	 */
	struct sof_audio_buffer *secondary_buffer_source;

#endif /* CONFIG_PIPELINE_2_0 */

	/**
	 * Pointer to effective runtime stream parameters.
	 */
	struct sof_audio_stream_params *audio_stream_params;

	/* private: */
	struct sof_source _source_api;  /**< Source API interface handler */
	struct sof_sink _sink_api;      /**< Sink API interface handler */

	/* virtual methods */
	const struct audio_buffer_ops *ops; /**< Virtual methods table */

	bool walking;		/**< Indicates if the buffer is being walked in pipeline graph */

	struct mod_alloc_ctx *alloc;	/**< Allocation context */
};

#if CONFIG_PIPELINE_2_0
/**
 * \brief Attach a secondary buffer at data input or data output of primary buffer.
 * \param[in,out] buffer Pointer to primary audio buffer.
 * \param[in] at_input True if secondary buffer is placed at input, false if at output.
 * \param[in,out] secondary_buffer Pointer to secondary buffer to attach.
 * \return 0 on success, or negative error code on failure.
 */
int audio_buffer_attach_secondary_buffer(struct sof_audio_buffer *buffer, bool at_input,
					 struct sof_audio_buffer *secondary_buffer);

/**
 * \brief Synchronize data between primary and secondary hybrid buffers.
 * \param[in,out] buffer Pointer to audio buffer.
 * \param[in] limit Maximum number of bytes to move.
 * \return 0 on success, or negative error code on failure.
 */
int audio_buffer_sync_secondary_buffer(struct sof_audio_buffer *buffer, size_t limit);

/**
 * \brief Return pointer to sink interface of audio buffer.
 * \param[in] buffer Pointer to audio buffer.
 * \return Pointer to sink interface.
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
 * \brief Return pointer to source interface of audio buffer.
 * \param[in] buffer Pointer to audio buffer.
 * \return Pointer to source interface.
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
 * \brief Return pointer to sink interface of audio buffer.
 * \param[in] buffer Pointer to audio buffer.
 * \return Pointer to sink interface.
 */
static inline
struct sof_sink *audio_buffer_get_sink(struct sof_audio_buffer *buffer)
{
	CORE_CHECK_STRUCT(buffer);
	return &buffer->_sink_api;
}

/**
 * \brief Return pointer to source interface of audio buffer.
 * \param[in] buffer Pointer to audio buffer.
 * \return Pointer to source interface.
 */
static inline
struct sof_source *audio_buffer_get_source(struct sof_audio_buffer *buffer)
{
	CORE_CHECK_STRUCT(buffer);
	return &buffer->_source_api;
}

#endif /* CONFIG_PIPELINE_2_0 */

/**
 * \brief Check if audio buffer is shared across cores.
 * \param[in] buffer Pointer to audio buffer.
 * \return True if shared, false otherwise.
 */
static inline bool audio_buffer_is_shared(struct sof_audio_buffer *buffer)
{
	return buffer->is_shared;
}

/**
 * \brief Check if buffer hardware parameters are configured.
 * \param[in] buffer Pointer to audio buffer.
 * \return True if configured, false otherwise.
 */
static inline bool audio_buffer_hw_params_configured(struct sof_audio_buffer *buffer)
{
	return buffer->audio_stream_params->state != AUDIOBUF_STATE_INITIAL;
}

/**
 * \brief Mark buffer hardware parameters as configured (state set to READY).
 * \param[in,out] buffer Pointer to audio buffer.
 */
static inline void audio_buffer_set_hw_params_configured(struct sof_audio_buffer *buffer)
{
	buffer->audio_stream_params->state = AUDIOBUF_STATE_READY;
}

/**
 * \brief Reset buffer parameters state to initial.
 * \param[in,out] buffer Pointer to audio buffer.
 */
static inline void audio_buffer_reset_params(struct sof_audio_buffer *buffer)
{
	buffer->audio_stream_params->state = AUDIOBUF_STATE_INITIAL;
}

/**
 * \brief Retrieve current buffer state.
 * \param[in] buffer Pointer to audio buffer.
 * \return Current buffer state (enum sof_audio_buffer_state).
 */
static inline enum sof_audio_buffer_state audio_buffer_get_state(
	const struct sof_audio_buffer *buffer)
{
	return buffer->audio_stream_params->state;
}

/**
 * \brief Set buffer state.
 * \param[in,out] buffer Pointer to audio buffer.
 * \param[in] state New state to assign.
 */
static inline void audio_buffer_set_state(struct sof_audio_buffer *buffer,
					  enum sof_audio_buffer_state state)
{
	buffer->audio_stream_params->state = state;
}

/**
 * \brief Mark buffer state as end-of-stream (EOS).
 * \param[in,out] buffer Pointer to audio buffer.
 */
static inline void audio_buffer_set_eos(struct sof_audio_buffer *buffer)
{
	buffer->audio_stream_params->state = AUDIOBUF_STATE_END_OF_STREAM;
}

/**
 * \brief Retrieve channel map value for a given channel index.
 * \param[in] buffer Pointer to audio buffer.
 * \param[in] index Channel index.
 * \return Channel map value.
 */
static inline uint16_t audio_buffer_get_chmap(struct sof_audio_buffer *buffer, size_t index)
{
	return buffer->audio_stream_params->chmap[index];
}

/**
 * \brief Set channel map value for a given channel index.
 * \param[in,out] buffer Pointer to audio buffer.
 * \param[in] index Channel index.
 * \param[in] value Channel map value to set.
 */
static inline void audio_buffer_set_chmap(struct sof_audio_buffer *buffer, size_t index,
					  uint16_t value)
{
	buffer->audio_stream_params->chmap[index] = value;
}

/**
 * \brief Retrieve pointer to runtime stream parameters.
 * \param[in] buffer Pointer to audio buffer.
 * \return Pointer to runtime stream parameters.
 */
static inline
struct sof_audio_stream_params *audio_buffer_get_stream_params(struct sof_audio_buffer *buffer)
{
	return buffer->audio_stream_params;
}

/**
 * \brief Retrieve enclosing sof_audio_buffer from sink interface pointer.
 *
 * NOTE: Ensure that sink is really embedded within a sof_audio_buffer;
 * otherwise an invalid derived pointer will be returned.
 *
 * \param[in] sink Pointer to sink interface.
 * \return Pointer to enclosing struct sof_audio_buffer.
 */
static inline struct sof_audio_buffer *sof_audio_buffer_from_sink(struct sof_sink *sink)
{
	return container_of(sink, struct sof_audio_buffer, _sink_api);
}

/**
 * \brief Retrieve enclosing sof_audio_buffer from source interface pointer.
 *
 * NOTE: Ensure that source is really embedded within a sof_audio_buffer;
 * otherwise an invalid derived pointer will be returned.
 *
 * \param[in] source Pointer to source interface.
 * \return Pointer to enclosing struct sof_audio_buffer.
 */
static inline struct sof_audio_buffer *sof_audio_buffer_from_source(struct sof_source *source)
{
	return container_of(source, struct sof_audio_buffer, _source_api);
}

/**
 * \brief Initialize audio buffer structure and virtual operation tables.
 * \param[in,out] buffer Pointer to audio buffer.
 * \param[in] buffer_type Buffer type identifier (BUFFER_TYPE_*).
 * \param[in] is_shared True if buffer is shared between cores.
 * \param[in] source_ops Pointer to source operations table.
 * \param[in] sink_ops Pointer to sink operations table.
 * \param[in] audio_buffer_ops Pointer to audio buffer operations table.
 * \param[in,out] audio_stream_params Pointer to audio stream parameters.
 */
void audio_buffer_init(struct sof_audio_buffer *buffer, uint32_t buffer_type, bool is_shared,
		       const struct source_ops *source_ops, const struct sink_ops *sink_ops,
		       const struct audio_buffer_ops *audio_buffer_ops,
		       struct sof_audio_stream_params *audio_stream_params);

/**
 * \brief Free audio buffer and all allocated resources.
 * \param[in,out] buffer Pointer to audio buffer to free.
 */
void audio_buffer_free(struct sof_audio_buffer *buffer);

/**
 * \brief Reset all buffer data and positions leaving configuration intact.
 * \param[in,out] buffer Pointer to audio buffer.
 */
static inline
void audio_buffer_reset(struct sof_audio_buffer *buffer)
{
	if (buffer->ops->reset)
		buffer->ops->reset(buffer);

#if CONFIG_PIPELINE_2_0
	if (buffer->secondary_buffer_sink && buffer->secondary_buffer_sink->ops->reset)
		buffer->secondary_buffer_sink->ops->reset(buffer->secondary_buffer_sink);

	if (buffer->secondary_buffer_source && buffer->secondary_buffer_source->ops->reset)
		buffer->secondary_buffer_source->ops->reset(buffer->secondary_buffer_source);
#endif
}

/* Audio-buffer wrappers for the source-sink API */

/**
 * \brief Set IPC stream parameters via source interface wrapper.
 * \param[in,out] source Pointer to source interface.
 * \param[in] params Stream parameters to apply.
 * \param[in] force_update If true, overwrite parameters unconditionally.
 * \return 0 on success, or negative error code on failure.
 */
int audio_buffer_source_set_ipc_params(struct sof_source *source,
				       struct sof_ipc_stream_params *params, bool force_update);

/**
 * \brief Audio format set callback via source interface wrapper.
 * \param[in,out] source Pointer to source interface.
 * \return 0 on success, or negative error code on failure.
 */
int audio_buffer_source_on_audio_format_set(struct sof_source *source);

/**
 * \brief Set alignment constraints via source interface wrapper.
 * \param[in,out] source Pointer to source interface.
 * \param[in] byte_align Byte alignment requirement in bytes.
 * \param[in] frame_align_req Frame alignment requirement in frames.
 * \return 0 on success, or negative error code on failure.
 */
int audio_buffer_source_set_alignment_constants(struct sof_source *source,
						const uint32_t byte_align,
						const uint32_t frame_align_req);

/**
 * \brief Set IPC stream parameters via sink interface wrapper.
 * \param[in,out] sink Pointer to sink interface.
 * \param[in] params Stream parameters to apply.
 * \param[in] force_update If true, overwrite parameters unconditionally.
 * \return 0 on success, or negative error code on failure.
 */
int audio_buffer_sink_set_ipc_params(struct sof_sink *sink, struct sof_ipc_stream_params *params,
				     bool force_update);

/**
 * \brief Audio format set callback via sink interface wrapper.
 * \param[in,out] sink Pointer to sink interface.
 * \return 0 on success, or negative error code on failure.
 */
int audio_buffer_sink_on_audio_format_set(struct sof_sink *sink);

/**
 * \brief Set alignment constraints via sink interface wrapper.
 * \param[in,out] sink Pointer to sink interface.
 * \param[in] byte_align Byte alignment requirement in bytes.
 * \param[in] frame_align_req Frame alignment requirement in frames.
 * \return 0 on success, or negative error code on failure.
 */
int audio_buffer_sink_set_alignment_constants(struct sof_sink *sink,
					      const uint32_t byte_align,
					      const uint32_t frame_align_req);

/**
 * \brief Get linear free time (LFT) duration in microseconds from sink interface wrapper.
 * \param[in] sink Pointer to sink interface.
 * \return Buffered duration in microseconds (us_in_buffer) calculated from source available data and sample rate.
 */
uint32_t audio_buffer_sink_get_lft(struct sof_sink *sink);

/** @}*/

#endif /* __SOF_AUDIO_BUFFER__ */
