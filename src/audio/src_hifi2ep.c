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
 *
 */

/* HiFi EP optimized code parts for SRC */

#include <stdint.h>
#include <sof/alloc.h>
#include <sof/audio/format.h>
#include <sof/math/numbers.h>

#include "src_config.h"
#include "src.h"

#if SRC_HIFIEP

#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi2.h>

/* HiFi EP has
 * 4x 56 bit registers in register file Q
 * 8x 48 bit registers in register file P
 */

#if SRC_SHORT /* 16 bit coefficients version */

static inline void fir_filter(ae_q32s *rp, const void *cp, ae_q32s *wp0,
	const int taps_div_4, const int shift, const int nch)
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
	const int taps_div_4, const int shift, const int nch)
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

void src_polyphase_stage_cir(struct src_stage_prm *s)
{
	/* This function uses
	 *  1x 56 bit registers Q,
	 *  0x 48 bit registers P,
	 * 16x integers
	 *  7x address pointers,
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
	const void *cp; /* Can be int32_t or int16_t */
	const size_t out_size = fir->out_delay_size * sizeof(int32_t);
	const int nch = s->nch;
	const int nch_x_odm = cfg->odm * nch;
	const int blk_in_words = nch * cfg->blk_in;
	const int blk_out_words = nch * cfg->num_of_subfilters;
	const int sz = sizeof(int32_t);
	const int n_sz = -sizeof(int32_t);
	const int rewind_sz = sz * (nch * (cfg->blk_in
		+ (cfg->num_of_subfilters - 1) * cfg->idm) - nch);
	const int nch_x_idm_sz = -nch * cfg->idm * sizeof(int32_t);
	const int taps_div_4 = cfg->subfilter_length >> 2;

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
			n_wrap_buf = s->x_end_addr - s->x_rptr;
			n_min = (m < n_wrap_buf) ? m : n_wrap_buf;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				/* Load 32 bits sample to accumulator */
				q = AE_LQ32F_I((ae_q32s *)s->x_rptr++, 0);

				/* Store to circular buffer, advance pointer */
				AE_SQ32F_C(q, (ae_q32s *)fir->fir_wp, n_sz);
			}

			/* Check for wrap */
			src_circ_inc_wrap(&s->x_rptr, s->x_end_addr, s->x_size);
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
			src_circ_inc_wrap((int32_t **)&wp, out_delay_end,
				out_size);

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
			n_wrap_buf = s->y_end_addr - s->y_wptr;
			n_min = (m < n_wrap_buf) ? m : n_wrap_buf;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				/* Circular load followed by linear store */
				AE_LQ32F_C(q, (ae_q32s *)fir->out_rp, sz);
				AE_SQ32F_I(q, (ae_q32s *)s->y_wptr, 0);
				s->y_wptr++;
			}
			/* Check wrap */
			src_circ_inc_wrap(&s->y_wptr, s->y_end_addr, s->y_size);
		}
	}
}

void src_polyphase_stage_cir_s24(struct src_stage_prm *s)
{
	/* This function uses
	 *  1x 56 bit registers Q,
	 *  0x 48 bit registers P,
	 * 16x integers
	 *  7x address pointers,
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
	const void *cp; /* Can be int32_t or int16_t */
	const size_t out_size = fir->out_delay_size * sizeof(int32_t);
	const int nch = s->nch;
	const int nch_x_odm = cfg->odm * nch;
	const int blk_in_words = nch * cfg->blk_in;
	const int blk_out_words = nch * cfg->num_of_subfilters;
	const int sz = sizeof(int32_t);
	const int n_sz = -sizeof(int32_t);
	const int rewind_sz = sz * (nch * (cfg->blk_in
		+ (cfg->num_of_subfilters - 1) * cfg->idm) - nch);
	const int nch_x_idm_sz = -nch * cfg->idm * sizeof(int32_t);
	const int taps_div_4 = cfg->subfilter_length >> 2;

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
			n_wrap_buf = s->x_end_addr - s->x_rptr;
			n_min = (m < n_wrap_buf) ? m : n_wrap_buf;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				/* Load 32 bits sample to accumulator
				 * and left shift by 8, advance read
				 * pointer.
				 */
				q = AE_SLLIQ56(AE_LQ32F_I(
					(ae_q32s *)s->x_rptr++, 0), 8);

				/* Store to circular buffer, advance
				 * write pointer.
				 */
				AE_SQ32F_C(q, (ae_q32s *)fir->fir_wp, n_sz);
			}

			/* Check for wrap */
			src_circ_inc_wrap(&s->x_rptr, s->x_end_addr, s->x_size);
		}

		/* Do filter */
		cp = cfg->coefs; /* Reset to 1st coefficient */
		rp = (ae_q32s *)fir->fir_wp;

		/* Do circular modification to pointer rp by amount of
		 * rewind to to data start. Loaded value q is discarded.
		 */
		AE_LQ32F_C(q, (ae_q32s *)rp, rewind_sz);

		/* Reset FIR output write pointer and compute all polyphase
		 * sub-filters.
		 */
		wp = (ae_q32s *)fir->out_rp;
		for (i = 0; i < cfg->num_of_subfilters; i++) {
			fir_filter(rp, cp, wp, taps_div_4, cfg->shift, nch);
			wp += nch_x_odm;
			cp += subfilter_size;
			src_circ_inc_wrap((int32_t **)&wp, out_delay_end,
				out_size);

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
			n_wrap_buf = s->y_end_addr - s->y_wptr;
			n_min = (m < n_wrap_buf) ? m : n_wrap_buf;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				/* Circular load for 32 bit sample,
				 * advance pointer.
				 */
				AE_LQ32F_C(q, (ae_q32s *)fir->out_rp, sz);

				/* Store value as shifted right by 8 for
				 * sign extended 24 bit value, advance pointer.
				 */
				AE_SQ32F_I(AE_SRAIQ56(q, 8),
					   (ae_q32s *)s->y_wptr, 0);
				s->y_wptr++;
			}
			/* Check wrap */
			src_circ_inc_wrap(&s->y_wptr, s->y_end_addr, s->y_size);
		}
	}
}

#endif
