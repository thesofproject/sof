// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/format.h>
#include <sof/math/auditory.h>
#include <sof/math/icomplex32.h>
#include <sof/math/log.h>
#include <sof/math/numbers.h>
#include <sof/math/sqrt.h>
#include <stdint.h>

static inline uint32_t mel_sqrt32(uint32_t num)
{
	if (num == 0)
		return 0;

	/* sofm_sqrt_int32 treats input as Q2.30, returning sqrt(n)*2^15.
	 * Scale down by 2^15 with rounding to obtain integer sqrt(num).
	 */
	return (uint32_t)((sofm_sqrt_int32((int32_t)num) + (1 << 14)) >> 15);
}

void psy_apply_mel_filterbank_with_linear_32(struct psy_mel_filterbank *fb, struct icomplex32 *fft_out,
					    int32_t *power_spectra, int32_t *mel_log,
					    uint32_t *mel_linear, int bitshift)
{
	int64_t pmax;
	int64_t p;
	int32_t log_arg;
	int32_t log;
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
		p = (int64_t)fft_out[i].real * fft_out[i].real +
			(int64_t)fft_out[i].imag * fft_out[i].imag;
		pmax = MAX(pmax, p);
	}

	/* Product Q2.62, convert to 2.30 */
	pmax = sat_int32(pmax >> 32);
	lshift = norm_int32(pmax);
	for (i = 0; i < fb->half_fft_bins; i++) {
		p = (int64_t)fft_out[i].real * fft_out[i].real +
			(int64_t)fft_out[i].imag * fft_out[i].imag;
		power_spectra[i] = Q_SHIFT_RND(p << lshift, 62, 30);
	}

	for (i = 0; i < fb->mel_bins; i++) {
		/* Integrate power spectrum with Mel filter bank triangle weights */
		p = 0;
		next_idx = fb->data[base_idx];
		start_bin = fb->data[base_idx + 1];
		num_bins = fb->data[base_idx + 2];
		coef_idx = base_idx + 3;
		base_idx = next_idx; /* For next round */

		/* Accumulate power as Q3.45 (Q2.30 x Q1.15). Note that filter bank need
		 * to be later scaled with fb->scale.
		 */
		for (j = 0; j < num_bins; j++)
			p += (int64_t)power_spectra[start_bin + j] * fb->data[coef_idx + j];

		/* Convert Mel band energy from Q19.45 to Q7.25 that has sufficient headroom
		 * for worst-case all ones FFT output. Log2() function input is unsigned Q32.0,
		 * output is signed Q16.16. The Q7.25 scale log2(2^25) need to be subtracted
		 * from log output.
		 */
		log_arg = sat_int32(Q_SHIFT_RND(p, 45, 25));
		log_arg = MAX(log_arg, AUDITORY_EPS_Q31);

		if (mel_linear) {
			/* Compensate dynamic lshift and FFT bitshift so mel_linear reflects
			 * true acoustic magnitude calibrated to Google microfrontend range.
			 */
			uint32_t s = mel_sqrt32((uint32_t)log_arg);
			/* Total power shift applied was: lshift + 2 * bitshift */
			int neg_shift = -((int32_t)lshift + 2 * bitshift);
			int int_shift = neg_shift >> 1;
			int frac_shift = neg_shift & 1;

			uint64_t s_comp = s;
			if (frac_shift)
				s_comp = (s_comp * 46341U) >> 15; /* 46341 / 32768 ~= sqrt(2) */

			if (int_shift > 0)
				s_comp <<= int_shift;
			else if (int_shift < 0)
				s_comp >>= -int_shift;

			/* Scale to Google microfrontend range: 25826 / 32768 ~= 0.788 */
			s_comp = (s_comp * 25826U) >> 15;
			if (s_comp > 65535U)
				s_comp = 65535U;

			mel_linear[i] = (uint32_t)s_comp;
		}

		if (mel_log) {
			log = base2_logarithm((uint32_t)log_arg);
			log -= AUDITORY_LOG2_2P25_Q16;

			/* Compensate Mel triangles scale */
			log += fb->scale_log2;

			/* Subtract the applied lshift for power spectra
			 * log2(x * 2^(-n)) = log2(x) - n. Note that the bitshift need to be subtracted
			 * as doubled because it was applied in linear domain, from log(x * 2^(-2 * n))
			 */
			log -= ((int32_t)lshift + 2 * bitshift) << 16;

			/* Scale for desired log, output as Q9.23 */
			log = Q_MULTSR_32X32((int64_t)log, fb->log_mult, 16, 29, 23);
			mel_log[i] = log; /* Q9.23 */
		}
	}
}

void psy_apply_mel_filterbank_32(struct psy_mel_filterbank *fb, struct icomplex32 *fft_out,
				 int32_t *power_spectra, int32_t *mel_log, int bitshift)
{
	psy_apply_mel_filterbank_with_linear_32(fb, fft_out, power_spectra, mel_log, NULL, bitshift);
}
