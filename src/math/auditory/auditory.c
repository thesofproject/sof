// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <rtos/alloc.h>
#include <sof/math/auditory.h>
#include <sof/math/decibels.h>
#include <sof/math/fft.h>
#include <sof/math/log.h>
#include <sof/math/numbers.h>
#include <ipc/topology.h>
#include <errno.h>
#include <stdint.h>

#define ONE_Q16 Q_CONVERT_FLOAT(1, 16)
#define ONE_Q20 Q_CONVERT_FLOAT(1, 20)
#define ONE_Q30 Q_CONVERT_FLOAT(1, 30)
#define TWO_Q29 Q_CONVERT_FLOAT(2, 29)

#define LOG_SCALE_HZMEL_Q26 Q_CONVERT_FLOAT(18.02182669, 26)
#define LOG_MULT_HZMEL_Q20 Q_CONVERT_FLOAT(1126.9941805389, 20)
#define ONE_OVER_HZDIV_Q31 Q_CONVERT_FLOAT(1.0 / 700, 31)
#define ONE_OVER_MELDIV_Q31 Q_CONVERT_FLOAT(1.0 / 1126.9941805389, 31)

#define MEL_MAX_Q2 Q_CONVERT_FLOAT(4358.351140, 2) /* 32767 Hz */

#define LOG2_2P16 Q_CONVERT_FLOAT(16.0, 16) /* log2(2^16) as Q16.16 */

/* For scaling log2 power to log, log10, or dB*/
#define ONE_OVER_LOG2E_Q29	Q_CONVERT_FLOAT(0.6931471806, 29) /* 1/log2(exp(1))) */
#define ONE_OVER_LOG2TEN_Q29	Q_CONVERT_FLOAT(0.3010299957, 29) /* 1/log2(10) */
#define TEN_OVER_LOG2TEN_Q29	Q_CONVERT_FLOAT(3.0102999566, 29) /* 10/log2(10) */

/**
 * Convert Hz to Mel
 * Wikipedia https://en.wikipedia.org/wiki/Mel_scale
 * mel = 1126.9941805389 * log(1 + hz / 700)
 *
 * @param hz Frequency as Q16.0 Hz, max 32767 Hz
 * @return Value as Q14.2 Mel, max 4358.4 Mel
 */
int16_t psy_hz_to_mel(int16_t hz)
{
	uint32_t log_arg;
	int32_t log;
	int16_t mel;

	if (hz < 0)
		return 0;

	/* Log function input is unsigned UQ32.0, output is UQ5.27 */
	log_arg = (1 << 26) + Q_MULTSR_32X32((int64_t)hz, ONE_OVER_HZDIV_Q31, 0, 31, 26);

	log = (int32_t)(ln_int32(log_arg) >> 1) - LOG_SCALE_HZMEL_Q26;
	mel = Q_MULTSR_32X32((int64_t)log, LOG_MULT_HZMEL_Q20, 26, 20, 2);
	return mel;
}

/**
 * Convert Mel to Hz
 * Wikipedia https://en.wikipedia.org/wiki/Mel_scale
 * hz = 700 * (exp(mel / 1126.9941805389) - 1)
 *
 * @param mel Value as Q14.2 Mel, max 4358.4 Mel
 * @return hz Frequency as Q16.0 Hz, max 32767 Hz
 */
int16_t psy_mel_to_hz(int16_t mel)
{
	int32_t exp_arg;
	int32_t exp;
	int16_t hz;

	if (mel > MEL_MAX_Q2)
		return INT16_MAX;

	if (mel < 0)
		return 0;

	exp_arg = Q_MULTSR_32X32((int64_t)mel, ONE_OVER_MELDIV_Q31, 2, 31, 27);
	exp = exp_fixed(exp_arg)  - ONE_Q20;
	hz = Q_MULTSR_32X32((int64_t)exp, 700, 20, 0, 0);
	return hz;
}

