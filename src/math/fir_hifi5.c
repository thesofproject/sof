// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017-2025 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_MIN_HIFI(5, FILTER)

#include <sof/audio/buffer.h>
#include <sof/math/fir_hifi3.h>
#include <user/fir.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi5.h>
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

	/* The optimization requires the tap count to be multiple of four */
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
	fir->coef = (ae_f16x4 *)&config->coef[0];
	return 0;
}
EXPORT_SYMBOL(fir_init_coef);

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data)
{
	fir->delay = (ae_int32 *)*data;
	fir->delay_end = fir->delay + fir->length;
	fir->rwp = (ae_int32 *)(fir->delay + fir->length - 1);
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

void fir_32x16(struct fir_state_32x16 *fir, ae_int32 x, ae_int32 *y, int shift)
{
	/* This function uses
	 * 1x 56 bit registers Q,
	 * 4x 48 bit registers P
	 * 3x integers
	 * 2x address pointers,
	 */
	ae_f64 a;
	ae_valign u;
	ae_f32x2 data2;
	ae_f16x4 coefs;
	ae_f32x2 d0;
	ae_f32x2 d1;
	int i;
	ae_int32 *dp = fir->rwp;
	ae_int16x4 *coefp = (ae_int16x4 *)fir->coef;
	const int taps_div_4 = fir->taps >> 2;
	const int inc = sizeof(int32_t);

	/* Bypass samples if taps count is zero. */
	if (!taps_div_4) {
		*y = x;
		return;
	}

	/* Write sample to delay */
	AE_S32_L_XC(x, fir->rwp, -sizeof(int32_t));

	/* Prime the coefficients stream */
	u = AE_LA64_PP(coefp);

	/* Note: If the next function is converted to handle two samples
	 * per call the data load can be done with single instruction
	 * AE_LP24X2F_C(data2, dp, sizeof(ae_p24x2f));
	 */
	a = AE_ZEROQ56();
	for (i = 0; i < taps_div_4; i++) {
		/* Load four coefficients. Coef_3 contains tap h[n],
		 * coef_2 contains h[n+1], coef_1 contains h[n+2], and
		 * coef_0 contains h[n+3];
		 */
		AE_LA16X4_IP(coefs, u, coefp);

		/* Load two data samples and pack to d0 to data2_h and
		 * d1 to data2_l.
		 */
		AE_L32_XC(d0, dp, inc);
		AE_L32_XC(d1, dp, inc);
		data2 = AE_SEL32_LL(d0, d1);

		/* Accumulate
		 * a += data2_h * coefs_3 + data2_l * coefs_2. The Q1.31
		 * data and Q1.15 coefficients are used as 24 bits as
		 * Q1.23 values.
		 */
		AE_MULAAFD32X16_H3_L2(a, data2, coefs);

		/* Repeat the same for next two taps and increase coefp.
		 * a += data2_h * coefs_1 + data2_l * coefs_0.
		 */
		AE_L32_XC(d0, dp, inc);
		AE_L32_XC(d1, dp, inc);
		data2 = AE_SEL32_LL(d0, d1);
		AE_MULAAFD32X16_H1_L0(a, data2, coefs);
	}

	/* Do scaling shifts and store sample. */
	a = AE_SLAA64S(a, shift);
	AE_S32_L_I(AE_ROUND32F48SSYM(a), (ae_int32 *)y, 0);
}
EXPORT_SYMBOL(fir_32x16);

/* HiFi EP has the follow number of reqisters that should not be exceeded
 * 4x 56 bit registers in register file Q
 * 8x 48 bit registers in register file P
 */

void fir_32x16_2x(struct fir_state_32x16 *fir, ae_int32 x0, ae_int32 x1,
		  ae_int32 *y0, ae_int32 *y1, int shift)
{
	/* This function uses
	 * 2x 56 bit registers Q,
	 * 4x 48 bit registers P
	 * 3x integers
	 * 2x address pointers,
	 */
	ae_f64 a;
	ae_f64 b;
	ae_valign u;
	ae_f32x2 d0;
	ae_f32x2 d1;
	ae_f16x4 coefs;
	int i;
	ae_f32x2 *dp;
	ae_f16x4 *coefp = fir->coef;
	const int taps_div_4 = fir->taps >> 2;
	const int inc = 2 * sizeof(int32_t);

	/* Bypass samples if taps count is zero. */
	if (!taps_div_4) {
		*y0 = x0;
		*y1 = x1;
		return;
	}

	/* Write samples to delay */
	AE_S32_L_XC(x0, fir->rwp, -sizeof(int32_t));
	dp = (ae_f32x2 *)fir->rwp;
	AE_S32_L_XC(x1, fir->rwp, -sizeof(int32_t));

	/* Note: If the next function is converted to handle two samples
	 * per call the data load can be done with single instruction
	 * AE_LP24X2F_C(data2, dp, sizeof(ae_p24x2f));
	 */
	a = AE_ZERO64();
	b = AE_ZERO64();

	/* Prime the coefficients stream */
	u = AE_LA64_PP(coefp);

	/* Load two data samples and pack to d0 to data2_h and
	 * d1 to data2_l.
	 */
	AE_L32X2_XC(d0, dp, inc);
	for (i = 0; i < taps_div_4; i++) {
		/* Load four coefficients. Coef_3 contains tap h[n],
		 * coef_2 contains h[n+1], coef_1 contains h[n+2], and
		 * coef_0 contains h[n+3];
		 */
		AE_LA16X4_IP(coefs, u, coefp);

		/* Load two data samples. Upper part d1_h is x[n+1] and
		 * lower part d1_l is x[n].
		 */
		AE_L32X2_XC(d1, dp, inc);

		/* Quad MAC (HH)
		 * b += d0_h * coefs_3 + d0_l * coefs_2
		 * a += d0_l * coefs_3 + d1_h * coefs_2
		 */
		AE_MULAFD32X16X2_FIR_HH(b, a, d0, d1, coefs);
		d0 = d1;

		/* Repeat the same for next two taps and increase coefp. */
		AE_L32X2_XC(d1, dp, inc);

		/* Quad MAC (HL)
		 * b += d0_h * coefs_1 + d0_l * coefs_0
		 * a += d0_l * coefs_1 + d1_h * coefs_0
		 */
		AE_MULAFD32X16X2_FIR_HL(b, a, d0, d1, coefs);
		d0 = d1;
	}

	/* Do scaling shifts and store sample. */
	b = AE_SLAA64S(b, shift);
	a = AE_SLAA64S(a, shift);
	AE_S32_L_I(AE_ROUND32F48SSYM(b), (ae_int32 *)y1, 0);
	AE_S32_L_I(AE_ROUND32F48SSYM(a), (ae_int32 *)y0, 0);
}
EXPORT_SYMBOL(fir_32x16_2x);

#endif
