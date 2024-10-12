/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 Google LLC.
 *
 * Author: Eddy Hsu <eddyhsu@google.com>
 */
#ifndef GOOGLE_CTC_AUDIO_PROCESSING_H
#define GOOGLE_CTC_AUDIO_PROCESSING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GoogleCtcAudioProcessingState GoogleCtcAudioProcessingState;

// Creates an instance of GoogleCtcAudioProcessing with the tuning embedded in
// the library. If creation fails, NULL is returned.
GoogleCtcAudioProcessingState *GoogleCtcAudioProcessingCreate(void);

GoogleCtcAudioProcessingState *GoogleCtcAudioProcessingCreateWithConfig(int chunk_frames,
									int sample_rate,
									const uint8_t *config,
									int config_size);

// Frees all allocated resources in `state`.
void GoogleCtcAudioProcessingFree(GoogleCtcAudioProcessingState *state);

// Apply CTC on `src` and produce result in `dest`.
void GoogleCtcAudioProcessingProcess(GoogleCtcAudioProcessingState *state,
				     const float *src, float *dest,
				     int num_frames, int num_channels);

// Reconfigure the audio processing.
// Returns 0 if success and non zero if failure.
int GoogleCtcAudioProcessingReconfigure(GoogleCtcAudioProcessingState *state,
					const uint8_t *config, int config_size);

#ifdef __cplusplus
}
#endif

#endif  // GOOGLE_CTC_AUDIO_PROCESSING_H
