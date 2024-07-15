/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 Google LLC.
 *
 * Author: Eddy Hsu <eddyhsu@google.com>
 */
#ifndef GOOGLE_CTC_AUDIO_PROCESSING_SOF_MESSAGE_READER_H_
#define GOOGLE_CTC_AUDIO_PROCESSING_SOF_MESSAGE_READER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parses the fields from a SOF config message `message` of size `message_size`
// intended for the GoogleCtcAudioProcessing component and the corresponding
// outputs parameters based on the content. Any previous content in the output
// parameters is overwritten regardless of the content in `message`.
void GoogleCtcAudioProcessingParseSofConfigMessage(uint8_t *message,
						   size_t message_size,
						   uint8_t **config,
						   size_t *config_size,
						   bool *config_present);

#ifdef __cplusplus
}
#endif

#endif  // GOOGLE_CTC_AUDIO_PROCESSING_SOF_MESSAGE_READER_H_
