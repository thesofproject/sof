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

// Contains the content produced by the parsing of a SOF config message done by
// `GoogleRtcAudioProcessingParseSofConfigMessage`.
// `google_rtc_audio_processing_config` refers to the content of the `message`
// in the call to `GoogleRtcAudioProcessingParseSofConfigMessage` which means
// that it can only safely be used as long as the content in `message` remains
// valid.
struct ParsedGoogleRtcAudioProcessingSofConfigMessage {
  const uint8_t* google_rtc_audio_processing_config;
  size_t google_rtc_audio_processing_config_size;
  int num_capture_input_channels;
  int num_capture_output_channels;
  float aec_reference_delay;
  float mic_gain;
  bool google_rtc_audio_processing_config_present;
  bool num_capture_input_channels_present;
  bool num_capture_output_channels_present;
  bool aec_reference_delay_present;
  bool mic_gain_present;
};

// Parses the fields from a SOF config message `message` of size `message_size`
// intended for the GoogleRtcAudioProcessing component and populates
// `parsed_message` based on the content. Any previous content in
// `parsed_message` is overwritten regardless of the content in `message`.
void GoogleRtcAudioProcessingParseSofConfigMessage(
    const uint8_t* message, size_t message_size,
    struct ParsedGoogleRtcAudioProcessingSofConfigMessage* parsed_message);

#ifdef __cplusplus
}
#endif

#endif  // GOOGLE_RTC_AUDIO_PROCESSING_SOF_MESSAGE_READER_H_
