// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Google LLC.
//
// Author: Per Ahgren <peah@google.com>
#ifndef GOOGLE_RTC_AUDIO_PROCESSING_SOF_MESSAGE_READER_H_
#define GOOGLE_RTC_AUDIO_PROCESSING_SOF_MESSAGE_READER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parses the fields from a SOF config message `message` of size `message_size`
// intended for the GoogleRtcAudioProcessing component and the corresponding
// outputs parameters based on the content. Any previous content in the output
// parameters is overwritten regardless of the content in `message`.
void GoogleRtcAudioProcessingParseSofConfigMessage(
    uint8_t *message, size_t message_size,
    uint8_t **google_rtc_audio_processing_config,
    size_t *google_rtc_audio_processing_config_size,
    int *num_capture_input_channels, int *num_capture_output_channels,
    float *aec_reference_delay, float *mic_gain,
    bool *google_rtc_audio_processing_config_present,
    bool *num_capture_input_channels_present,
    bool *num_capture_output_channels_present,
    bool *aec_reference_delay_present, bool *mic_gain_present);

#ifdef __cplusplus
}
#endif

#endif  // GOOGLE_RTC_AUDIO_PROCESSING_SOF_MESSAGE_READER_H_
