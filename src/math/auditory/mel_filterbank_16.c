// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/auditory.h>
#include <sof/math/fft.h>
#include <sof/math/log.h>
#include <sof/math/numbers.h>
#include <stdint.h>

void psy_apply_mel_filterbank_16(struct psy_mel_filterbank *fb, struct icomplex16 *fft_out,
				 int32_t *power_spectra, int16_t *mel_log, int bitshift)
{
	int64_t pp;
	int32_t pmax;
	int32_t log_arg;
	int32_t log;
	int32_t p;
	int next_idx;
	int start_bin;
	int num_bins;
	int coef_idx;
	int i, j;
	int base_idx = 0;
	int lshift;

	/* A FFT out bin is used several times in Mel bands conversion, so first
	 * convert FFT to real power spectra, p = (a + bi)(a - bi) = a^2 + b^2
	 */
	pmax = 0;
	for (i = 0; i < fb->half_fft_bins; i++) {
		p = (int32_t)fft_out[i].real * fft_out[i].real +
			(int32_t)fft_out[i].imag * fft_out[i].imag;
		pmax = MAX(pmax, p);
	}

	/* Power spectra is Q2.30 */
	lshift = norm_int32(pmax);
	for (i = 0; i < fb->half_fft_bins; i++) {
		p = (int32_t)fft_out[i].real * fft_out[i].real +
			(int32_t)fft_out[i].imag * fft_out[i].imag;
		power_spectra[i] = p << lshift;
	}

	for (i = 0; i < fb->mel_bins; i++) {
		/* Integrate power spectrum with Mel filter bank triangle weights */
		pp = 0;
		next_idx = fb->data[base_idx];
		start_bin = fb->data[base_idx + 1];
		num_bins = fb->data[base_idx + 2];
		coef_idx = base_idx + 3;
		base_idx = next_idx; /* For next round */

		/* Accumulate power as Q3.45 (Q2.30 x Q1.15). Note that filter bank need
		 * to be later scaled with fb->scale.
		 */
		for (j = 0; j < num_bins; j++)
			pp += (int64_t)power_spectra[start_bin + j] * fb->data[coef_idx + j];

		/* Convert Mel band energy from Q19.45 to Q7.25 that has sufficient headroom
		 * for worst-case all ones FFT output. Log2() function input is unsigned Q32.0,
		 * output is signed Q16.16. The Q7.25 scale log2(2^25) need to be subtracted
		 * from log output.
		 */
		log_arg = sat_int32(Q_SHIFT_RND(pp, 45, 25));
		log_arg = MAX(log_arg, AUDITORY_EPS_Q31);
		log = base2_logarithm((uint32_t)log_arg);
		log -= AUDITORY_LOG2_2P25_Q16;

		/* Compensate Mel triangles scale */
		log += fb->scale_log2;

		/* Subtract the applied lshift for power spectra
		 * log2(x * 2^(-n)) = log2(x) - n. Note that the bitshift need to be subtracted
		 * as doubled because it was applied in linear domain, from log(x * 2^(-2 * n))
		 */
		log -= ((int32_t)lshift + 2 * bitshift) << 16;

		/* Scale for desired log  */
		log = Q_MULTSR_32X32((int64_t)log, fb->log_mult, 16, 29, 7);
		mel_log[i] = sat_int16(log); /* Q8.7 */
	}
}