int psy_get_mel_filterbank(struct psy_mel_filterbank *fb)
{
	int32_t up_slope;
	int32_t down_slope;
	int32_t slope;
	int32_t scale;
	int32_t scale_inv;
	int16_t *mel;
	int16_t mel_start;
	int16_t mel_end;
	int16_t mel_step;
	int16_t left_mel;
	int16_t center_mel;
	int16_t right_mel;
	int16_t delta_cl;
	int16_t delta_rc;
	int16_t left_hz;
	int16_t right_hz;
	int16_t f;
	int segment;
	int i, j, idx;
	int base_idx = 0;
	int start_bin = 0;
	int end_bin = 0;

	if (!fb)
		return -ENOMEM;

	if (!fb->scratch_data1 || !fb->scratch_data2)
		return -ENOMEM;

	/* Log power can be log, or log10 or dB, get multiply coef to convert
	 * log to desired format.
	 */
	switch (fb->mel_log_scale) {
	case MEL_LOG:
		fb->log_mult = ONE_OVER_LOG2E_Q29;
		break;
	case MEL_LOG10:
		fb->log_mult = ONE_OVER_LOG2TEN_Q29;
		break;
	case MEL_DB:
		fb->log_mult = TEN_OVER_LOG2TEN_Q29;
		break;
	default:
		return -EINVAL;
	}

	/* Use scratch area to hold vector of Mel values for each FFT frequency */
	if (fb->scratch_length1 < fb->half_fft_bins)
		return -ENOMEM;

	mel = fb->scratch_data1;
	for (i = 0; i < fb->half_fft_bins; i++) {
		f = fb->samplerate * i / fb->fft_bins;
		mel[i] = psy_hz_to_mel(f);
	}

	mel_start = psy_hz_to_mel(fb->start_freq);
	mel_end = psy_hz_to_mel(fb->end_freq);
	mel_step = (mel_end - mel_start) / (fb->mel_bins + 1);
	for (i = 0; i < fb->mel_bins; i++) {
		left_mel = mel_start + i * mel_step;
		center_mel = mel_start + (i + 1) * mel_step;
		right_mel = mel_start + (i + 2) * mel_step;
		delta_cl = center_mel - left_mel;
		delta_rc = right_mel - center_mel;
		segment = 0;
		idx = base_idx + 3; /* start of filter weight values */
		if (fb->slaney_normalize) {
			left_hz = psy_mel_to_hz(left_mel);
			right_hz = psy_mel_to_hz(right_mel);
			scale = Q_SHIFT_RND(TWO_Q29 / (right_hz - left_hz), 29, 16); /* Q16.16*/
			if (i == 0) {
				scale_inv = Q_SHIFT_LEFT(ONE_Q30 / scale, 14, 16);
				fb->scale_log2 = base2_logarithm((uint32_t)scale) - LOG2_2P16;
			}

			scale = Q_MULTSR_32X32((int64_t)scale, scale_inv, 16, 16, 16);
		} else {
			scale = ONE_Q16;
			scale_inv = ONE_Q16;
			fb->scale_log2 = 0;
		}
		for (j = 0; j < fb->half_fft_bins; j++) {
			up_slope = (((int32_t)mel[j] - left_mel) << 15) / delta_cl; /* Q17.15 */
			down_slope = (((int32_t)right_mel - mel[j]) << 15) / delta_rc; /* Q17.15 */
			slope = MIN(up_slope, down_slope);
			slope = Q_MULTSR_32X32((int64_t)slope, scale, 15, 16, 15);
			if (segment == 1 && slope <= 0) {
				end_bin = j - 1;
				break;
			}

			if (segment == 0 && slope > 0) {
				start_bin = j;
				segment = 1;
			}

			if (segment == 1) {
				if (idx >= fb->scratch_length2)
					return -EINVAL;

				fb->scratch_data2[idx++] = sat_int16(slope);
			}
		}

		if (idx + 2 >= fb->scratch_length2)
			return -EINVAL;

		fb->scratch_data2[base_idx] = idx; /* index to next */
		fb->scratch_data2[base_idx + 1] = start_bin;
		fb->scratch_data2[base_idx + 2] = end_bin - start_bin + 1; /* length */
		base_idx = idx;
	}

	fb->data_length = &fb->scratch_data2[base_idx] - &fb->scratch_data2[0];
	fb->data = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			   sizeof(int16_t) * fb->data_length);
	if (!fb->data)
		return -ENOMEM;

	/* Copy the exact triangles data size to allocated buffer */
	memcpy_s(fb->data, sizeof(int16_t) * fb->data_length,
		 fb->scratch_data2, sizeof(int16_t) * fb->data_length);
	return 0;
}
