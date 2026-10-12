/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

/**
 * \file sof/audio/source_api.h
 * \brief Audio source API helpers and stream configuration.
 * \ingroup module_api
 */

#ifndef __SOF_SOURCE_API_H__
#define __SOF_SOURCE_API_H__

#include <module/audio/source_api.h>

/** \addtogroup module_api
 *  @{
 */

/**
 * \brief Initialize source API structure before any operation.
 *
 * NOTE: audio_stream_params must be accessible by the caller core, and the
 * implementation must ensure coherent access to the parameters (e.g. for a
 * cross-core shared queue, it must be located in non-cached memory).
 *
 * \param[in,out] source Pointer to source structure.
 * \param[in] ops Pointer to API operations vtable.
 * \param[in] audio_stream_params Pointer to structure with audio parameters.
 */
void source_init(struct sof_source *source, const struct source_ops *ops,
		 struct sof_audio_stream_params *audio_stream_params);

/**
 * \brief Get total number of bytes processed by the source (freed by source_release_data()).
 * \param[in] source Pointer to source structure.
 * \return Total number of processed bytes.
 */
size_t source_get_num_of_processed_bytes(struct sof_source *source);

/**
 * \brief Reset counter of total processed bytes to zero.
 * \param[in,out] source Pointer to source structure.
 */
void source_reset_num_of_processed_bytes(struct sof_source *source);

/**
 * \brief Check if buffer underrun is permitted on source stream.
 * \param[in] source Pointer to source structure.
 * \return True if underrun is permitted by audio stream params, false otherwise.
 */
bool source_get_underrun(struct sof_source *source);

/**
 * \brief Get unique stream identifier for source.
 * \param[in] source Pointer to source structure.
 * \return Stream identifier.
 */
uint32_t source_get_id(struct sof_source *source);

/**
 * \brief Set audio frame format for source.
 * \param[in,out] source Pointer to source structure.
 * \param[in] frm_fmt Audio frame format.
 * \return 0 on success, negative error code on failure.
 */
int source_set_frm_fmt(struct sof_source *source, enum sof_ipc_frame frm_fmt);

/**
 * \brief Set valid sample format for source.
 * \param[in,out] source Pointer to source structure.
 * \param[in] valid_sample_fmt Valid sample format.
 * \return 0 on success, negative error code on failure.
 */
int source_set_valid_fmt(struct sof_source *source,
			 enum sof_ipc_frame valid_sample_fmt);

/**
 * \brief Set sample rate for source.
 * \param[in,out] source Pointer to source structure.
 * \param[in] rate Sample rate in Hz.
 * \return 0 on success, negative error code on failure.
 */
int source_set_rate(struct sof_source *source, unsigned int rate);

/**
 * \brief Set channel count for source.
 * \param[in,out] source Pointer to source structure.
 * \param[in] channels Number of audio channels.
 * \return 0 on success, negative error code on failure.
 */
int source_set_channels(struct sof_source *source, unsigned int channels);

/**
 * \brief Configure whether underrun is permitted on source.
 * \param[in,out] source Pointer to source structure.
 * \param[in] underrun_permitted True if underrun is permitted, false otherwise.
 * \return 0 on success, negative error code on failure.
 */
int source_set_underrun(struct sof_source *source, bool underrun_permitted);

/**
 * \brief Set buffer format for source.
 * \param[in,out] source Pointer to source structure.
 * \param[in] buffer_fmt Buffer format identifier.
 * \return 0 on success, negative error code on failure.
 */
int source_set_buffer_fmt(struct sof_source *source, uint32_t buffer_fmt);

/**
 * \brief Set minimum available data requirement in bytes.
 * \param[in,out] source Pointer to source structure.
 * \param[in] min_available Minimum available bytes.
 */
void source_set_min_available(struct sof_source *source, size_t min_available);

/**
 * \brief Configure initial set of audio parameters.
 * \param[in,out] source Pointer to source structure.
 * \param[in] params Audio stream parameters.
 * \param[in] force_update True to force parameters override.
 * \return 0 on success, negative error code on failure.
 */
int source_set_params(struct sof_source *source,
		      struct sof_ipc_stream_params *params, bool force_update);

/**
 * \brief Set byte and frame alignment constants for source stream.
 *
 * Sets frame_align_shift and frame_align of stream according to byte_align and
 * frame_align_req requirements. Once the channel number and frame size are
 * determined, the frame_align and frame_align_shift are determined too.
 * These features are used by audio_stream_get_avail_frames_aligned() to
 * calculate available frames.
 *
 * @note This must be called in component prepare or param functions only once
 *       after channel count and frame size are known and before stream copying begins.
 *       If omitted or called late, aligned availability can evaluate to zero and
 *       no data will be copied.
 *
 * \param[in,out] source Pointer to source structure.
 * \param[in] byte_align Processing byte alignment requirement.
 * \param[in] frame_align_req Processing frames alignment requirement.
 * \return 0 on success, negative error code on failure.
 */
int source_set_alignment_constants(struct sof_source *source,
				   const uint32_t byte_align,
				   const uint32_t frame_align_req);

/** @}*/

#endif /* __SOF_SOURCE_API_H__ */
