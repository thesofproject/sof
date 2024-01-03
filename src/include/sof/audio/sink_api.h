/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_SINK_API_H__
#define __SOF_SINK_API_H__

#include <module/audio/sink_api.h>

/**
 * Init of the API, must be called before any operation
 *
 * @param sink pointer to the structure
 * @param ops pointer to API operations
 * @param audio_stream_params pointer to structure with audio parameters
 *	  note that the audio_stream_params must be accessible by the caller core
 *	  the implementation must ensure coherent access to the parameteres
 *	  in case of i.e. cross core shared queue, it must be located in non-cached memory
 */
void sink_init(struct sof_sink *sink, const struct sink_ops *ops,
	       struct sof_audio_stream_params *audio_stream_params);

/**
 * Get total number of bytes processed by the sink (meaning - committed by sink_commit_buffer())
 *
 * @param sink a handler to sink
 * @return total number of processed data
 */
size_t sink_get_num_of_processed_bytes(struct sof_sink *sink);

/**
 * sets counter of total number of bytes processed  to zero
 */
void sink_reset_num_of_processed_bytes(struct sof_sink *sink);

/** set of functions for retrieve audio parameters */
bool sink_get_overrun(struct sof_sink *sink);

/** set of functions for setting audio parameters */
void sink_set_min_free_space(struct sof_sink *sink, size_t min_free_space);

#endif /* __SOF_SINK_API_H__ */
