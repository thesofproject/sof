/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_STREAM_COMMON_H__
#define __SOF_AUDIO_STREAM_COMMON_H__

#include <ipc/stream.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * set of parameters describing audio stream
 * this structure is shared between audio_stream.h and sink/source interface
 * TODO: compressed formats
 */
struct sof_audio_stream_params {
	enum sof_ipc_frame frame_fmt;	/**< Sample data format */
	enum sof_ipc_frame valid_sample_fmt;

	uint32_t rate;		/**< Number of data frames per second [Hz] */
	uint16_t channels;	/**< Number of samples in each frame */

	/** alignment limit of stream copy, this value indicates how many
	 * integer frames which can meet both byte align and frame align
	 * requirement. it should be set in component prepare or param functions
	 */
	uint16_t frame_align;

	/**
	 * alignment limit of stream copy, alignment is the frame_align_shift-th
	 * power of 2 bytes. it should be set in component prepare or param functions
	 */
	uint16_t frame_align_shift;

	bool overrun_permitted; /**< indicates whether overrun is permitted */
	bool underrun_permitted; /**< indicates whether underrun is permitted */

	uint32_t buffer_fmt; /**< enum sof_ipc_buffer_format */
};

#endif /* __SOF_AUDIO_STREAM_COMMON_H__ */
