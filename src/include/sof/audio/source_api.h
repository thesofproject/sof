/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

#ifndef __SOF_SOURCE_API_H__
#define __SOF_SOURCE_API_H__

#include <module/audio/source_api.h>

/**
 * Init of the API, must be called before any operation
 *
 * @param source pointer to the structure
 * @param ops pointer to API operations
 * @param audio_stream_params pointer to structure with audio parameters
 *	  note that the audio_stream_params must be accessible by the caller core
 *	  the implementation must ensure coherent access to the parameteres
 *	  in case of i.e. cross core shared queue, it must be located in non-cached memory
 */
void source_init(struct sof_source *source, const struct source_ops *ops,
		 struct sof_audio_stream_params *audio_stream_params);

/**
 * Get total number of bytes processed by the source (meaning - freed by source_release_data())
 */
size_t source_get_num_of_processed_bytes(struct sof_source *source);

/**
 * sets counter of total number of bytes processed  to zero
 */
void source_reset_num_of_processed_bytes(struct sof_source *source);

/** set of functions for retrieve audio parameters */
bool source_get_underrun(struct sof_source *source);
uint32_t source_get_id(struct sof_source *source);

/** set of functions for setting audio parameters */
int source_set_frm_fmt(struct sof_source *source, enum sof_ipc_frame frm_fmt);
int source_set_valid_fmt(struct sof_source *source,
			 enum sof_ipc_frame valid_sample_fmt);
int source_set_rate(struct sof_source *source, unsigned int rate);
int source_set_channels(struct sof_source *source, unsigned int channels);
int source_set_underrun(struct sof_source *source, bool underrun_permitted);
int source_set_buffer_fmt(struct sof_source *source, uint32_t buffer_fmt);
void source_set_min_available(struct sof_source *source, size_t min_available);

/**
 * initial set of audio parameters, provided in sof_ipc_stream_params
 *
 * @param source a handler to source
 * @param params the set of parameters
 * @param force_update tells the implementation that the params should override actual settings
 * @return 0 if success
 */
int source_set_params(struct sof_source *source,
		      struct sof_ipc_stream_params *params, bool force_update);

/**
 * Set frame_align_shift and frame_align of stream according to byte_align and
 * frame_align_req alignment requirement. Once the channel number,frame size
 * are determined, the frame_align and frame_align_shift are determined too.
 * these two feature will be used in audio_stream_get_avail_frames_aligned
 * to calculate the available frames. it should be called in component prepare
 * or param functions only once before stream copy. if someone forgets to call
 * this first, there would be unexampled error such as nothing is copied at all.
 *
 * @param source a handler to source
 * @param byte_align Processing byte alignment requirement.
 * @param frame_align_req Processing frames alignment requirement.
 *
 * @return 0 if success
 */
int source_set_alignment_constants(struct sof_source *source,
				   const uint32_t byte_align,
				   const uint32_t frame_align_req);

#endif /* __SOF_SOURCE_API_H__ */
