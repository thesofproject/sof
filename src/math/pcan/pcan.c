// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Antigravity AI & SOF Team

#include <sof/math/exp_fcn.h>
#include <sof/math/log.h>
#include <sof/math/pcan.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* ln(2) in Q5.27:  round(ln(2) * 2^27) */
#define PCAN_LN2_Q27		93032640

/* Convert PCAN config floats to fixed-point (init-time only). */
static inline int32_t pcan_strength_q15(float strength)
{
	return (int32_t)(strength * 32768.0f + 0.5f);
}

static inline int32_t pcan_offset_q7(float offset)
{
	return (int32_t)(offset * 128.0f + 0.5f);
}

int16_t pcan_gain_lookup_function(float strength, float offset, int32_t gain_bits,
				  int32_t input_bits, uint32_t x)
{
	const int32_t strength_q15 = pcan_strength_q15(strength);
	const int32_t offset_q7 = pcan_offset_q7(offset);
	uint64_t u64;
	uint32_t u32;
	uint32_t ln_u_q27;
	int32_t ln_actual_q27;
	int64_t s_ln;
	int32_t exp_arg_q27;
	int32_t exp_arg_ceiling;
	int32_t gain_q20;
	int32_t gain;

	/* u = x + offset * 2^input_bits, matches upstream's (x_as_float + offset) in fixed-point. */
	u64 = (uint64_t)x + (((uint64_t)offset_q7 << input_bits) >> 7);
	if (u64 == 0)
		u64 = 1;
	u32 = (u64 > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)u64;

	/* ln(u) as UQ5.27. */
	ln_u_q27 = ln_int32(u32);

	/* ln(x_as_float + offset) = ln(u) - input_bits * ln(2), in Q5.27. */
	ln_actual_q27 = (int32_t)ln_u_q27 - input_bits * PCAN_LN2_Q27;

	/* strength * ln(actual):  Q1.15 * Q5.27 -> Q6.42, shift down 15 -> Q5.27. */
	s_ln = ((int64_t)strength_q15 * (int64_t)ln_actual_q27) >> 15;

	/* exp_arg = gain_bits * ln(2) - strength * ln(actual), Q5.27. */
	exp_arg_q27 = gain_bits * PCAN_LN2_Q27 - (int32_t)s_ln;

	/* exp(10.3972) = 32767.5, above this the int16 clamp always fires.
	 * Q5.27: round(10.3972 * 2^27) = 1395385005.
	 */
	exp_arg_ceiling = 1395385005;
	if (exp_arg_q27 >= exp_arg_ceiling)
		return INT16_MAX;
	if (exp_arg_q27 <= -SOFM_EXP_FIXED_INPUT_MAX)
		return 0;

	/* sofm_exp_fixed returns Q12.20 for input Q5.27 in [-16, +7.6246]. Above the
	 * upper input bound the output saturates. Handle the intermediate range
	 * [7.6246, 10.3972] with an explicit split so we still get an accurate value.
	 */
	if (exp_arg_q27 > SOFM_EXP_FIXED_INPUT_MAX) {
		/* exp(a + b) = exp(a) * exp(b); pick b = 5.5 in Q5.27. */
		const int32_t half_q27 = 738197504;  /* round(5.5 * 2^27) */
		const int32_t half_exp_q20 = 256590991; /* round(exp(5.5) * 2^20) */
		int64_t full_q40;

		gain_q20 = sofm_exp_fixed(exp_arg_q27 - half_q27);
		/* Q12.20 * Q12.20 -> Q24.40. Do the >> 20 to Q(?)20 as int64. */
		full_q40 = (int64_t)gain_q20 * half_exp_q20;
		/* Round to integer directly to avoid intermediate int32 overflow. */
		gain = (int32_t)((full_q40 + ((int64_t)1 << 39)) >> 40);
		if (gain > INT16_MAX)
			return INT16_MAX;
		if (gain < 0)
			return 0;
		return (int16_t)gain;
	}

	gain_q20 = sofm_exp_fixed(exp_arg_q27);

	/* Round to nearest integer (matching upstream: gain_as_float + 0.5f, then cast). */
	gain = (gain_q20 + (1 << 19)) >> 20;
	if (gain > INT16_MAX)
		return INT16_MAX;
	if (gain < 0)
		return 0;
	return (int16_t)gain;
}

