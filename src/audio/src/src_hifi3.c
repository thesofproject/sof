// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/* HiFi3 optimized code parts for SRC */

#include "src_config.h"

#if SRC_HIFI3

#include "src.h"

#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi3.h>
#include <stddef.h>
#include <stdint.h>

/* HiFi3 has
 * 16x 64 bit registers in register file AE_DR
 */

#if SRC_SHORT /* 16 bit coefficients version */

static inline void fir_filter(ae_f32 *rp, const void *cp, ae_f32 *wp0,
			      const int taps_div_4, const int shift,
			      const int nch)
{
	/* This function uses
	 * 6x 64 bit registers
	 * 3x integers
	 * 5x address pointers,
	 */
	ae_f64 a0;
	ae_f64 a1;
	ae_valign u;
	ae_f16x4 coef4;
	ae_f32x2 d0;
	ae_f32x2 d1;
	ae_f32x2 data2;
	ae_f16x4 *coefp;
	ae_f32x2 *dp;
	ae_f32 *dp0;
	ae_f32 *dp1;
	int i;
	int j;
	ae_f32 *wp = wp0;
	const int inc = nch * sizeof(int32_t);

	if (nch == 2) {
		/* Move data pointer back by one sample to start from right
		 * channel sample. Discard read value p0.
		 */
		dp = (ae_f32x2 *)rp;
		AE_L32_XC(d0, (ae_f32 *)dp, -sizeof(ae_f32));

		/* Reset coefficient pointer and clear accumulator */
		coefp = (ae_f16x4 *)cp;
		a0 = AE_ZERO64();
		a1 = AE_ZERO64();

		/* Compute FIR filter for current channel with four
		 * taps per every loop iteration.  Four coefficients
		 * are loaded simultaneously. Data is read
		 * from interleaved buffer with stride of channels
		 * count.
		 */
		for (i = 0; i < taps_div_4; i++) {
			/* Load four coefficients */
			AE_LA16X4_IP(coef4, u, coefp);

			/* Load two data samples from two channels */
			AE_L32X2_XC(d0, dp, inc); /* r0, l0 */
			AE_L32X2_XC(d1, dp, inc); /* r1, l1 */

			/* Select to data2 sequential samples from a channel
			 * and then accumulate to a0 and a1
			 * data2_h * coef4_3 + data2_l * coef4_2.
			 * The data is 32 bits Q1.31 and coefficient 16 bits
			 * Q1.15. The accumulators are Q17.47.
			 */
			data2 = AE_SEL32_LL(d0, d1); /* l0, l1 */
			AE_MULAAFD32X16_H3_L2(a0, data2, coef4);
			data2 = AE_SEL32_HH(d0, d1); /* r0, r1 */
			AE_MULAAFD32X16_H3_L2(a1, data2, coef4);

			/* Load two data samples from two channels */
			AE_L32X2_XC(d0, dp, inc); /* r2, l2 */
			AE_L32X2_XC(d1, dp, inc); /* r3, l3 */

			/* Accumulate
			 * data2_h * coef4_1 + data2_l * coef4_0.
			 */
			data2 = AE_SEL32_LL(d0, d1); /* l2, l3 */
			AE_MULAAFD32X16_H1_L0(a0, data2, coef4);
			data2 = AE_SEL32_HH(d0, d1); /* r2, r3 */
			AE_MULAAFD32X16_H1_L0(a1, data2, coef4);
		}

		/* Scale FIR output with right shifts, round/saturate
		 * to Q1.31, and store 32 bit output.
		 */
		AE_S32_L_XP(AE_ROUND32F48SSYM(AE_SRAA64(a0, shift)), wp,
			    sizeof(int32_t));
		AE_S32_L_XP(AE_ROUND32F48SSYM(AE_SRAA64(a1, shift)), wp,
			    sizeof(int32_t));

		return;
	}

	dp1 = (ae_f32 *)rp;
	for (j = 0; j < nch; j++) {
		/* Copy pointer and advance to next ch with dummy load */
		dp0 = dp1;
		AE_L32_XC(d0, dp1, -sizeof(ae_f32));

		/* Reset coefficient pointer and clear accumulator */
		coefp = (ae_f16x4 *)cp;
		a0 = AE_ZERO64();

		/* Compute FIR filter for current channel with four
		 * taps per every loop iteration. Data is read from
		 * interleaved buffer with stride of channels count.
		 */
		for (i = 0; i < taps_div_4; i++) {
			/* Load four coefficients */
			AE_LA16X4_IP(coef4, u, coefp);

			/* Load two data samples, place to high and
			 * low of data2.
			 */
			AE_L32_XC(d0, dp0, inc);
			AE_L32_XC(d1, dp0, inc);
			data2 = AE_SEL32_LL(d0, d1);

			/* Accumulate
			 * data2_h * coef4_3 + data2_l* coef4_2.
			 * The data is 32 bits Q1.31 and coefficient 16 bits
			 * Q1.15. The accumulator is Q17.47.
			 */
			AE_MULAAFD32X16_H3_L2(a0, data2, coef4);

			/* Repeat with next two samples */
			AE_L32_XC(d0, dp0, inc);
			AE_L32_XC(d1, dp0, inc);
			data2 = AE_SEL32_LL(d0, d1);

			/* Accumulate
			 * data2_h * coef4_1 + data2_l * coef4_0.
			 */
			AE_MULAAFD32X16_H1_L0(a0, data2, coef4);
		}

		/* Scale FIR output with right shifts, round/saturate Q17.47
		 * to Q1.31, and store 32 bit output. Advance write
		 * pointer to next sample.
		 */
		AE_S32_L_XP(AE_ROUND32F48SSYM(AE_SRAA64(a0, shift)), wp,
			    sizeof(int32_t));
	}
}

