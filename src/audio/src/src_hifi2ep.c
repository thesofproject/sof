// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

/* HiFi EP optimized code parts for SRC */

#include "src_config.h"

#if SRC_HIFIEP

#include "src.h"

#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi2.h>
#include <stddef.h>
#include <stdint.h>

/* HiFi EP has
 * 4x 56 bit registers in register file Q
 * 8x 48 bit registers in register file P
 */

#if SRC_SHORT /* 16 bit coefficients version */

static inline void fir_filter(ae_q32s *rp, const void *cp, ae_q32s *wp0,
			      const int taps_div_4, const int shift,
			      const int nch)
{
	/* This function uses
	 * 2x 56 bit registers Q,
	 * 4x 48 bit registers P
	 * 3x integers
	 * 4x address pointers,
	 */
	ae_q56s a0;
	ae_q56s a1;
	ae_p24x2f data2;
	ae_p24x2f coef2;
	ae_p24x2f p0;
	ae_p24x2f p1;
	ae_p16x2s *coefp;
	ae_p24x2f *dp = (ae_p24x2f *)rp;
	ae_p24x2f *dp0;
	ae_q32s *wp = wp0;
	int i;
	int j;
	const int inc = sizeof(ae_p24x2f);

	/* 2ch FIR case */
	if (nch == 2) {
		/* Move data pointer back by one sample to start from right
		 * channel sample. Discard read value p0.
		 */
		AE_LP24F_C(p0, dp, -sizeof(ae_p24f));

		/* Reset coefficient pointer and clear accumulator */
		coefp = (ae_p16x2s *)cp;
		a0 = AE_ZEROQ56();
		a1 = AE_ZEROQ56();

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
			coef2 = AE_LP16X2F_I(coefp, 0);
			coefp++;

			/* Load two data samples from two channels */
			AE_LP24X2F_C(p0, dp, inc); /* r0, l0 */
			AE_LP24X2F_C(p1, dp, inc); /* r1, l1 */

			/* Select to d0 successive left channel samples, to d1
			 * successive right channel samples. Then accumulate
			 * data2_h * coef2_h + data2_l * coef2_l. The Q1.31
			 * data and Q1.15 coefficients are used as 24 bits as
			 * Q1.23 values.
			 */
			data2 = AE_SELP24_LL(p0, p1);
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);
			data2 = AE_SELP24_HH(p0, p1);
			AE_MULAAFP24S_HH_LL(a1, data2, coef2);

			/* Repeat for next two taps */
			coef2 = AE_LP16X2F_I(coefp, 0);
			coefp++;
			AE_LP24X2F_C(p0, dp, inc); /* r2, l2 */
			AE_LP24X2F_C(p1, dp, inc); /* r3, l3 */
			data2 = AE_SELP24_LL(p0, p1);
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);
			data2 = AE_SELP24_HH(p0, p1);
			AE_MULAAFP24S_HH_LL(a1, data2, coef2);
		}

		/* Scale FIR output with right shifts, round/saturate
		 * to Q1.31, and store 32 bit output.
		 */
		AE_SQ32F_I(AE_ROUNDSQ32SYM(AE_SRAAQ56(a0, shift)), wp, 0);
		AE_SQ32F_I(AE_ROUNDSQ32SYM(AE_SRAAQ56(a1, shift)), wp,
			   sizeof(int32_t));
		return;
	}

	for (j = 0; j < nch; j++) {
		/* Copy pointer and advance to next ch with dummy load */
		dp0 = dp;
		AE_LP24F_C(p0, dp, -sizeof(ae_p24f));

		/* Reset coefficient pointer and clear accumulator */
		coefp = (ae_p16x2s *)cp;
		a0 = AE_ZEROQ56();

		/* Compute FIR filter for current channel with four
		 * taps per every loop iteration.  Two coefficients
		 * are loaded simultaneously. Data is read
		 * from interleaved buffer with stride of channels
		 * count.
		 */
		for (i = 0; i < taps_div_4; i++) {
			/* Load two coefficients */
			coef2 = *coefp++;

			/* Load two data samples */
			AE_LP24F_C(p0, dp0, inc);
			AE_LP24F_C(p1, dp0, inc);

			/* Pack p0 and p1 to data2_h and data2_l */
			data2 = AE_SELP24_LL(p0, p1);

			/* Accumulate data2_h * coef2_h + data2_l * coef2_l */
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);

			/* Repeat for next two filter taps */
			coef2 = *coefp++;
			AE_LP24F_C(p0, dp0, inc);
			AE_LP24F_C(p1, dp0, inc);
			data2 = AE_SELP24_LL(p0, p1);
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);
		}

		/* Scale FIR output with right shifts, round/saturate
		 * to Q1.31, and store 32 bit output. Advance write
		 * pointer to next sample.
		 */
		AE_SQ32F_I(AE_ROUNDSQ32SYM(AE_SRAAQ56(a0, shift)), wp, 0);
		wp++;
	}
}

