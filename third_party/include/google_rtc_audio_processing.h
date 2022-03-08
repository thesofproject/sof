// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Google LLC.
//
// Author: Lionel Koenig <lionelk@google.com>
#ifndef GOOGLE_RTC_AUDIO_PROCESSING_H
#define GOOGLE_RTC_AUDIO_PROCESSING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// This define ensure that the linked library matches the header file.
#define GoogleRtcAudioProcessingCreate GoogleRtcAudioProcessingCreate_v1

typedef struct GoogleRtcAudioProcessingState GoogleRtcAudioProcessingState;

// Creates an instance of GoogleRtcAudioProcessing with the tuning embedded in
// the library.
GoogleRtcAudioProcessingState *GoogleRtcAudioProcessingCreate(void);

// Frees all allocated resources in `state`.
void GoogleRtcAudioProcessingFree(GoogleRtcAudioProcessingState *state);

// Returns the framesize used for processing.
int GoogleRtcAudioProcessingGetFramesizeInMs(
    GoogleRtcAudioProcessingState *const state);

// Reconfigure the audio processing.
// Returns 0 if success and non zero if failure.
int GoogleRtcAudioProcessingReconfigure(
    GoogleRtcAudioProcessingState *const state, const uint8_t *const config,
    int config_size);

// Processes the microphone stream.
// Accepts deinterleaved float audio with the range [-1, 1]. Each element of
// |src| points to an array of samples for the channel. At output, the channels
// will be in |dest|.
// Returns 0 if success and non zero if failure.
int GoogleRtcAudioProcessingProcessCapture_float32(
    GoogleRtcAudioProcessingState *const state, const float *const *src,
    float *const *dest);

// Accepts and and produces a frame of interleaved 16 bit integer audio. `src`
// and `dest` may use the same memory, if desired.
// Returns 0 if success and non zero if failure.
int GoogleRtcAudioProcessingProcessCapture_int16(
    GoogleRtcAudioProcessingState *const state, const int16_t *const src,
    int16_t *const dest);

// Analyzes the playback stream.
// Accepts deinterleaved float audio with the range [-1, 1]. Each element
// of |src| points to an array of samples for the channel.
// Returns 0 if success and non zero if failure.
int GoogleRtcAudioProcessingAnalyzeRender_float32(
    GoogleRtcAudioProcessingState *const state, const float *const *src);

// Accepts interleaved int16 audio.
// Returns 0 if success and non zero if failure.
int GoogleRtcAudioProcessingAnalyzeRender_int16(
    GoogleRtcAudioProcessingState *const state, const int16_t *const src);

#ifdef __cplusplus
}
#endif

#endif  // GOOGLE_RTC_AUDIO_PROCESSING_H
