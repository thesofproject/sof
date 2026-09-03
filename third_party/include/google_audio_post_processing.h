/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Google LLC.
 *
 * Author: Kehuang Li <kehuangli@google.com>
 */

#ifndef GOOGLE_AUDIO_POST_PROCESSING_H
#define GOOGLE_AUDIO_POST_PROCESSING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// This define ensure that the linked library matches the header file.
#define GoogleAudioPostProcessingCreate GoogleAudioPostProcessingCreate_v1

typedef struct GoogleAudioPostProcessingState GoogleAudioPostProcessingState;

struct GoogleAudioPostProcessingBuffer {
	uint16_t sample_size; // Bytes per sample, s16=>2, s32=>4.
	uint16_t channels; // Number of interleaved channels.
	uint32_t frames; // Total available frames.
	void *base_addr; // Start address of the circular buffer.
	void *head_ptr; // Current read/write position of the circular buffer.
	void *end_addr; // End address of the circular buffer.
};

// Creates an instance of GoogleAudioPostProcessing with the tuning embedded in
// the library.
GoogleAudioPostProcessingState *GoogleAudioPostProcessingCreate(void);

// Frees all allocated resources in `state` and deletes `state`.
void GoogleAudioPostProcessingDelete(GoogleAudioPostProcessingState *state);

// Setup or reconfigure the audio processing.
// Returns 0 if success and non zero if failure.
int GoogleAudioPostProcessingSetup(GoogleAudioPostProcessingState *const state,
				   int channels, int frames, int volume,
				   const uint8_t *const config,
				   int config_size);

// Pulls the current config (serialized) of the audio processing pipeline.
// If the config size is greater than `max_config_size`, following calls with
// `msg_index` > 0 can happen, and the implementation will maintain that internally.
// Returns config size if success and negative if failure.
int GoogleAudioPostProcessingGetConfig(
	GoogleAudioPostProcessingState *const state, int code, int msg_index,
	uint8_t *const config, int max_config_size);

// Accepts and produces a frame of interleaved 32 bit integer audio. `src` and
// `dest` may use the same memory, if desired.
// Returns 0 if success and non zero if failure.
int GoogleAudioPostProcessingProcess(
	GoogleAudioPostProcessingState *const state,
	const struct GoogleAudioPostProcessingBuffer *const src,
	struct GoogleAudioPostProcessingBuffer *const dest);

// Sets system volume.
// Returns volume if success and negative if failure.
int GoogleAudioPostProcessingSetVol(GoogleAudioPostProcessingState *const state,
				    const int *const volume, int num_channels);

// Gets current volume.
// Returns 0 if success and non zero if failure.
int GoogleAudioPostProcessingGetVol(GoogleAudioPostProcessingState *const state,
				    int *const volume, int num_channels);

#ifdef __cplusplus
}
#endif

#endif // GOOGLE_AUDIO_POST_PROCESSING_H