int pcan_compute_lut(float strength, float offset, int32_t gain_bits,
		     int32_t input_bits, int16_t *gain_lut)
{
	int interval;

	if (!gain_lut)
		return -EINVAL;

	memset(gain_lut, 0, PCAN_LUT_SIZE * sizeof(int16_t));

	/* Layout matches upstream PcanGainControlPopulateState(): entries 0 and 1
	 * hold the low-x samples, then for intervals 2..kWideDynamicFunctionBits
	 * the triplet (y0, a1, a2) is stored at (4 * interval - 6, -5, -4).
	 */
	gain_lut[0] = pcan_gain_lookup_function(strength, offset, gain_bits, input_bits, 0);
	gain_lut[1] = pcan_gain_lookup_function(strength, offset, gain_bits, input_bits, 1);

	for (interval = 2; interval <= kWideDynamicFunctionBits; ++interval) {
		const uint32_t x0 = (uint32_t)1 << (interval - 1);
		const uint32_t x1 = x0 + (x0 >> 1);
		const uint32_t x2 = (interval == kWideDynamicFunctionBits) ?
				    x0 + (x0 - 1) : 2 * x0;
		const int16_t y0 = pcan_gain_lookup_function(strength, offset, gain_bits,
							     input_bits, x0);
		const int16_t y1 = pcan_gain_lookup_function(strength, offset, gain_bits,
							     input_bits, x1);
		const int16_t y2 = pcan_gain_lookup_function(strength, offset, gain_bits,
							     input_bits, x2);
		const int32_t diff1 = (int32_t)y1 - y0;
		const int32_t diff2 = (int32_t)y2 - y0;
		const int32_t a1 = 4 * diff1 - diff2;
		const int32_t a2 = diff2 - a1;

		gain_lut[4 * interval - 6] = y0;
		gain_lut[4 * interval - 5] = (int16_t)a1;
		gain_lut[4 * interval - 4] = (int16_t)a2;
	}

	return 0;
}

int pcan_populate_state(const struct pcan_config *config, struct pcan_state *state,
			uint32_t *noise_estimate_buffer, int16_t *gain_lut_buffer,
			int num_channels, uint16_t smoothing_bits,
			int32_t input_correction_bits)
{
	int ret;

	if (!config || !state || num_channels <= 0)
		return -EINVAL;

	memset(state, 0, sizeof(*state));
	state->enable_pcan = config->enable_pcan;
	if (!state->enable_pcan)
		return 0;

	state->num_channels = num_channels;
	state->smoothing_bits = smoothing_bits;
	state->snr_shift = config->gain_bits - input_correction_bits - PCAN_SNR_BITS;
	if (state->snr_shift < 0)
		return -EINVAL;

	if (noise_estimate_buffer) {
		state->noise_estimate = noise_estimate_buffer;
	} else {
		state->noise_estimate = malloc(num_channels * sizeof(uint32_t));
		if (!state->noise_estimate)
			return -ENOMEM;
		state->allocated_noise = true;
	}
	memset(state->noise_estimate, 0, num_channels * sizeof(uint32_t));

	if (gain_lut_buffer) {
		state->gain_lut = gain_lut_buffer;
	} else {
		state->gain_lut = malloc(PCAN_LUT_SIZE * sizeof(int16_t));
		if (!state->gain_lut) {
			pcan_free_state(state);
			return -ENOMEM;
		}
		state->allocated_lut = true;
	}

	ret = pcan_compute_lut(config->strength, config->offset, config->gain_bits,
			       (int32_t)smoothing_bits - input_correction_bits,
			       state->gain_lut);
	if (ret < 0) {
		pcan_free_state(state);
		return ret;
	}

	/* Mirror the SOF state into the upstream struct consumed by PcanGainControlApply(). */
	state->g_pcan.enable_pcan = 1;
	state->g_pcan.noise_estimate = state->noise_estimate;
	state->g_pcan.num_channels = num_channels;
	state->g_pcan.gain_lut = state->gain_lut;
	state->g_pcan.snr_shift = state->snr_shift;

	state->g_noise.estimate = state->noise_estimate;
	state->g_noise.num_channels = num_channels;
	state->g_noise.smoothing_bits = smoothing_bits;
	state->g_noise.even_smoothing = 410; /* 0.025 * (1 << 14) */
	state->g_noise.odd_smoothing = 983;  /* 0.06 * (1 << 14) */
	state->g_noise.min_signal_remaining = 819; /* 0.05 * (1 << 14) */

	state->g_log_scale.enable_log = 1;
	state->g_log_scale.scale_shift = 6;

	return 0;
}

void pcan_free_state(struct pcan_state *state)
{
	if (!state)
		return;

	if (state->allocated_noise)
		free(state->noise_estimate);
	if (state->allocated_lut)
		free(state->gain_lut);

	memset(state, 0, sizeof(*state));
}

void pcan_reset(struct pcan_state *state)
{
	if (!state || !state->noise_estimate)
		return;

	memset(state->noise_estimate, 0, state->num_channels * sizeof(uint32_t));
}

void pcan_log_scale(struct pcan_state *state, uint32_t *signal)
{
	int i;
	uint16_t *scaled;

	if (!state || !state->enable_pcan)
		return;

	scaled = LogScaleApply(&state->g_log_scale, signal, state->num_channels, 3);
	for (i = state->num_channels - 1; i >= 0; --i)
		signal[i] = scaled[i];
}
