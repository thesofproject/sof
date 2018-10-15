/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef FIR_HIFI2EP_H
#define FIR_HIFI2EP_H

#include "fir_config.h"

#if FIR_HIFIEP

#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi2.h>
#include <sof/audio/format.h>

struct fir_state_32x16 {
	ae_p24x2f *rwp; /* Circular read and write pointer */
	ae_p24f *delay; /* Pointer to FIR delay line */
	ae_p24f *delay_end; /* Pointer to FIR delay line end */
	ae_p16x2s *coef; /* Pointer to FIR coefficients */
	int taps; /* Number of FIR taps */
	int length; /* Number of FIR taps plus input length (even) */
	int in_shift; /* Amount of right shifts at input */
	int out_shift; /* Amount of right shifts at output */
};

void fir_reset(struct fir_state_32x16 *fir);

size_t fir_init_coef(struct fir_state_32x16 *fir,
		     struct sof_eq_fir_coef_data *config);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data);

void eq_fir_s16_hifiep(struct fir_state_32x16 fir[], struct comp_buffer *source,
		       struct comp_buffer *sink, int frames, int nch);

void eq_fir_s24_hifiep(struct fir_state_32x16 fir[], struct comp_buffer *source,
		       struct comp_buffer *sink, int frames, int nch);

void eq_fir_s32_hifiep(struct fir_state_32x16 fir[], struct comp_buffer *source,
		       struct comp_buffer *sink, int frames, int nch);

void eq_fir_2x_s16_hifiep(struct fir_state_32x16 fir[],
			  struct comp_buffer *source,
			  struct comp_buffer *sink,
			  int frames, int nch);

void eq_fir_2x_s24_hifiep(struct fir_state_32x16 fir[],
			  struct comp_buffer *source,
			  struct comp_buffer *sink,
			  int frames, int nch);

void eq_fir_2x_s32_hifiep(struct fir_state_32x16 fir[],
			  struct comp_buffer *source,
			  struct comp_buffer *sink,
			  int frames, int nch);

/* Setup circular buffer for FIR input data delay */
static inline void fir_hifiep_setup_circular(struct fir_state_32x16 *fir)
{
	AE_SETCBEGIN0(fir->delay);
	AE_SETCEND0(fir->delay_end);
}

void fir_get_lrshifts(struct fir_state_32x16 *fir, int *lshift,
		      int *rshift);

/* The next functions are inlined to optmize execution speed */

/* HiFi EP has the follow number of reqisters that should not be exceeded
 * 4x 56 bit registers in register file Q
 * 8x 48 bit registers in register file P
 */

static inline void fir_32x16_hifiep(struct fir_state_32x16 *fir, int32_t x,
				    int32_t *y, int lshift, int rshift)
{
	/* This function uses
	 * 1x 56 bit registers Q,
	 * 4x 48 bit registers P
	 * 3x integers
	 * 2x address pointers,
	 */
	ae_q56s a;
	ae_p24x2f data2;
	ae_p24x2f coef2;
	ae_p24x2f d0;
	ae_p24x2f d1;
	int i;
	ae_p24x2f *dp = fir->rwp;
	ae_p16x2s *coefp = fir->coef;
	const int taps_div_4 = fir->taps >> 2;
	const int inc = sizeof(int32_t);

	/* Bypass samples if taps count is zero. */
	if (!taps_div_4) {
		*y = x;
		return;
	}

	/* Write sample to delay */
	a = AE_CVTQ48A32S(x);
	AE_SQ32F_C(a, (ae_q32s *)fir->rwp, -sizeof(int32_t));

	/* Note: If the next function is converted to handle two samples
	 * per call the data load can be done with single instruction
	 * AE_LP24X2F_C(data2, dp, sizeof(ae_p24x2f));
	 */
	a = AE_ZEROQ56();
	for (i = 0; i < taps_div_4; i++) {
		/* Load two coefficients. Coef2_h contains tap coefp[n]
		 * and coef2_l contains coef[n+1].
		 */
		coef2 = AE_LP16X2F_I(coefp, 0);

		/* Load two data samples and pack to d0 to data2_h and
		 * d1 to data2_l.
		 */
		AE_LP24F_C(d0, dp, inc);
		AE_LP24F_C(d1, dp, inc);
		data2 = AE_SELP24_LL(d0, d1);

		/* Accumulate
		 * data2_h * coef2_h + data2_l * coef2_l. The Q1.31
		 * data and Q1.15 coefficients are used as 24 bits as
		 * Q1.23 values.
		 */
		AE_MULAAFP24S_HH_LL(a, data2, coef2);

		/* Repeat the same for next two taps and increase coefp. */
		coef2 = AE_LP16X2F_I(coefp, sizeof(ae_p16x2s));
		AE_LP24F_C(d0, dp, inc);
		AE_LP24F_C(d1, dp, inc);
		data2 = AE_SELP24_LL(d0, d1);
		AE_MULAAFP24S_HH_LL(a, data2, coef2);
		coefp += 2;
	}

	/* Do scaling shifts and store sample. */
	a = AE_SRAAQ56(AE_SLLASQ56S(a, lshift), rshift);
	AE_SQ32F_I(AE_ROUNDSQ32SYM(a), (ae_q32s *)y, 0);
}

