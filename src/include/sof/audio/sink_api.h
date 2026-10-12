/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

/**
 * \file sof/audio/sink_api.h
 * \brief Audio sink API helpers and stream configuration.
 * \ingroup module_api
 */

#ifndef __SOF_SINK_API_H__
#define __SOF_SINK_API_H__

#include <module/audio/sink_api.h>

/** \addtogroup module_api
 *  @{
 */

/**
 * \brief Initialize sink API structure before any operation.
 *
 * NOTE: audio_stream_params must be accessible by the caller core, and the
 * implementation must ensure coherent access to the parameters (e.g. for a
 * cross-core shared queue, it must be located in non-cached memory).
 *
 * \param[in,out] sink Pointer to sink structure.
 * \param[in] ops Pointer to API operations vtable.
 * \param[in] audio_stream_params Pointer to structure with audio parameters.
 */
void sink_init(struct sof_sink *sink, const struct sink_ops *ops,
	       struct sof_audio_stream_params *audio_stream_params);

/**
 * \brief Get total number of bytes processed by the sink (committed by sink_commit_buffer()).
 * \param[in] sink Pointer to sink structure.
 * \return Total number of processed bytes.
 */
size_t sink_get_num_of_processed_bytes(struct sof_sink *sink);

/**
 * \brief Reset counter of total processed bytes to zero.
 * \param[in,out] sink Pointer to sink structure.
 */
void sink_reset_num_of_processed_bytes(struct sof_sink *sink);

/**
 * \brief Check if buffer overrun is permitted on sink stream.
 * \param[in] sink Pointer to sink structure.
 * \return True if overrun is permitted by audio stream params, false otherwise.
 */
bool sink_get_overrun(struct sof_sink *sink);

/**
 * \brief Set minimum free space required in sink buffer.
 * \param[in,out] sink Pointer to sink structure.
 * \param[in] min_free_space Minimum free space in bytes.
 */
void sink_set_min_free_space(struct sof_sink *sink, size_t min_free_space);

/** @}*/

#endif /* __SOF_SINK_API_H__ */
