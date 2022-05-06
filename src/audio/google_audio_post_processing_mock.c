// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Google LLC.
//
// Author: Kehuang Li <kehuangli@google.com>

#include "google_audio_post_processing.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sof/common.h"
#include "sof/lib/alloc.h"
#include "ipc/topology.h"

#define GOOGLE_AUDIO_POST_PROCESSING_SAMPLE_RATE_HZ 48000

struct GoogleAudioPostProcessingState {
	uint8_t config[8];
	int num_channels;
	int num_frames;
	int volume[SOF_IPC_MAX_CHANNELS];
};

GoogleAudioPostProcessingState *GoogleAudioPostProcessingCreate(void)
{
	struct GoogleAudioPostProcessingState *s =
		rballoc(0, SOF_MEM_CAPS_RAM, sizeof(GoogleAudioPostProcessingState));

	if (!s)
		return NULL;

	s->num_channels = 1;
	s->num_frames = 0;
	return s;
}

void GoogleAudioPostProcessingDelete(GoogleAudioPostProcessingState *state)
{
	if (state != NULL)
		rfree(state);
}

int GoogleAudioPostProcessingSetup(GoogleAudioPostProcessingState *const state, int channels,
				   int frames, int volume, const uint8_t *const config,
				   int config_size)
{
	if (config_size > sizeof(state->config))
		return -ENOMEM;

	return memcpy_s(state->config, sizeof(state->config), config, config_size);
}

int GoogleAudioPostProcessingGetConfig(GoogleAudioPostProcessingState *const state, int code,
				       int msg_index, uint8_t *const config, int max_config_size)
{
	int config_size = sizeof(state->config);
	int ret;

	if (max_config_size < config_size)
		return -ENOMEM;

	ret = memcpy_s(config, max_config_size, state->config, config_size);
	if (ret)
		return ret;

	return config_size;
}

int GoogleAudioPostProcessingProcess(GoogleAudioPostProcessingState *const state,
				     const struct GoogleAudioPostProcessingBuffer *const src,
				     struct GoogleAudioPostProcessingBuffer *const dest)
{
	int32_t *r_ptr = src->head_ptr;
	int32_t *w_ptr = dest->head_ptr;
	int n;
	int c;

	for (n = 0; n < src->frames; ++n) {
		for (c = 0; c < src->channels; ++c) {
			*w_ptr = *r_ptr;
			r_ptr++;
			w_ptr++;
		}
		if (r_ptr >= (int32_t *)src->end_addr) {
			r_ptr = (int32_t *)src->base_addr + (r_ptr - (int32_t *)src->end_addr);
		}
		if (w_ptr >= (int32_t *)dest->end_addr) {
			w_ptr = (int32_t *)dest->base_addr + (w_ptr - (int32_t *)dest->end_addr);
		}
	}
	return 0;
}

int GoogleAudioPostProcessingSetVol(GoogleAudioPostProcessingState *const state,
				    const int *const volume, int num_channels)
{
	int j;

	if (num_channels > ARRAY_SIZE(state->volume))
		return -ENOMEM;

	for (j = 0; j < num_channels; j++)
		state->volume[j] = volume[j];
	state->num_channels = num_channels;

	return num_channels;
}

int GoogleAudioPostProcessingGetVol(GoogleAudioPostProcessingState *const state, int *const volume,
				    int num_channels)
{
	int j;

	if (num_channels < state->num_channels)
		return -ENOMEM;

	for (j = 0; j < state->num_channels; j++)
		volume[j] = state->volume[j];

	return 0;
}
