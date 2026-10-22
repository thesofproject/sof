// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Antigravity AI & SOF Team

#include <sof/audio/format.h>
#include <sof/math/pcan.h>
#include <stdint.h>

#if defined(PCAN_HIFI3)

#include <xtensa/tie/xt_hifi3.h>

int16_t pcan_wide_dynamic_function(uint32_t x, const int16_t *lut)
{
	if (x <= 2)
		return lut[x];

	/* HiFi fast leading zero count (MSB = 32 - clz) */
	ae_int32 x_reg = (ae_int32)x;
	int clz = AE_NSAU(x_reg);
	int interval = 32 - clz;
	const int16_t *base_lut = lut + (4 * interval - 6);

	int16_t frac;
	if (interval < 11)
		frac = (int16_t)((x << (11 - interval)) & 0x3FF);
	else
		frac = (int16_t)((x >> (interval - 11)) & 0x3FF);

	int32_t result = ((int32_t)base_lut[2] * frac) >> 5;
	result += (int32_t)((uint32_t)base_lut[1] << 5);
	result *= frac;
	result = (result + (1 << 14)) >> 15;
	result += base_lut[0];
	return (int16_t)result;
}

uint32_t pcan_shrink(uint32_t x)
{
	if (x < (2 << PCAN_SNR_BITS)) {
		return (uint32_t)(((uint64_t)x * x) >> (2 + 2 * PCAN_SNR_BITS - PCAN_OUTPUT_BITS));
	} else {
		return (x >> (PCAN_SNR_BITS - PCAN_OUTPUT_BITS)) - (1 << PCAN_OUTPUT_BITS);
	}
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

void pcan_apply(struct pcan_state *state, uint32_t *signal)
{
	int i;
	for (i = 0; i < state->num_channels; ++i) {
		const uint32_t gain = (uint32_t)(uint16_t)pcan_wide_dynamic_function(state->noise_estimate[i], state->gain_lut);
		const uint32_t snr = (uint32_t)(((uint64_t)signal[i] * gain) >> state->snr_shift);
		signal[i] = pcan_shrink(snr);
	}
}

#endif /* PCAN_HIFI3 */
