// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@intel.com>
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

#include <../ipc4/up_down_mixer.h>
#include <../format.h>
#include <stdint.h>

#define COMPUTE_COEFF_32BIT(counter, denominator) ((0x7fffffffULL * (counter)) / (denominator))
#define COMPUTE_COEFF_16BIT(counter, denominator) ((0x7fffULL * (counter)) / (denominator))

const int32_t k_lo_ro_downmix32bit[UP_DOWN_MIX_COEFFS_LENGTH] = {
	COMPUTE_COEFF_32BIT(1, 1),      /* 1.0   - L */
	COMPUTE_COEFF_32BIT(707, 1000), /* 0.707 - Center */
	COMPUTE_COEFF_32BIT(1, 1),      /* 1.0   - R */
	COMPUTE_COEFF_32BIT(707, 1000), /* 0.707 - Ls */
	COMPUTE_COEFF_32BIT(707, 1000), /* 0.707 - Rs */
	COMPUTE_COEFF_32BIT(100, 1000), /* 0.100 - LS */
	COMPUTE_COEFF_32BIT(100, 1000), /* 0.100 - RS */
	COMPUTE_COEFF_32BIT(000, 1000), /* 0.000 - LFE */
};

const int32_t k_scaled_lo_ro_downmix32bit[UP_DOWN_MIX_COEFFS_LENGTH] = {
	COMPUTE_COEFF_32BIT(414, 1000), /* 0.414 - L */
	COMPUTE_COEFF_32BIT(293, 1000), /* 0.293 - Center */
	COMPUTE_COEFF_32BIT(414, 1000), /* 0.414 - R */
	COMPUTE_COEFF_32BIT(293, 1000), /* 0.293 - Ls */
	COMPUTE_COEFF_32BIT(293, 1000), /* 0.293 - Rs */
	COMPUTE_COEFF_32BIT(100, 1000), /* 0.100 - LS */
	COMPUTE_COEFF_32BIT(100, 1000), /* 0.100 - RS */
	COMPUTE_COEFF_32BIT(000, 1000), /* 0.000 - LFE */
};

const int32_t k_half_scaled_lo_ro_downmix32bit[UP_DOWN_MIX_COEFFS_LENGTH] = {
	COMPUTE_COEFF_32BIT(586, 1000), /* 0.586 - L */
	COMPUTE_COEFF_32BIT(414, 1000), /* 0.414 - Center */
	COMPUTE_COEFF_32BIT(586, 1000), /* 0.586 - R */
	COMPUTE_COEFF_32BIT(414, 1000), /* 0.414 - Ls */
	COMPUTE_COEFF_32BIT(414, 1000), /* 0.414 - Rs */
	COMPUTE_COEFF_32BIT(100, 1000), /* 0.100 - LS */
	COMPUTE_COEFF_32BIT(100, 1000), /* 0.100 - RS */
	COMPUTE_COEFF_32BIT(000, 1000), /* 0.000 - LFE */
};

const int32_t k_quatro_mono_scaled_lo_ro_downmix32bit[UP_DOWN_MIX_COEFFS_LENGTH] = {
	COMPUTE_COEFF_32BIT(293, 1000), /* 0.293 - L */
	COMPUTE_COEFF_32BIT(207, 1000), /* 0.207 - Center */
	COMPUTE_COEFF_32BIT(293, 1000), /* 0.293 - R */
	COMPUTE_COEFF_32BIT(207, 1000), /* 0.207 - Ls */
	COMPUTE_COEFF_32BIT(207, 1000), /* 0.207 - Rs */
	COMPUTE_COEFF_32BIT(100, 1000), /* 0.100 - LS */
	COMPUTE_COEFF_32BIT(100, 1000), /* 0.100 - RS */
	COMPUTE_COEFF_32BIT(000, 1000), /* 0.000 - LFE */
};

const int32_t k_lo_ro_downmix16bit[UP_DOWN_MIX_COEFFS_LENGTH] = {
	COMPUTE_COEFF_16BIT(1, 1),      /* 1.0   - L */
	COMPUTE_COEFF_16BIT(707, 1000), /* 0.707 - Center */
	COMPUTE_COEFF_16BIT(1, 1),      /* 1.0   - R */
	COMPUTE_COEFF_16BIT(707, 1000), /* 0.707 - Ls, Cs */
	COMPUTE_COEFF_16BIT(707, 1000), /* 0.707 - Rs */
	COMPUTE_COEFF_16BIT(100, 1000), /* 0.100 - LS */
	COMPUTE_COEFF_16BIT(100, 1000), /* 0.100 - RS */
	COMPUTE_COEFF_16BIT(000, 1000), /* 0.000 - LFE */
};

const int32_t k_scaled_lo_ro_downmix16bit[UP_DOWN_MIX_COEFFS_LENGTH] = {
	COMPUTE_COEFF_16BIT(414, 1000), /* 0.414 - L */
	COMPUTE_COEFF_16BIT(293, 1000), /* 0.293 - Center */
	COMPUTE_COEFF_16BIT(414, 1000), /* 0.414 - R */
	COMPUTE_COEFF_16BIT(293, 1000), /* 0.293 - Ls Cs */
	COMPUTE_COEFF_16BIT(293, 1000), /* 0.293 - Rs */
	COMPUTE_COEFF_16BIT(100, 1000), /* 0.100 - LS */
	COMPUTE_COEFF_16BIT(100, 1000), /* 0.100 - RS */
	COMPUTE_COEFF_16BIT(000, 1000), /* 0.000 - LFE */
};

const int32_t k_half_scaled_lo_ro_downmix16bit[UP_DOWN_MIX_COEFFS_LENGTH] = {
	COMPUTE_COEFF_16BIT(586, 1000), /* 0.586 - L */
	COMPUTE_COEFF_16BIT(414, 1000), /* 0.414 - Center */
	COMPUTE_COEFF_16BIT(586, 1000), /* 0.586 - R */
	COMPUTE_COEFF_16BIT(414, 1000), /* 0.414 - Ls Cs */
	COMPUTE_COEFF_16BIT(414, 1000), /* 0.414 - Rs */
	COMPUTE_COEFF_16BIT(100, 1000), /* 0.100 - LS */
	COMPUTE_COEFF_16BIT(100, 1000), /* 0.100 - RS */
	COMPUTE_COEFF_16BIT(000, 1000), /* 0.000 - LFE */
};

const int32_t k_quatro_mono_scaled_lo_ro_downmix16bit[UP_DOWN_MIX_COEFFS_LENGTH] = {
	COMPUTE_COEFF_16BIT(293, 1000), /* 0.293 - L */
	COMPUTE_COEFF_16BIT(207, 1000), /* 0.207 - Center */
	COMPUTE_COEFF_16BIT(293, 1000), /* 0.293 - R */
	COMPUTE_COEFF_16BIT(207, 1000), /* 0.207 - Ls */
	COMPUTE_COEFF_16BIT(207, 1000), /* 0.207 - Rs */
	COMPUTE_COEFF_16BIT(100, 1000), /* 0.100 - LS */
	COMPUTE_COEFF_16BIT(100, 1000), /* 0.100 - RS */
	COMPUTE_COEFF_16BIT(000, 1000), /* 0.000 - LFE */
};

