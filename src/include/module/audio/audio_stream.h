/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 *	   Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_AUDIO_AUDIO_STREAM_H__
#define __MODULE_AUDIO_AUDIO_STREAM_H__

#include <stdint.h>
#include <stdbool.h>
#include <rtos/panic.h>
#include "../ipc/stream.h"


/**
 * @enum sof_audio_buffer_state
 * @brief Define states of an audio stream buffer connecting two components.
 *
 * This enum represents the lifecycle of an audio stream, including its
 * initialization, readiness, and end-of-stream handling. It is used to
 * track and manage the state transitions of the stream during audio processing.
 */
enum sof_audio_buffer_state {
	AUDIOBUF_STATE_INITIAL,			/* Initial state, hw params not configured. */
	AUDIOBUF_STATE_READY,			/* Stream ready, hw params configured */
	AUDIOBUF_STATE_END_OF_STREAM,		/* Detected End Of Stream */
	AUDIOBUF_STATE_END_OF_STREAM_FLUSH,	/* Detected End Of Stream, generating silence
						 * to flush buffers in dp modules.
						 */
};

/**
 * set of parameters describing audio stream
 * this structure is shared between audio_stream.h and sink/source interface
 * TODO: compressed formats
 */
struct sof_audio_stream_params {
	uint32_t id;
	uint32_t pipeline_id;
	enum sof_ipc_frame frame_fmt;	/**< Sample data format */
	enum sof_ipc_frame valid_sample_fmt;

	uint32_t rate;		/**< Number of data frames per second [Hz] */
	uint16_t channels;	/**< Number of samples in each frame */

	/**
	 * align_frame_cnt indicates minimum number of frames that satisfies both byte
	 * align and frame align requirements. E.g: Consider an algorithm that processes
	 * in blocks of 3 frames configured to process 16-bit stereo using xtensa HiFi3
	 * SIMD. Therefore with 16-bit stereo we have a frame size of 4 bytes, and
	 * SIMD intrinsic requirement of 8 bytes(2 frames) for HiFi3 and an algorithim
	 * requirement of 3 frames. Hence the common processing block size has to align
	 * with frame(1), intrinsic(2) and algorithm (3) giving us an optimum processing
	 * block size of 6 frames.
	 */
	uint16_t align_frame_cnt;

	/**
	 * the free/available bytes of sink/source right shift align_shift_idx, the result
	 * multiplied by align_frame_cnt is the frame count free/available that can meet
	 * the align requirement.
	 */
	uint16_t align_shift_idx;

	bool overrun_permitted; /**< indicates whether overrun is permitted */
	bool underrun_permitted; /**< indicates whether underrun is permitted */

	uint32_t buffer_fmt; /**< enum sof_ipc_buffer_format */

	uint16_t chmap[SOF_IPC_MAX_CHANNELS];	/**< channel map - SOF_CHMAP_ */

	enum sof_audio_buffer_state state;	/**< audio stream state */
};

/**
 * @brief Read-only view of source data in a circular buffer.
 *
 * Describes a contiguous fragment of a circular buffer obtained from the source
 * API together with the buffer boundaries needed for wrap handling. All pointers
 * are const because the source data must not be modified.
 */
struct cir_buf_source {
	const void *buf_start;	/**< Start address of the circular buffer. */
	const void *buf_end;	/**< End address of the circular buffer. */
	const void *ptr;	/**< Current read pointer within the buffer. */
};

/**
 * @brief Writable view of sink data in a circular buffer.
 *
 * Describes a contiguous fragment of a circular buffer obtained from the sink
 * API together with the buffer boundaries needed for wrap handling.
 */
struct cir_buf_sink {
	void *buf_start;	/**< Start address of the circular buffer. */
	void *buf_end;		/**< End address of the circular buffer. */
	void *ptr;		/**< Current write pointer within the buffer. */
};

/**
 * @brief Calculates numbers of s16 samples to buffer wrap.
 * @param ptr Read or write pointer of circular buffer.
 * @param buf_start Start address of circular buffer.
 * @param buf_samples Total size of circular buffer in samples.
 * @return Number of samples to buffer wrap.
 */
static inline size_t cir_buf_samples_to_wrap_s16(const int16_t *ptr, const int16_t *buf_start,
						 size_t buf_samples)
{
	const int16_t *const buf_end = buf_start + buf_samples;

	assert(buf_end >= ptr);

	return buf_end - ptr;
}

/**
 * @brief Calculates numbers of s32 samples to buffer wrap.
 * @param ptr Read or write pointer of circular buffer.
 * @param buf_start Start address of circular buffer.
 * @param buf_samples Total size of circular buffer in samples.
 * @return Number of samples to buffer wrap.
 */
static inline size_t cir_buf_samples_to_wrap_s32(const int32_t *ptr, const int32_t *buf_start,
						 size_t buf_samples)
{
	const int32_t *const buf_end = buf_start + buf_samples;

	assert(buf_end >= ptr);

	return buf_end - ptr;
}

#endif /* __MODULE_AUDIO_AUDIO_STREAM_H__ */
