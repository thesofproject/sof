// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Antigravity AI & SOF Team

#include <sof/math/pcan.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

int16_t pcan_gain_lookup_function(float strength, float offset, int32_t gain_bits,
				  int32_t input_bits, uint32_t x)
{
	struct PcanGainControlConfig config;
	config.strength = strength;
	config.offset = offset;
	config.gain_bits = gain_bits;
	return PcanGainLookupFunction(&config, input_bits, x);
}

int pcan_compute_lut(float strength, float offset, int32_t gain_bits,
		     int32_t input_bits, int16_t *gain_lut)
{
	struct PcanGainControlConfig config;
	struct PcanGainControlState state;
	int ret;

	if (!gain_lut)
		return -EINVAL;

	config.enable_pcan = 1;
	config.strength = strength;
	config.offset = offset;
	config.gain_bits = gain_bits;

	ret = PcanGainControlPopulateState(&config, &state, NULL, 1,
					   (uint16_t)input_bits, 0);
	if (!ret)
		return -EINVAL;

	memcpy(gain_lut, state.gain_lut, PCAN_LUT_SIZE * sizeof(int16_t));
	PcanGainControlFreeStateContents(&state);
	return 0;
}

int pcan_populate_state(const struct pcan_config *config, struct pcan_state *state,
			uint32_t *noise_estimate_buffer, int16_t *gain_lut_buffer,
			int num_channels, uint16_t smoothing_bits,
			int32_t input_correction_bits)
{
	struct PcanGainControlConfig g_config;
	int ret;

	if (!config || !state || num_channels <= 0)
		return -EINVAL;

	memset(state, 0, sizeof(*state));
	state->enable_pcan = config->enable_pcan;
	if (!state->enable_pcan)
		return 0;

	state->num_channels = num_channels;
	state->smoothing_bits = smoothing_bits;
	state->smoothing_coef = (config->smoothing_coef > 0) ? config->smoothing_coef : 819;
	state->one_minus_smoothing_coef = (1 << PCAN_SMOOTHING_COEF_BITS) - state->smoothing_coef;
	state->snr_shift = config->gain_bits - input_correction_bits - PCAN_SNR_BITS;
	if (state->snr_shift < 0)
		return -EINVAL;

	if (noise_estimate_buffer) {
		state->noise_estimate = noise_estimate_buffer;
	} else {
		state->noise_estimate = malloc(num_channels * sizeof(uint32_t));
		if (!state->noise_estimate)
			return -ENOMEM;
		state->allocated = true;
	}
	memset(state->noise_estimate, 0, num_channels * sizeof(uint32_t));

	g_config.enable_pcan = 1;
	g_config.strength = config->strength;
	g_config.offset = config->offset;
	g_config.gain_bits = config->gain_bits;

	if (gain_lut_buffer) {
		/* Use caller-supplied buffer */
		state->gain_lut = gain_lut_buffer;
		ret = pcan_compute_lut(config->strength, config->offset, config->gain_bits,
				       (int32_t)smoothing_bits - input_correction_bits,
				       gain_lut_buffer);
		if (ret < 0) {
			pcan_free_state(state);
			return ret;
		}
		state->g_pcan.enable_pcan = 1;
		state->g_pcan.noise_estimate = state->noise_estimate;
		state->g_pcan.num_channels = num_channels;
		state->g_pcan.gain_lut = gain_lut_buffer;
		state->g_pcan.snr_shift = state->snr_shift;
	} else {
		/* Use Google's internal allocator */
		ret = PcanGainControlPopulateState(&g_config, &state->g_pcan,
						   state->noise_estimate,
						   num_channels, smoothing_bits,
						   input_correction_bits);
		if (!ret) {
			pcan_free_state(state);
			return -EINVAL;
		}
		state->gain_lut = state->g_pcan.gain_lut;
	}

	return 0;
}

void pcan_free_state(struct pcan_state *state)
{
	if (!state)
		return;

	if (!state->gain_lut && state->g_pcan.gain_lut)
		PcanGainControlFreeStateContents(&state->g_pcan);
	else if (state->g_pcan.gain_lut && state->gain_lut == state->g_pcan.gain_lut && state->allocated)
		PcanGainControlFreeStateContents(&state->g_pcan);

	if (state->allocated && state->noise_estimate) {
		free(state->noise_estimate);
		state->noise_estimate = NULL;
	}

	memset(state, 0, sizeof(*state));
}

void pcan_reset(struct pcan_state *state)
{
	if (!state || !state->noise_estimate)
		return;

	memset(state->noise_estimate, 0, state->num_channels * sizeof(uint32_t));
}

void pcan_update_noise_estimate(struct pcan_state *state, const uint32_t *signal)
{
	int i;
	const uint32_t smoothing = state->smoothing_coef;
	const uint32_t one_minus_smoothing = state->one_minus_smoothing_coef;
	const int smoothing_bits = state->smoothing_bits;

	for (i = 0; i < state->num_channels; ++i) {
		const uint32_t signal_scaled_up = signal[i] << smoothing_bits;
		const uint32_t estimate =
			(uint32_t)((((uint64_t)signal_scaled_up * smoothing) +
				    ((uint64_t)state->noise_estimate[i] * one_minus_smoothing)) >>
				   PCAN_SMOOTHING_COEF_BITS);
		state->noise_estimate[i] = estimate;
	}
}
