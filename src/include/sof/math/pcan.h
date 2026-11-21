/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
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
#include "tensorflow/lite/experimental/microfrontend/lib/log_scale.h"

#define PCAN_SNR_BITS			kPcanSnrBits		/* 12 */
#define PCAN_OUTPUT_BITS		kPcanOutputBits		/* 6 */
#define PCAN_SMOOTHING_COEF_BITS	kNoiseReductionBits	/* 14 */
#define PCAN_WIDE_DYNAMIC_BITS		kWideDynamicFunctionBits /* 32 */
#define PCAN_LUT_SIZE			kWideDynamicFunctionLUTSize /* 125 */

/* Google derives this as log2(fft_size) - 1 - kFilterbankBits / 2 because its
 * fixed-point FFT keeps a 1/N scale. SOF compensates the FFT length in the Mel
 * filter bank and scales mel_linear to Google's 512-point range, so it is 3 for
 * every FFT size.
 */
#define PCAN_INPUT_CORRECTION_BITS	3

/**
 * \brief SOF PCAN configuration structure.
 */
struct pcan_config {
	int32_t strength;		/**< Exponent alpha in Q1.15 (e.g. 31130 for 0.95) */
	int32_t offset;			/**< Additive offset delta in Q8.7 (e.g. 10240 for 80.0) */
	int32_t gain_bits;		/**< Gain bit shift (e.g. 21) */
	uint16_t even_smoothing;	/**< Even-channel IIR smoothing in Q14 (e.g. 410 for 0.025) */
	uint16_t odd_smoothing;		/**< Odd-channel IIR smoothing in Q14 (e.g. 983 for 0.06) */
	uint16_t min_signal_remaining;	/**< Minimum signal floor in Q14 (e.g. 819 for 0.05) */
	uint16_t smoothing_bits;	/**< Bit shift for noise estimate (e.g. 10) */
	int32_t input_correction_bits;	/**< Input correction bits shift (e.g. 3) */
	bool enable_pcan;		/**< Enable flag */
};

/**
 * \brief SOF PCAN runtime state wrapper.
 */
struct pcan_state {
	struct PcanGainControlState g_pcan;	/**< Google upstream PCAN state */
	struct NoiseReductionState g_noise;	/**< Google upstream Noise Reduction state */
	struct LogScaleState g_log_scale;	/**< Google upstream LogScale state */
	uint32_t *noise_estimate;		/**< Pointer to noise estimate buffer */
	int16_t *gain_lut;			/**< Pointer to gain LUT */
	int num_channels;			/**< Number of channels */
	int32_t snr_shift;			/**< SNR bit shift */
	int32_t input_correction_bits;		/**< Input correction bits for log scale */
	uint16_t smoothing_bits;		/**< Smoothing bits */
	bool enable_pcan;			/**< PCAN enabled */
	bool allocated_noise;			/**< Internally allocated noise buffer */
	bool allocated_lut;			/**< Internally allocated gain LUT buffer */
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
 *
 * \param strength Exponent alpha in Q1.15.
 * \param offset Additive offset delta in Q8.7.
 * \param gain_bits Fractional bit count for the gain output.
 * \param input_bits Bit shift applied to input energy.
 * \param x Input noise estimate value.
 */
int16_t pcan_gain_lookup_function(int32_t strength, int32_t offset, int32_t gain_bits,
				  int32_t input_bits, uint32_t x);

/**
 * \brief Fill 125-entry gain lookup table for WideDynamicFunction.
 *
 * \param strength Exponent alpha in Q1.15.
 * \param offset Additive offset delta in Q8.7.
 * \param gain_bits Fractional bit count for the gain output.
 * \param input_bits Bit shift applied to input energy.
 * \param gain_lut Output lookup table buffer of size PCAN_LUT_SIZE.
 */
int pcan_compute_lut(int32_t strength, int32_t offset, int32_t gain_bits,
		     int32_t input_bits, int16_t *gain_lut);

/**
 * \brief Initialize PCAN state with caller-supplied buffers.
 *
 * On host/unit-test builds (`CONFIG_LIBRARY` or `UNIT_TEST`), `NULL` buffer
 * arguments fall back to internal heap allocation freed by pcan_free_state().
 * On firmware builds, caller-supplied buffers are required.
 *
 * \param config PCAN and noise reduction configuration.
 * \param state PCAN runtime state to initialize.
 * \param noise_estimate_buffer Buffer of size num_channels uint32_t elements.
 * \param gain_lut_buffer Buffer of size PCAN_LUT_SIZE int16_t elements.
 * \param num_channels Number of filterbank channels.
 */
int pcan_populate_state(const struct pcan_config *config, struct pcan_state *state,
			uint32_t *noise_estimate_buffer, int16_t *gain_lut_buffer,
			int num_channels);

/**
 * \brief Free allocated resources in PCAN state.
 */
void pcan_free_state(struct pcan_state *state);

/**
 * \brief Reset PCAN temporal state.
 */
void pcan_reset(struct pcan_state *state);

/**
 * \brief Apply noise reduction and update noise estimate using Google's NoiseReductionApply.
 */
static inline void pcan_noise_reduction(struct pcan_state *state, uint32_t *signal)
{
	if (state && state->enable_pcan)
		NoiseReductionApply(&state->g_noise, signal);
}

/**
 * \brief Apply PCAN gain control and compression using Google's PcanGainControlApply.
 */
static inline void pcan_apply(struct pcan_state *state, uint32_t *signal)
{
	if (state && state->enable_pcan)
		PcanGainControlApply(&state->g_pcan, signal);
}

/**
 * \brief Apply Google's fixed-point logarithm and scale to signal in-place.
 */
void pcan_log_scale(struct pcan_state *state, uint32_t *signal);

#endif /* __SOF_MATH_PCAN_H__ */