#else /* 32bit coefficients version */

static inline void fir_filter(ae_f32 *rp, const void *cp, ae_f32 *wp0,
			      const int taps_div_4, const int shift,
			      const int nch)
{
	/* This function uses
	 * 6x 64 bit registers
	 * 3x integers
	 * 5x address pointers,
	 */
	ae_f64 a0;
	ae_f64 a1;
	ae_f24x2 data2 = AE_ZERO24();
	ae_f24x2 coef2 = AE_ZERO24();
	ae_f24x2 d0 = AE_ZERO24();
	ae_f24x2 d1 = AE_ZERO24();
	ae_f24x2 *coefp;
	ae_f24x2 *dp;
	ae_f24 *dp1;
	ae_f24 *dp0;
	int i;
	int j;
	ae_f32 *wp = wp0;
	const int inc = nch * sizeof(int32_t);

	if (nch == 2) {
		/* Move data pointer back by one sample to start from right
		 * channel sample. Discard read value p0.
		 */
		dp = (ae_f24x2 *)rp;
		AE_L32F24_XC(d0, (ae_f24 *)dp, -sizeof(ae_f24));

		/* Reset coefficient pointer and clear accumulator */
		coefp = (ae_f24x2 *)cp;
		a0 = AE_ZERO64();
		a1 = AE_ZERO64();

		/* Compute FIR filter for current channel with four
		 * taps per every loop iteration.  Two coefficients
		 * are loaded simultaneously. Data is read
		 * from interleaved buffer with stride of channels
		 * count.
		 */
		for (i = 0; i < taps_div_4; i++) {
			/* Load two coefficients. Coef2_h contains tap *coefp
			 * and coef2_l contains the next tap.
			 */
			/* TODO: Ensure coefficients are 64 bits aligned */
			AE_L32X2F24_IP(coef2, coefp, sizeof(ae_f24x2));

			/* Load two data samples from two channels */
			AE_L32X2F24_XC(d0, dp, inc); /* r0, l0 */
			AE_L32X2F24_XC(d1, dp, inc); /* r1, l1 */

			/* Select to d0 successive left channel samples, to d1
			 * successive right channel samples. Then Accumulate
			 * to a0 and a1
			 * data2_h * coef2_h + data2_l * coef2_l. The Q1.31
			 * data and Q1.15 coefficients are used as 24 bits as
			 * Q1.23 values.
			 */
			data2 = AE_SELP24_LL(d0, d1);
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);
			data2 = AE_SELP24_HH(d0, d1);
			AE_MULAAFP24S_HH_LL(a1, data2, coef2);

			/* Repeat for next two taps */
			AE_L32X2F24_IP(coef2, coefp, sizeof(ae_f24x2));
			AE_L32X2F24_XC(d0, dp, inc); /* r2, l2 */
			AE_L32X2F24_XC(d1, dp, inc); /* r3, l3 */
			data2 = AE_SELP24_LL(d0, d1);
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);
			data2 = AE_SELP24_HH(d0, d1);
			AE_MULAAFP24S_HH_LL(a1, data2, coef2);
		}

		/* Scale FIR output with right shifts, round/saturate
		 * to Q1.31, and store 32 bit output.
		 */
		AE_S32_L_XP(AE_ROUND32F48SSYM(AE_SRAA64(a0, shift)), wp,
			    sizeof(int32_t));
		AE_S32_L_XP(AE_ROUND32F48SSYM(AE_SRAA64(a1, shift)), wp,
			    sizeof(int32_t));

		return;
	}

	dp1 = (ae_f24 *)rp;
	for (j = 0; j < nch; j++) {
		/* Copy pointer and advance to next ch with dummy load */
		dp0 = dp1;
		AE_L32F24_XC(data2, dp1, -sizeof(ae_f24));

		/* Reset coefficient pointer and clear accumulator */
		coefp = (ae_f24x2 *)cp;
		a0 = AE_ZERO64();

		/* Compute FIR filter for current channel with four
		 * taps per every loop iteration. Data is read from
		 * interleaved buffer with stride of channels count.
		 */
		for (i = 0; i < taps_div_4; i++) {
			/* Load two coefficients */
			coef2 = *coefp++;

			/* Load two data samples, place to high and
			 * low of data2.
			 */
			AE_L32F24_XC(d0, dp0, inc);
			AE_L32F24_XC(d1, dp0, inc);
			data2 = AE_SELP24_LL(d0, d1);

			/* Accumulate to data2_h * coef2_h +
			 * data2_l*coef2_l. The Q1.31 bit data is used
			 * as Q1.23 from MSB side bits of the 32 bit
			 * word. The accumulator m is Q17.47.
			 */
			AE_MULAAFD24_HH_LL(a0, data2, coef2);

			/* Repeat the same for next two filter taps */
			coef2 = *coefp++;
			AE_L32F24_XC(d0, dp0, inc);
			AE_L32F24_XC(d1, dp0, inc);
			data2 = AE_SELP24_LL(d0, d1);
			AE_MULAAFD24_HH_LL(a0, data2, coef2);
		}

		/* Scale FIR output with right shifts, round/saturate Q17.47
		 * to Q1.31, and store 32 bit output. Advance write
		 * pointer to next sample.
		 */
		AE_S32_L_XP(AE_ROUND32F48SSYM(AE_SRAA64(a0, shift)), wp,
			    sizeof(int32_t));
	}
}

