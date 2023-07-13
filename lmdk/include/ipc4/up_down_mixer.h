// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@intel.com>

#ifndef __SOF_IPC4_UP_DOWN_MIXER_H__
#define __SOF_IPC4_UP_DOWN_MIXER_H__

#include "base-config.h"

/**
 *  \brief bits field map which helps to describe each channel location a the data stream buffer
 */
typedef uint32_t channel_map;

enum up_down_mix_coeff_select {
	/**< module will use default coeffs */
	DEFAULT_COEFFICIENTS = 0,
	/**< custom coeffs are required */
	CUSTOM_COEFFICIENTS,
	/**< module will use default coeffs */
	DEFAULT_COEFFICIENTS_WITH_CHANNEL_MAP,
	/**< custom coeffs are required */
	CUSTOM_COEFFICIENTS_WITH_CHANNEL_MAP
};

#define UP_DOWN_MIX_COEFFS_LENGTH       8
#define IPC4_UP_DOWN_MIXER_MODULE_OUTPUT_PINS_COUNT	1

struct ipc4_up_down_mixer_module_cfg {
	struct ipc4_base_module_cfg base_cfg;

	/*
	 * Output Channel Configuration.
	 * Together with audio_fmt.channel_config determines module conversion ratio.
	 * Please note that UpDownMixer module does not support all conversions.
	 */
	enum ipc4_channel_config out_channel_config;

	/**< Selects which coeffs are going to be used by UpDownMixer. */
	enum up_down_mix_coeff_select coefficients_select;

	/*
	 * Optional, when coefficients_select == #CUSTOM_COEFFICIENTS. For
	 * #CUSTOM_COEFFICIENTS expect coeffs array in quantity equal to
	 * #UP_DOWN_MIX_COEFFS_LENGTH. Values in this array should be
	 * in range <#MIN_COEFF_VALUE, #MAX_COEFF_VALUE>.
	 *
	 * Coefficients should be in order:
	 * 1. Left
	 * 2. Center
	 * 3. Right
	 * 4. Left Surround
	 * 5. Right Surround
	 * 6. Low Frequency Effects
	 */
	int32_t coefficients[UP_DOWN_MIX_COEFFS_LENGTH];

	/*
	 * Optional, When coefficients_select is set to
	 * #DEFAULT_COEFFICIENTS_WITH_CHANNEL_MAP or
	 * #CUSTOM_COEFFICIENTS_WITH_CHANNEL_MAP, then it will be used for
	 * channel decoding.
	 */
	channel_map channel_map;
} __attribute__((packed)) __attribute__((__aligned__(8)));

#endif /* __SOF_IPC4_UP_DOWN_MIXER_H__ */
