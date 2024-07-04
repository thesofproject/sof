// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_HIFI(2, FILTER)

#include <sof/audio/format.h>
#include <sof/math/fir_hifi2ep.h>
#include <user/fir.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi2.h>
#include <rtos/symbol.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/*
 * EQ FIR algorithm code
 */

void fir_reset(struct fir_state_32x16 *fir)
{
	fir->taps = 0;
	fir->length = 0;
	fir->out_shift = 0;
	fir->coef = NULL;
	/* There may need to know the beginning of dynamic allocation after
	 * reset so omitting setting also fir->delay to NULL.
	 */
}
EXPORT_SYMBOL(fir_reset);

int fir_delay_size(struct sof_fir_coef_data *config)
{
	/* Check FIR tap count for implementation specific constraints */
	if (config->length > SOF_FIR_MAX_LENGTH || config->length < 4)
		return -EINVAL;

	if (config->length & 0x3)
		return -EINVAL;

	/* The dual sample version needs one more delay entry. To preserve
	 * align for 64 bits need to add two.
	 */
	return (config->length + 2) * sizeof(int32_t);
}
EXPORT_SYMBOL(fir_delay_size);

int fir_init_coef(struct fir_state_32x16 *fir,
		  struct sof_fir_coef_data *config)
{
	/* The length is taps plus two since the filter computes two
	 * samples per call. Length plus one would be minimum but the add
	 * must be even. The even length is needed for 64 bit loads from delay
	 * lines with 32 bit samples.
	 */
	fir->taps = (int)config->length;
	fir->length = fir->taps + 2;
	fir->out_shift = (int)config->out_shift;
	fir->coef = (ae_p16x2s *)&config->coef[0];
	return 0;
}
EXPORT_SYMBOL(fir_init_coef);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data)
{
	fir->delay = (ae_p24f *)*data;
	fir->delay_end = fir->delay + fir->length;
	fir->rwp = (ae_p24x2f *)(fir->delay + fir->length - 1);
	*data += fir->length; /* Point to next delay line start */
}
EXPORT_SYMBOL(fir_init_delay);

void fir_get_lrshifts(struct fir_state_32x16 *fir, int *lshift,
		      int *rshift)
{
	*lshift = (fir->out_shift < 0) ? -fir->out_shift : 0;
	*rshift = (fir->out_shift > 0) ? fir->out_shift : 0;
}
EXPORT_SYMBOL(fir_get_lrshifts);

/* HiFi EP has the follow number of reqisters that should not be exceeded
 * 4x 56 bit registers in register file Q
 * 8x 48 bit registers in register file P
 */

void fir_32x16_hifiep(struct fir_state_32x16 *fir, int32_t x, int32_t *y, int lshift, int rshift)
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
EXPORT_SYMBOL(fir_32x16_hifiep);

/* HiFi EP has the follow number of reqisters that should not be exceeded
 * 4x 56 bit registers in register file Q
 * 8x 48 bit registers in register file P
 */

void fir_32x16_2x_hifiep(struct fir_state_32x16 *fir, int32_t x0, int32_t x1,
			 int32_t *y0, int32_t *y1, int lshift, int rshift)
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
EXPORT_SYMBOL(fir_32x16_2x_hifiep);

#endif