#endif /* 32bit coefficients version */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
void src_polyphase_stage_cir(struct src_stage_prm *s)
{
	/* This function uses
	 *  1x 64 bit registers
	 * 16x integers
	 * 11x address pointers,
	 */
	ae_int32x2 q = AE_ZERO32();
	ae_f32 *rp;
	ae_f32 *wp;
	int i;
	int n;
	int m;
	int n_wrap_buf;
	int n_min;
	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int32_t *fir_end = &fir->fir_delay[fir->fir_delay_size];
	int32_t *out_delay_end = &fir->out_delay[fir->out_delay_size];
	const void *cp; /* Can be int32_t or int16_t */
	const size_t out_size = fir->out_delay_size * sizeof(int32_t);
	const int nch = s->nch;
	const int nch_x_odm = cfg->odm * nch;
	const int blk_in_words = nch * cfg->blk_in;
	const int blk_out_words = nch * cfg->num_of_subfilters;
	const int sz = sizeof(int32_t);
	const int n_sz = -sizeof(int32_t);
	const int rewind_sz = sz * nch * (cfg->blk_in + (cfg->num_of_subfilters - 1) * cfg->idm);
	const int nch_x_idm_sz = -nch * cfg->idm * sizeof(int32_t);
	const int taps_div_4 = cfg->subfilter_length >> 2;
	int32_t *x_rptr = (int32_t *)s->x_rptr;
	int32_t *y_wptr = (int32_t *)s->y_wptr;
	int32_t *x_end_addr = (int32_t *)s->x_end_addr;
	int32_t *y_end_addr = (int32_t *)s->y_end_addr;

#if SRC_SHORT
	const size_t subfilter_size = cfg->subfilter_length * sizeof(int16_t);
#else
	const size_t subfilter_size = cfg->subfilter_length * sizeof(int32_t);
#endif

	for (n = 0; n < s->times; n++) {
		/* Input data to filter */
		m = blk_in_words;

		/* Setup circular buffer for FIR input data delay */
		AE_SETCBEGIN0(fir->fir_delay);
		AE_SETCEND0(fir_end);

		while (m > 0) {
			/* Number of words until circular wrap */
			n_wrap_buf = x_end_addr - x_rptr;
			n_min = (m < n_wrap_buf) ? m : n_wrap_buf;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				/* Load 32 bits sample to accumulator,
				 * advance pointer, shift left with saturate.
				 */
				AE_L32_XP(q, (ae_int32 *)x_rptr, sz);
				q = AE_SLAA32(q, s->shift);

				/* Store to circular buffer, advance pointer */
				AE_S32_L_XC(q, (ae_int32 *)fir->fir_wp, n_sz);
			}

			/* Check for wrap */
			src_inc_wrap(&x_rptr, x_end_addr, s->x_size);
		}

		/* Do filter */
		cp = cfg->coefs; /* Reset to 1st coefficient */
		rp = (ae_f32 *)fir->fir_wp;

		/* Do circular modification to pointer rp by amount of
		 * rewind to to data start. Loaded value q is discarded.
		 */
		AE_L32_XC(q, rp, rewind_sz);

		/* Reset FIR write pointer and compute all polyphase
		 * sub-filters.
		 */
		wp = (ae_f32 *)fir->out_rp;
		for (i = 0; i < cfg->num_of_subfilters; i++) {
			fir_filter(rp, cp, wp, taps_div_4, cfg->shift, nch);
			wp += nch_x_odm;
			cp = (char *)cp + subfilter_size;
			src_inc_wrap((int32_t **)&wp, out_delay_end, out_size);

			/* Circular advance pointer rp by number of
			 * channels x input delay multiplier. Loaded value q
			 * is discarded.
			 */
			AE_L32_XC(q, rp, nch_x_idm_sz);
		}

		/* Output */

		/* Setup circular buffer for SRC out delay access */
		AE_SETCBEGIN0(fir->out_delay);
		AE_SETCEND0(out_delay_end);
		m = blk_out_words;
		while (m > 0) {
			n_wrap_buf = y_end_addr - y_wptr;
			n_min = (m < n_wrap_buf) ? m : n_wrap_buf;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				/* Circular load, shift right, linear store,
				 * and advance read and write pointers.
				 */
				AE_L32_XC(q, (ae_int32 *)fir->out_rp, sz);
				q = AE_SRAA32(q, s->shift);
				AE_S32_L_XP(q, (ae_int32 *)y_wptr, sz);
			}

			/* Check wrap */
			src_inc_wrap(&y_wptr, y_end_addr, s->y_size);
		}
	}
	s->x_rptr = x_rptr;
	s->y_wptr = y_wptr;
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
void src_polyphase_stage_cir_s16(struct src_stage_prm *s)
{
	/* This function uses
	 *  2x 64 bit registers
	 * 16x integers
	 * 11x address pointers,
	 */
	ae_int32x2 q = AE_ZERO32();
	ae_int16x4 d = AE_ZERO16();
	ae_f32 *rp;
	ae_f32 *wp;
	int i;
	int n;
	int m;
	int n_wrap_buf;
	int n_min;

	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int32_t *fir_end = &fir->fir_delay[fir->fir_delay_size];
	int32_t *out_delay_end = &fir->out_delay[fir->out_delay_size];
	const void *cp; /* Can be int32_t or int16_t */
	const size_t out_size = fir->out_delay_size * sizeof(int32_t);
	const int nch = s->nch;
	const int nch_x_odm = cfg->odm * nch;
	const int blk_in_words = nch * cfg->blk_in;
	const int blk_out_words = nch * cfg->num_of_subfilters;
	const int sz = sizeof(int32_t);
	const int n_sz = -sizeof(int32_t);
	const int rewind_sz = sz * nch * (cfg->blk_in + (cfg->num_of_subfilters - 1) * cfg->idm);
	const int nch_x_idm_sz = -nch * cfg->idm * sizeof(int32_t);
	const int taps_div_4 = cfg->subfilter_length >> 2;
	int16_t *x_rptr = (int16_t *)s->x_rptr;
	int16_t *y_wptr = (int16_t *)s->y_wptr;
	int16_t *x_end_addr = (int16_t *)s->x_end_addr;
	int16_t *y_end_addr = (int16_t *)s->y_end_addr;

#if SRC_SHORT
	const size_t subfilter_size = cfg->subfilter_length * sizeof(int16_t);
#else
	const size_t subfilter_size = cfg->subfilter_length * sizeof(int32_t);
#endif

	for (n = 0; n < s->times; n++) {
		/* Input data */
		m = blk_in_words;

		/* Setup circular buffer for FIR input data delay */
		AE_SETCBEGIN0(fir->fir_delay);
		AE_SETCEND0(fir_end);

		while (m > 0) {
			/* Number of words without circular wrap */
			n_wrap_buf = x_end_addr - x_rptr;
			n_min = (m < n_wrap_buf) ? m : n_wrap_buf;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				/* Load a 16 bits sample into d and left shift
				 * by 16 into q, advance read and write
				 * pointers.
				 */
				AE_L16_XP(d, (ae_int16 *)x_rptr,
					  sizeof(ae_int16));
				q = AE_CVT32X2F16_32(d);
				AE_S32_L_XC(q, (ae_int32 *)fir->fir_wp, n_sz);
			}

			/* Check for wrap */
			src_inc_wrap_s16(&x_rptr, x_end_addr, s->x_size);
		}

		/* Do filter */
		cp = cfg->coefs; /* Reset to 1st coefficient */
		rp = (ae_f32 *)fir->fir_wp;

		/* Do circular modification to pointer rp by amount of
		 * rewind to to data start. Loaded value q is discarded.
		 */
		AE_L32_XC(q, rp, rewind_sz);

		/* Reset FIR output write pointer and compute all polyphase
		 * sub-filters.
		 */
		wp = (ae_f32 *)fir->out_rp;
		for (i = 0; i < cfg->num_of_subfilters; i++) {
			fir_filter(rp, cp, wp, taps_div_4, cfg->shift, nch);
			wp += nch_x_odm;
			cp = (char *)cp + subfilter_size;
			src_inc_wrap((int32_t **)&wp, out_delay_end, out_size);

			/* Circular advance pointer rp by number of
			 * channels x input delay multiplier. Loaded value q
			 * is discarded.
			 */
			AE_L32_XC(q, rp, nch_x_idm_sz);
		}

		/* Output */

		/* Setup circular buffer for SRC out delay access */
		AE_SETCBEGIN0(fir->out_delay);
		AE_SETCEND0(out_delay_end);
		m = blk_out_words;
		while (m > 0) {
			n_wrap_buf = y_end_addr - y_wptr;
			n_min = (m < n_wrap_buf) ? m : n_wrap_buf;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				/* Circular load for 32 bit sample,
				 * advance read pointer.
				 */
				AE_L32_XC(q, (ae_int32 *)fir->out_rp, sz);

				/* Store Q1.31 value as Q1.15 and
				 * advance write pointer.
				 */
				d = AE_ROUND16X4F32SSYM(q, q);
				AE_S16_0_XP(d, (ae_int16 *)y_wptr,
					    sizeof(ae_int16));
			}

			/* Check wrap */
			src_inc_wrap_s16(&y_wptr, y_end_addr, s->y_size);
		}
	}
	s->x_rptr = x_rptr;
	s->y_wptr = y_wptr;
}
#endif /* CONFIG_FORMAT_S16LE */

#endif
