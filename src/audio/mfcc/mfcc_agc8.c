// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/**
 * \file mfcc_agc8.c
 * \brief Soft AGC and int8 quantization of MFCC Mel log spectra.
 */

#include <sof/audio/mfcc/mfcc_agc8.h>
#include <sof/audio/format.h>
#include <user/mfcc.h>
#include <stdint.h>

#define MFCC_AGC8_CLIP_MAX_Q23	(1 << 23)
#define MFCC_AGC8_CLIP_MIN_Q23	(-(1 << 23))

void mfcc_agc8_init(struct mfcc_agc8 *agc, const struct sof_mfcc_config *config)
{
	/* Blob target and floor are Q8.7, release steps are Q9.23 */
	agc->target = config->agc_target ? (int32_t)config->agc_target << 16 :
		MFCC_AGC8_TARGET_Q23;
	agc->floor = config->agc_floor ? (int32_t)config->agc_floor << 16 :
		MFCC_AGC8_FLOOR_Q23;
	agc->release = config->agc_release ? config->agc_release :
		MFCC_AGC8_RELEASE_Q23;
	agc->release_speech = config->agc_release_speech ? config->agc_release_speech :
		MFCC_AGC8_RELEASE_SPEECH_Q23;
	agc->gain = agc->target;
}

void mfcc_agc8_process(struct mfcc_agc8 *agc, const int32_t *mel, int8_t *out, int n,
		       bool speech)
{
	int32_t peak = mel[0];
	int32_t gain;
	int32_t v;
	int i;

	for (i = 1; i < n; i++) {
		if (mel[i] > peak)
			peak = mel[i];
	}

	/* Attack: instantly keep the hop peak at or below +1.0 */
	if (agc->gain > MFCC_AGC8_CLIP_MAX_Q23 - peak)
		agc->gain = MFCC_AGC8_CLIP_MAX_Q23 - peak;
	if (agc->gain < agc->floor)
		agc->gain = agc->floor;

	gain = agc->gain;
	for (i = 0; i < n; i++) {
		v = mel[i] + gain;
		if (v > MFCC_AGC8_CLIP_MAX_Q23)
			v = MFCC_AGC8_CLIP_MAX_Q23;
		else if (v < MFCC_AGC8_CLIP_MIN_Q23)
			v = MFCC_AGC8_CLIP_MIN_Q23;

		v = Q_SHIFT_RND(v, 23, 7);
		out[i] = (int8_t)(v > INT8_MAX ? INT8_MAX : v);
	}

	/* Release towards target, with a slower step during speech */
	if (agc->gain < agc->target) {
		agc->gain += speech ? agc->release_speech : agc->release;
		if (agc->gain > agc->target)
			agc->gain = agc->target;
	}
}
