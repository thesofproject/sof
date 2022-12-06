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
// LINT.IfChange(api_version)
#define GoogleRtcAudioProcessingCreate GoogleRtcAudioProcessingCreate_v1
// LINT.ThenChange()

typedef struct GoogleRtcAudioProcessingState GoogleRtcAudioProcessingState;

// LINT.IfChange()

// Attaches `buffer` to use for memory allocations. The ownership of `buffer`
// remains within the caller.
void GoogleRtcAudioProcessingAttachMemoryBuffer(uint8_t *const buffer,
						int buffer_size);

// Detaches any attached memory buffer used for memory allocations. Returns 0 if
// success and non zero if failure.
void GoogleRtcAudioProcessingDetachMemoryBuffer(void);

// Creates an instance of GoogleRtcAudioProcessing with the tuning embedded in
// the library. If creation fails, NULL is returned.
GoogleRtcAudioProcessingState *GoogleRtcAudioProcessingCreate(void);

// Creates an instance of GoogleRtcAudioProcessing based on `config` and stream
// formats, where the content of config overrides any embedded parameters and
// the stream formats overrides any content in the config. Setting `config` to
// NULL means that no config is specified. If creation fails, NULL is returned.
GoogleRtcAudioProcessingState *GoogleRtcAudioProcessingCreateWithConfig(
    int capture_sample_rate_hz, int num_capture_input_channels,
    int num_capture_output_channels, int render_sample_rate_hz,
    int num_render_channels, const uint8_t *const config, int config_size);

// Frees all allocated resources in `state`.
void GoogleRtcAudioProcessingFree(GoogleRtcAudioProcessingState *state);

// Specifies the stream formats to use. Returns 0 if success and non zero if
// failure.
int GoogleRtcAudioProcessingSetStreamFormats(
    GoogleRtcAudioProcessingState *const state, int capture_sample_rate_hz,
    int num_capture_input_channels, int num_capture_output_channels,
    int render_sample_rate_hz, int num_render_channels);

// Specifies setup-specific parameters. Returns 0 if success and non zero if
// failure. Parameters which are NULL are ignored.
int GoogleRtcAudioProcessingParameters(
    GoogleRtcAudioProcessingState *const state, float *capture_headroom_linear,
    float *echo_path_delay_ms);

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

// LINT.ThenChange(:api_version)

#ifdef __cplusplus
}
#endif

#endif	// GOOGLE_RTC_AUDIO_PROCESSING_H
