// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Google LLC.
//
// Author: Eddy Hsu <eddyhsu@google.com>
#include "google_ctc_audio_processing.h"
#include "google_ctc_audio_processing_sof_message_reader.h"

#include <rtos/alloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ipc/topology.h"

struct GoogleCtcAudioProcessingState {
	int num_frames;
	int partition_size;
	int impulse_size;
	int sample_rate;
	int is_symmetric;
};

GoogleCtcAudioProcessingState *GoogleCtcAudioProcessingCreate(void)
{
	struct GoogleCtcAudioProcessingState *s =
		rballoc(0, SOF_MEM_CAPS_RAM, sizeof(GoogleCtcAudioProcessingState));
	if (!s)
		return NULL;

	s->num_frames = CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING_NUM_FRAMES;
	s->partition_size = 64;
	s->impulse_size = 256;
	s->sample_rate = 48000;
	s->is_symmetric = 0;
	return s;
}

GoogleCtcAudioProcessingState *GoogleCtcAudioProcessingCreateWithConfig(
	int num_frames, int partition_size, int impulse_size, int sample_rate,
	int is_symmetric, const uint8_t *config, int config_size)
{
	struct GoogleCtcAudioProcessingState *s =
		rballoc(0, SOF_MEM_CAPS_RAM, sizeof(GoogleCtcAudioProcessingState));
	if (!s)
		return NULL;

	s->num_frames = num_frames;
	s->partition_size = partition_size;
	s->impulse_size = impulse_size;
	s->sample_rate = sample_rate;
	s->is_symmetric = is_symmetric;
	return s;
}

void GoogleCtcAudioProcessingFree(GoogleCtcAudioProcessingState *state)
{
	rfree(state);
}

void GoogleCtcAudioProcessingProcess(GoogleCtcAudioProcessingState *state,
				     const float *src, float *dest,
				     int num_frames, int num_channels)
{
	memcpy_s(dest, sizeof(float) * num_frames * num_channels,
		 src, sizeof(float) * num_frames * num_channels);
}

void GoogleCtcAudioProcessingParseSofConfigMessage(
	uint8_t *message, size_t message_size,
	uint8_t **config, size_t *config_size, bool *config_present)
{
	*config = NULL;
	*config_size = 0;
	*config_present = false;
}

int GoogleCtcAudioProcessingReconfigure(GoogleCtcAudioProcessingState *state,
					const uint8_t *config, int config_size)
{
	return 0;
}