/* HiFi EP has the follow number of reqisters that should not be exceeded
 * 4x 56 bit registers in register file Q
 * 8x 48 bit registers in register file P
 */

static inline void fir_32x16_2x_hifiep(struct fir_state_32x16 *fir, int32_t x0,
				       int32_t x1, int32_t *y0, int32_t *y1,
				       int lshift, int rshift)
{
	/* This function uses
	 * 2x 56 bit registers Q,
	 * 4x 48 bit registers P
	 * 3x integers
	 * 2x address pointers,
	 */
	ae_q56s a;
	ae_q56s b;
	ae_p24x2f d0;
	ae_p24x2f d1;
	ae_p24x2f d3;
	ae_p24x2f coefs;
	int i;
	ae_p24x2f *dp;
	ae_p16x2s *coefp = fir->coef;
	const int taps_div_4 = fir->taps >> 2;
	const int inc = 2 * sizeof(int32_t);

	/* Bypass samples if taps count is zero. */
	if (!taps_div_4) {
		*y0 = x0;
		*y1 = x1;
		return;
	}

	/* Write samples to delay */
	a = AE_CVTQ48A32S(x0);
	AE_SQ32F_C(a, (ae_q32s *)fir->rwp, -sizeof(int32_t));
	a = AE_CVTQ48A32S(x1);
	dp = fir->rwp;
	AE_SQ32F_C(a, (ae_q32s *)fir->rwp, -sizeof(int32_t));

	/* Note: If the next function is converted to handle two samples
	 * per call the data load can be done with single instruction
	 * AE_LP24X2F_C(data2, dp, sizeof(ae_p24x2f));
	 */
	a = AE_ZEROQ56();
	b = AE_ZEROQ56();
	/* Load two data samples and pack to d0 to data2_h and
	 * d1 to data2_l.
	 */
	AE_LP24X2F_C(d0, dp, inc);
	for (i = 0; i < taps_div_4; i++) {
		/* Load two coefficients. Coef2_h contains tap coefp[n]
		 * and coef2_l contains coef[n+1].
		 */
		coefs = AE_LP16X2F_I(coefp, 0);

		/* Load two data samples. Upper part d1_h is x[n+1] and
		 * lower part d1_l is x[n].
		 */
		AE_LP24X2F_C(d1, dp, inc);

		/* Accumulate
		 * b += d0_h * coefs_h + d0_l * coefs_l. The Q1.31 data
		 * and Q1.15 coefficients are converted to 24 bits as
		 * Q1.23 values.
		 */
		AE_MULAAFP24S_HH_LL(b, d0, coefs);

		/* Pack d0_l and d1_h to d3. Then accumulate
		 * a += d3_h * coefs_h + d3_l * coefs_l. Pass d1 to d1 for
		 * next unrolled iteration.
		 */
		d3 = AE_SELP24_LH(d0, d1);
		AE_MULAAFP24S_HH_LL(a, d3, coefs);
		d0 = d1;

		/* Repeat the same for next two taps and increase coefp. */
		coefs = AE_LP16X2F_I(coefp, sizeof(ae_p16x2s));
		AE_LP24X2F_C(d1, dp, inc);
		AE_MULAAFP24S_HH_LL(b, d0, coefs);
		d3 = AE_SELP24_LH(d0, d1);
		AE_MULAAFP24S_HH_LL(a, d3, coefs);
		d0 = d1;
		coefp += 2;
	}

	/* Do scaling shifts and store sample. */
	b = AE_SRAAQ56(AE_SLLASQ56S(b, lshift), rshift);
	a = AE_SRAAQ56(AE_SLLASQ56S(a, lshift), rshift);
	AE_SQ32F_I(AE_ROUNDSQ32SYM(b), (ae_q32s *)y1, 0);
	AE_SQ32F_I(AE_ROUNDSQ32SYM(a), (ae_q32s *)y0, 0);
}

#endif
#endif
