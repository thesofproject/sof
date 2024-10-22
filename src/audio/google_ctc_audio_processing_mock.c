// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Google LLC.
//
// Author: Eddy Hsu <eddyhsu@google.com>

#include <ipc/topology.h>
#include <sof/audio/buffer.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <google_ctc_audio_processing.h>

struct GoogleCtcAudioProcessingState {
};

GoogleCtcAudioProcessingState *GoogleCtcAudioProcessingCreate(void)
{
	struct GoogleCtcAudioProcessingState *s =
		rballoc(0, SOF_MEM_CAPS_RAM, sizeof(GoogleCtcAudioProcessingState));
	if (!s)
		return NULL;

	return s;
}

GoogleCtcAudioProcessingState *GoogleCtcAudioProcessingCreateWithConfig(int chunk_size,
									int sample_rate,
									const uint8_t *config,
									int config_size)
{
	struct GoogleCtcAudioProcessingState *s =
		rballoc(0, SOF_MEM_CAPS_RAM, sizeof(GoogleCtcAudioProcessingState));
	if (!s)
		return NULL;

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

int GoogleCtcAudioProcessingReconfigure(GoogleCtcAudioProcessingState *state,
					const uint8_t *config, int config_size)
{
	return 0;
}
