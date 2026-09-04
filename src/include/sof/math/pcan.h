/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Author: Antigravity AI & SOF Team
 */

/**
 * \file include/sof/math/pcan.h
 * \brief Per-Channel AGC Normalization (PCAN) library interface
 *
 * Wraps and integrates Google's upstream TFLite Micro microfrontend
 * library (under Apache-2.0 with patent grant protection).
 */

#ifndef __SOF_MATH_PCAN_H__
#define __SOF_MATH_PCAN_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tensorflow/lite/experimental/microfrontend/lib/bits.h"
#include "tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control.h"
#include "tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control_util.h"
#include "tensorflow/lite/experimental/microfrontend/lib/noise_reduction.h"
#include "tensorflow/lite/experimental/microfrontend/lib/noise_reduction_util.h"

#define PCAN_SNR_BITS			kPcanSnrBits		/* 12 */
#define PCAN_OUTPUT_BITS		kPcanOutputBits		/* 6 */
#define PCAN_SMOOTHING_COEF_BITS	kNoiseReductionBits	/* 14 */
#define PCAN_WIDE_DYNAMIC_BITS		kWideDynamicFunctionBits /* 32 */
#define PCAN_LUT_SIZE			kWideDynamicFunctionLUTSize /* 125 */

/* Aliases for Google Microfrontend Types */
typedef struct PcanGainControlConfig pcan_config_t;
typedef struct PcanGainControlState pcan_state_t;
typedef struct NoiseReductionConfig noise_reduction_config_t;
typedef struct NoiseReductionState noise_reduction_state_t;

/**
 * \brief SOF PCAN configuration structure.
 */
struct pcan_config {
	float strength;			/**< Exponent alpha (e.g. 0.95) */
	float offset;			/**< Additive offset delta (e.g. 80.0) */
	int32_t gain_bits;		/**< Gain bit shift (e.g. 21) */
	uint16_t smoothing_coef;	/**< IIR smoothing coefficient in Q14 (e.g. 819 for 0.05) */
	uint16_t smoothing_bits;	/**< Bit shift for noise estimate (e.g. 10) */
	int32_t input_correction_bits;	/**< Input correction bits shift (e.g. 0) */
	bool enable_pcan;		/**< Enable flag */
};

/**
 * \brief SOF PCAN runtime state wrapper.
 */
struct pcan_state {
	struct PcanGainControlState g_pcan;	/**< Google upstream PCAN state */
	struct NoiseReductionState g_noise;	/**< Google upstream Noise Reduction state */
	uint32_t *noise_estimate;		/**< Pointer to noise estimate buffer */
	int16_t *gain_lut;			/**< Pointer to gain LUT */
	int num_channels;			/**< Number of channels */
	int32_t snr_shift;			/**< SNR bit shift */
	uint16_t smoothing_coef;		/**< IIR smoothing coef in Q14 */
	uint16_t one_minus_smoothing_coef;	/**< (1 << 14) - smoothing_coef */
	uint16_t smoothing_bits;		/**< Smoothing bits */
	bool enable_pcan;			/**< PCAN enabled */
	bool allocated;				/**< Internally allocated buffers */
};

/**
 * \brief Evaluate Google's WideDynamicFunction.
 */
static inline int16_t pcan_wide_dynamic_function(uint32_t x, const int16_t *lut)
{
	return WideDynamicFunction(x, lut);
}

/**
 * \brief Evaluate Google's PcanShrink.
 */
static inline uint32_t pcan_shrink(uint32_t x)
{
	return PcanShrink(x);
}

/**
 * \brief Compute single point in continuous PCAN gain curve.
 */
int16_t pcan_gain_lookup_function(float strength, float offset, int32_t gain_bits,
				  int32_t input_bits, uint32_t x);

/**
 * \brief Fill 125-entry gain lookup table for WideDynamicFunction.
 */
int pcan_compute_lut(float strength, float offset, int32_t gain_bits,
		     int32_t input_bits, int16_t *gain_lut);

/**
 * \brief Initialize PCAN state with supplied buffers or allocate them.
 */
int pcan_populate_state(const struct pcan_config *config, struct pcan_state *state,
			uint32_t *noise_estimate_buffer, int16_t *gain_lut_buffer,
			int num_channels, uint16_t smoothing_bits,
			int32_t input_correction_bits);

/**
 * \brief Free allocated resources in PCAN state.
 */
void pcan_free_state(struct pcan_state *state);

/**
 * \brief Reset PCAN temporal state.
 */
void pcan_reset(struct pcan_state *state);

/**
 * \brief Update per-channel temporal noise estimate.
 */
void pcan_update_noise_estimate(struct pcan_state *state, const uint32_t *signal);

/**
 * \brief Apply PCAN gain control and compression using Google's PcanGainControlApply.
 */
static inline void pcan_apply(struct pcan_state *state, uint32_t *signal)
{
	if (state && state->enable_pcan)
		PcanGainControlApply(&state->g_pcan, signal);
}

#endif /* __SOF_MATH_PCAN_H__ */