#else /* 32bit coefficients version */

static inline void fir_filter(ae_q32s *rp, const void *cp, ae_q32s *wp0,
			      const int taps_div_4, const int shift,
			      const int nch)
{
	/* This function uses
	 * 2x 56 bit registers Q,
	 * 4x 48 bit registers P
	 * 3x integers
	 * 4x address pointers,
	 */
	ae_q56s a0;
	ae_q56s a1;
	ae_p24x2f p0;
	ae_p24x2f p1;
	ae_p24x2f data2;
	ae_p24x2f coef2;
	ae_p24x2f *coefp;
	ae_p24x2f *dp = (ae_p24x2f *)rp;
	ae_p24x2f *dp0;
	ae_q32s *wp = wp0;
	int i;
	int j;
	const int inc = sizeof(ae_p24x2f);

	/* 2ch FIR case */
	if (nch == 2) {
		/* Move data pointer back by one sample to start from right
		 * channel sample. Discard read value p0.
		 */
		AE_LP24F_C(p0, dp, -sizeof(ae_p24f));

		/* Reset coefficient pointer and clear accumulator */
		coefp = (ae_p24x2f *)cp;
		a0 = AE_ZEROQ56();
		a1 = AE_ZEROQ56();

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
			coef2 = AE_LP24X2F_I(coefp, 0);
			coefp++;

			/* Load two data samples from two channels */
			AE_LP24X2F_C(p0, dp, inc); /* r0, l0 */
			AE_LP24X2F_C(p1, dp, inc); /* r1, l1 */

			/* Select to d0 successive left channel samples, to d1
			 * successive right channel samples.
			 */

			/* Accumulate to m
			 * data2_h * coef2_h + data2_l * coef2_l. The Q1.31
			 * data and Q1.15 coefficients are used as 24 bits as
			 * Q1.23 values.
			 */
			data2 = AE_SELP24_LL(p0, p1);
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);
			data2 = AE_SELP24_HH(p0, p1);
			AE_MULAAFP24S_HH_LL(a1, data2, coef2);

			/* Repeat for next two taps */
			coef2 = AE_LP24X2F_I(coefp, 0);
			coefp++;
			AE_LP24X2F_C(p0, dp, inc); /* r2, l2 */
			AE_LP24X2F_C(p1, dp, inc); /* r3, l3 */
			data2 = AE_SELP24_LL(p0, p1);
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);
			data2 = AE_SELP24_HH(p0, p1);
			AE_MULAAFP24S_HH_LL(a1, data2, coef2);
		}

		/* Scale FIR output with right shifts, round/saturate
		 * to Q1.31, and store 32 bit output.
		 */
		AE_SQ32F_I(AE_ROUNDSQ32SYM(AE_SRAAQ56(a0, shift)), wp, 0);
		AE_SQ32F_I(AE_ROUNDSQ32SYM(AE_SRAAQ56(a1, shift)), wp,
			   sizeof(int32_t));
		return;
	}

	for (j = 0; j < nch; j++) {
		/* Copy pointer and advance to next ch with dummy load */
		dp0 = dp;
		AE_LP24F_C(p0, dp, -sizeof(ae_p24f));

		/* Reset coefficient pointer and clear accumulator */
		coefp = (ae_p24x2f *)cp;
		a0 = AE_ZEROQ56();

		/* Compute FIR filter for current channel with four
		 * taps per every loop iteration.  Two coefficients
		 * are loaded simultaneously. Data is read
		 * from interleaved buffer with stride of channels
		 * count.
		 */
		for (i = 0; i < taps_div_4; i++) {
			/* Load two coefficients */
			coef2 = *coefp++;

			/* Load two data samples and place them to L and H of
			 * data2.
			 */
			AE_LP24F_C(p0, dp0, inc);
			AE_LP24F_C(p1, dp0, inc);
			data2 = AE_SELP24_LH(p0, p1);

			/* Accumulate to m
			 * data2_h * coef2_h + data2_l * coef2_l. The Q1.31
			 * data and coefficients are used as the most
			 * significant 24 bits as Q1.23 values.
			 */
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);

			/* Repeat for next two filter taps */
			coef2 = *coefp++;
			AE_LP24F_C(p0, dp0, inc);
			AE_LP24F_C(p1, dp0, inc);
			data2 = AE_SELP24_LH(p0, p1);
			AE_MULAAFP24S_HH_LL(a0, data2, coef2);
		}

		/* Scale FIR output with right shifts, round/saturate
		 * to Q1.31, and store 32 bit output. Advance write
		 * pointer to next sample.
		 */
		AE_SQ32F_I(AE_ROUNDSQ32SYM(AE_SRAAQ56(a0, shift)), wp, 0);
		wp++;
	}
}
#endif /* 32bit coefficients version */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
void src_polyphase_stage_cir(struct src_stage_prm *s)
{
	/* This function uses
	 *  1x 56 bit registers Q,
	 *  0x 48 bit registers P,
	 * 16x integers
	 * 11x address pointers,
	 */
	ae_q56s q;
	ae_q32s *rp;
	ae_q32s *wp;
	int i;
	int n;
	int m;
	int n_wrap_buf;
	int n_min;
	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int32_t *fir_end = &fir->fir_delay[fir->fir_delay_size];
	int32_t *out_delay_end = &fir->out_delay[fir->out_delay_size];
	const char *cp; /* Can be int32_t or int16_t */
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
				/* Load 32 bits sample to accumulator */
				q = AE_LQ32F_I((ae_q32s *)x_rptr++, 0);

				/* Saturating shift left */
				q = AE_SLLASQ56S(q, s->shift);

				/* Store to circular buffer, advance pointer */
				AE_SQ32F_C(q, (ae_q32s *)fir->fir_wp, n_sz);
			}

			/* Check for wrap */
			src_inc_wrap(&x_rptr, x_end_addr, s->x_size);
		}

		/* Do filter */
		cp = cfg->coefs; /* Reset to 1st coefficient */
		rp = (ae_q32s *)fir->fir_wp;

		/* Do circular modification to pointer rp by amount of
		 * rewind to to data start. Loaded value q is discarded.
		 */
		AE_LQ32F_C(q, (ae_q32s *)rp, rewind_sz);

		/* Reset FIR write pointer and compute all polyphase
		 * sub-filters.
		 */
		wp = (ae_q32s *)fir->out_rp;
		for (i = 0; i < cfg->num_of_subfilters; i++) {
			fir_filter(rp, cp, wp, taps_div_4, cfg->shift, nch);
			wp += nch_x_odm;
			cp += subfilter_size;
			src_inc_wrap((int32_t **)&wp, out_delay_end, out_size);

			/* Circular advance pointer rp by number of
			 * channels x input delay multiplier. Loaded value q
			 * is discarded.
			 */
			AE_LQ32F_C(q, rp, nch_x_idm_sz);
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
				/* Circular load followed by shift right
				 * for optional s24 and linear store
				 */
				AE_LQ32F_C(q, (ae_q32s *)fir->out_rp, sz);
				q = AE_SRAAQ56(q, s->shift);
				AE_SQ32F_I(q, (ae_q32s *)y_wptr, 0);
				y_wptr++;
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
	 *  0x 56 bit registers Q,
	 *  1x 48 bit registers P,
	 * 16x integers
	 * 11x address pointers,
	 */
	ae_p24x2s d;
	ae_q32s *rp;
	ae_q32s *wp;
	int i;
	int n;
	int m;
	int n_wrap_buf;
	int n_min;
	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int32_t *fir_end = &fir->fir_delay[fir->fir_delay_size];
	int32_t *out_delay_end = &fir->out_delay[fir->out_delay_size];
	const char *cp; /* Can be int32_t or int16_t */
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
		/* Input data to filter */
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
				/* Load 16 bits sample to 24 bit register
				 * and advance read pointer.
				 */
				d = AE_LP16F_I((ae_p16s *)x_rptr, 0);
				x_rptr++;

				/* Store to 32 bit circular buffer, advance
				 * write pointer.
				 */
				AE_SP24F_L_C(d, (ae_p24f *)fir->fir_wp, n_sz);
			}

			/* Check for wrap */
			src_inc_wrap_s16(&x_rptr, x_end_addr, s->x_size);
		}

		/* Do filter */
		cp = cfg->coefs; /* Reset to 1st coefficient */
		rp = (ae_q32s *)fir->fir_wp;

		/* Do circular modification to pointer rp by amount of
		 * rewind to to data start. Loaded value d is discarded.
		 */
		AE_LP24F_C(d, (ae_p24f *)rp, rewind_sz);

		/* Reset FIR output write pointer and compute all polyphase
		 * sub-filters.
		 */
		wp = (ae_q32s *)fir->out_rp;
		for (i = 0; i < cfg->num_of_subfilters; i++) {
			fir_filter(rp, cp, wp, taps_div_4, cfg->shift, nch);
			wp += nch_x_odm;
			cp += subfilter_size;
			src_inc_wrap((int32_t **)&wp, out_delay_end, out_size);

			/* Circular advance pointer rp by number of
			 * channels x input delay multiplier. Loaded value d
			 * is discarded.
			 */
			AE_LP24F_C(d, (ae_p24f *)rp, nch_x_idm_sz);
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
				/* Circular load for 32 bit sample, get 24
				 * high bits, circular advance pointer.
				 */
				AE_LP24F_C(d, (ae_p24f *)fir->out_rp, sz);

				/* Round Q1.12 value and store as Q1.15 and
				 * advance pointer.
				 */
				d = AE_ROUNDSP16SYM(d);
				AE_SP16F_L_I(d, (ae_p16s *)y_wptr, 0);
				y_wptr++;
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
