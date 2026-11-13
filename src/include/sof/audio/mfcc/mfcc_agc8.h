/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

/**
 * \file mfcc_agc8.h
 * \brief Soft AGC and int8 quantization of MFCC Mel log spectra.
 *
 * A light alternative to PCAN for int8 Mel feature output. A single gain
 * is added to all Q9.23 Mel log values of a hop. The gain attacks
 * instantly so that the hop peak stays at or below +1.0 and releases
 * slowly towards the target, slower during speech. The result is clamped
 * to -1.0 .. +1.0 and converted to int8 Q1.7.
 */

#ifndef __SOF_AUDIO_MFCC_MFCC_AGC8_H__
#define __SOF_AUDIO_MFCC_MFCC_AGC8_H__

#include <stdbool.h>
#include <stdint.h>

struct sof_mfcc_config;

/** \brief Default gain target in Q9.23, 0.25 */
#define MFCC_AGC8_TARGET_Q23		2097152
/** \brief Default gain floor in Q9.23, -2.0 */
#define MFCC_AGC8_FLOOR_Q23		-16777216
/** \brief Default release step per hop in Q9.23 during silence */
#define MFCC_AGC8_RELEASE_Q23		4194
/** \brief Default release step per hop in Q9.23 during speech */
#define MFCC_AGC8_RELEASE_SPEECH_Q23	419

/** \brief Soft AGC state */
struct mfcc_agc8 {
	int32_t gain;		/**< Current gain in Q9.23 */
	int32_t target;		/**< Gain target in Q9.23 */
	int32_t floor;		/**< Minimum gain in Q9.23 */
	int32_t release;	/**< Release step per hop in Q9.23 during silence */
	int32_t release_speech;	/**< Release step per hop in Q9.23 during speech */
};

/**
 * \brief Initialize soft AGC parameters from the configuration blob.
 *
 * Zero valued blob parameters select the defaults. The gain is set to
 * the target.
 *
 * \param[out] agc Soft AGC state.
 * \param[in] config MFCC configuration blob.
 */
void mfcc_agc8_init(struct mfcc_agc8 *agc, const struct sof_mfcc_config *config);

/**
 * \brief Apply soft AGC to one hop of Mel log values and convert to int8.
 *
 * \param[in,out] agc Soft AGC state.
 * \param[in] mel Mel log values in Q9.23.
 * \param[out] out int8 Q1.7 output values.
 * \param[in] n Number of Mel values.
 * \param[in] speech True when VAD reports speech for this hop.
 */
void mfcc_agc8_process(struct mfcc_agc8 *agc, const int32_t *mel, int8_t *out, int n,
		       bool speech);

#endif /* __SOF_AUDIO_MFCC_MFCC_AGC8_H__ */
