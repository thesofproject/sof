// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Google LLC.
//
// Author: Lionel Koenig <lionelk@google.com>
#include <google_rtc_audio_processing.h>
#include <google_rtc_audio_processing_sof_message_reader.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sof/audio/format.h>
#include <sof/math/numbers.h>

#include <rtos/alloc.h>
#include <ipc/topology.h>

#define GOOGLE_RTC_AUDIO_PROCESSING_FREQENCY_TO_PERIOD_FRAMES 100
#define GOOGLE_RTC_AUDIO_PROCESSING_MS_PER_SECOND 1000

struct GoogleRtcAudioProcessingState {
	int num_capture_channels;
	int num_aec_reference_channels;
	int num_output_channels;
	int num_frames;
	float *aec_reference;
};

static void SetFormats(GoogleRtcAudioProcessingState *const state,
		       int capture_sample_rate_hz,
		       int num_capture_input_channels,
		       int num_capture_output_channels,
		       int render_sample_rate_hz,
		       int num_render_channels)
{
	state->num_capture_channels = num_capture_input_channels;
	state->num_output_channels = num_capture_output_channels;
	state->num_frames = capture_sample_rate_hz /
		GOOGLE_RTC_AUDIO_PROCESSING_FREQENCY_TO_PERIOD_FRAMES;

	state->num_aec_reference_channels = num_render_channels;
	rfree(state->aec_reference);
	state->aec_reference = rballoc(SOF_MEM_FLAG_USER,
				       sizeof(state->aec_reference[0]) *
				       state->num_frames *
				       state->num_aec_reference_channels);
}

void GoogleRtcAudioProcessingAttachMemoryBuffer(uint8_t *const buffer,
						int buffer_size)
{
}

void GoogleRtcAudioProcessingDetachMemoryBuffer(void)
{
}

GoogleRtcAudioProcessingState *GoogleRtcAudioProcessingCreateWithConfig(int capture_sample_rate_hz,
									int num_capture_input_channels,
									int num_capture_output_channels,
									int render_sample_rate_hz,
									int num_render_channels,
									const uint8_t *const config,
									int config_size)
{
	struct GoogleRtcAudioProcessingState *s =
		rballoc(SOF_MEM_FLAG_USER, sizeof(GoogleRtcAudioProcessingState));
	if (!s)
		return NULL;

	s->aec_reference = NULL;
	SetFormats(s,
		   capture_sample_rate_hz,
		   num_capture_input_channels,
		   num_capture_output_channels,
		   render_sample_rate_hz,
		   num_render_channels);

	if (!s->aec_reference) {
		rfree(s);
		return NULL;
	}
	return s;
}

GoogleRtcAudioProcessingState *GoogleRtcAudioProcessingCreate(void)
{
	return GoogleRtcAudioProcessingCreateWithConfig(CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							1,
							1,
							CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ,
							2,
							NULL,
							0);
}

void GoogleRtcAudioProcessingFree(GoogleRtcAudioProcessingState *state)
{
	if (state != NULL) {
		rfree(state->aec_reference);
		rfree(state);
	}
}

int GoogleRtcAudioProcessingSetStreamFormats(GoogleRtcAudioProcessingState *const state,
					     int capture_sample_rate_hz,
					     int num_capture_input_channels,
					     int num_capture_output_channels,
					     int render_sample_rate_hz,
					     int num_render_channels)
{
	SetFormats(state,
		   capture_sample_rate_hz,
		   num_capture_input_channels,
		   num_capture_output_channels,
		   render_sample_rate_hz,
		   num_render_channels);
	return 0;
}

int GoogleRtcAudioProcessingParameters(GoogleRtcAudioProcessingState *const state,
				       float *capture_headroom_linear,
				       float *echo_path_delay_ms)
{
	return 0;
}

int GoogleRtcAudioProcessingGetFramesizeInMs(GoogleRtcAudioProcessingState *state)
{
	return state->num_frames *
		GOOGLE_RTC_AUDIO_PROCESSING_MS_PER_SECOND /
		CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING_SAMPLE_RATE_HZ;
}

int GoogleRtcAudioProcessingReconfigure(GoogleRtcAudioProcessingState *const state,
					const uint8_t *const config,
					int config_size)
{
	return 0;
}

int GoogleRtcAudioProcessingProcessCapture_float32(GoogleRtcAudioProcessingState *const state,
						   const float *const *src,
						   float * const *dest)
{
	float *ref = state->aec_reference;
	float **mic = (float **)src;
	int n, chan;

	for (chan = 0; chan < state->num_output_channels; chan++) {
		for (n = 0; n < state->num_frames; ++n) {
			float mic_save = mic[chan][n];	/* allow same in/out buffer */

			if (chan < state->num_aec_reference_channels)
				dest[chan][n] = mic_save + ref[n + (chan * state->num_frames)];
			else
				dest[chan][n] = mic_save;
		}
	}
	return 0;
}

int GoogleRtcAudioProcessingAnalyzeRender_float32(GoogleRtcAudioProcessingState *const state,
						  const float *const *data)
{
	const size_t buffer_size =
		sizeof(state->aec_reference[0])
		* state->num_frames;
	int channel;

	for (channel = 0; channel < state->num_aec_reference_channels; channel++) {
		memcpy_s(&state->aec_reference[channel * state->num_frames], buffer_size,
			 data[channel], buffer_size);
	}

	return 0;
}

void GoogleRtcAudioProcessingParseSofConfigMessage(uint8_t *message,
						   size_t message_size,
						   uint8_t **google_rtc_audio_processing_config,
						   size_t *google_rtc_audio_processing_config_size,
						   int *num_capture_input_channels,
						   int *num_capture_output_channels,
						   float *aec_reference_delay,
						   float *mic_gain,
						   bool *google_rtc_audio_processing_config_present,
						   bool *num_capture_input_channels_present,
						   bool *num_capture_output_channels_present,
						   bool *aec_reference_delay_present,
						   bool *mic_gain_present)
{
	*google_rtc_audio_processing_config = NULL;
	*google_rtc_audio_processing_config_size = 0;
	*num_capture_input_channels = 1;
	*num_capture_output_channels = 1;
	*aec_reference_delay = 0;
	*mic_gain = 1;
	*google_rtc_audio_processing_config_present = false;
	*num_capture_input_channels_present = false;
	*num_capture_output_channels_present = false;
	*aec_reference_delay_present = false;
	*mic_gain_present = false;
}
