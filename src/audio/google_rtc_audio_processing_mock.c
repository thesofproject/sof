// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Google LLC.
//
// Author: Lionel Koenig <lionelk@google.com>
#include "google_rtc_audio_processing.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sof/lib/alloc.h"
#include "ipc/topology.h"

#define GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ 48000

struct GoogleRtcAudioProcessingState {
	int num_capture_channels;
	int num_aec_reference_channels;
	int num_output_channels;
	int num_frames;
	int16_t *aec_reference;
};

GoogleRtcAudioProcessingState *GoogleRtcAudioProcessingCreate()
{
	struct GoogleRtcAudioProcessingState *s =
			rballoc(0, SOF_MEM_CAPS_RAM, sizeof(GoogleRtcAudioProcessingState));
	if (!s)
		return NULL;
	s->num_capture_channels = 1;
	s->num_aec_reference_channels = 2;
	s->num_output_channels = 1;
	s->num_frames = GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ * 10 / 1000;
	s->aec_reference = rballoc(0,
				   SOF_MEM_CAPS_RAM,
				   sizeof(s->aec_reference[0]) *
				   s->num_frames *
				   s->num_aec_reference_channels
				   );
	if (!s->aec_reference) {
		rfree(s);
		return NULL;
	}
	return s;
}

void GoogleRtcAudioProcessingFree(GoogleRtcAudioProcessingState *state)
{
	if (state != NULL) {
		rfree(state->aec_reference);
		rfree(state);
	}
}

int GoogleRtcAudioProcessingGetFramesizeInMs(GoogleRtcAudioProcessingState *state)
{
	return state->num_frames * 1000 / GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ;
}

int GoogleRtcAudioProcessingReconfigure(
	GoogleRtcAudioProcessingState * const state, const uint8_t *const config,
	int config_size)
{
	return 0;
}

int GoogleRtcAudioProcessingProcessCapture_int16(
	GoogleRtcAudioProcessingState *const state, const int16_t *const src,
	int16_t *const dest)
{
	int16_t *ref = state->aec_reference;
	int16_t *mic = (int16_t *) src;
	int16_t *out = dest;
	int n;

	for (n = 0; n < state->num_frames; ++n) {
		*out = *mic + *ref;
		ref += state->num_aec_reference_channels;
		out += state->num_output_channels;
		mic += state->num_capture_channels;
	}
	return 0;
}

int GoogleRtcAudioProcessingAnalyzeRender_int16(
	GoogleRtcAudioProcessingState *const state, const int16_t *const data)
{
	const size_t buffer_size =
			sizeof(state->aec_reference[0])
			* state->num_frames
			* state->num_aec_reference_channels;
	memcpy_s(state->aec_reference, buffer_size,
		 data, buffer_size);
	return 0;
}
