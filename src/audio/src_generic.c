/*
 * Copyright (c) 2016, Intel Corporation
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

/* Default C implementation guaranteed to work on any
 * architecture.
 */

#include <stdint.h>
#include <sof/alloc.h>
#include <sof/audio/format.h>
#include <sof/math/numbers.h>

#include "src_config.h"
#include "src.h"

#if SRC_GENERIC

#if SRC_SHORT /* 16 bit coefficients version */

static inline void fir_filter_generic(int32_t *rp, const void *cp, int32_t *wp0,
				      int32_t *fir_start, int32_t *fir_end,
				      const int fir_delay_length,
				      const int taps_x_nch,
				      const int shift, const int nch)
{
	int64_t y0;
	int64_t y1;
	int32_t *data;
	const int16_t *coef;
	int i;
	int j;
	int n1;
	int n2;
	int frames;
	const int qshift = 15 + shift; /* Q2.46 -> Q2.31 */
	const int32_t rnd = 1 << (qshift - 1); /* Half LSB */
	int32_t *d = rp;
	int32_t *wp = wp0;

	/* Check for 2ch FIR case */
	if (nch == 2) {
		/* Decrement data pointer to next channel start. Note that
		 * initialization code ensures that circular wrap does not
		 * happen mid-frame.
		 */
		data = d - 1;

		/* Initialize to half LSB for rounding, prepare for FIR core */
		y0 = rnd;
		y1 = rnd;
		coef = (const int16_t *)cp;
		frames = fir_end - data; /* Frames until wrap */
		n1 = ((taps_x_nch < frames) ? taps_x_nch : frames) >> 1;
		n2 = (taps_x_nch >> 1) - n1;

		/* The FIR is calculated as Q1.15 x Q1.31 -> Q2.46. The
		 * output shift includes the shift by 15 for Qx.46 to
		 * Qx.31.
		 */
		for (i = 0; i < n1; i++) {
			y0 += (int64_t)(*coef) * (*data);
			data++;
			y1 += (int64_t)(*coef) * (*data);
			data++;
			coef++;
		}
		if (data == fir_end)
			data = fir_start;

		for (i = 0; i < n2; i++) {
			y0 += (int64_t)(*coef) * (*data);
			data++;
			y1 += (int64_t)(*coef) * (*data);
			data++;
			coef++;
		}

		*wp = sat_int32(y1 >> qshift);
		*(wp + 1) = sat_int32(y0 >> qshift);
		return;
	}

	for (j = 0; j < nch; j++) {
		/* Decrement data pointer to next channel start. Note that
		 * initialization code ensures that circular wrap does not
		 * happen mid-frame.
		 */
		data = d--;

		/* Initialize to half LSB for rounding, prepare for FIR core */
		y0 = rnd;
		coef = (const int16_t *)cp;
		frames = fir_end - data + nch - j - 1; /* Frames until wrap */
		n1 = (taps_x_nch < frames) ? taps_x_nch : frames;
		n2 = taps_x_nch - n1;

		/* The FIR is calculated as Q1.15 x Q1.31 -> Q2.46. The
		 * output shift includes the shift by 15 for Qx.46 to
		 * Qx.31.
		 */
		for (i = 0; i < n1; i += nch) {
			y0 += (int64_t)(*coef) * (*data);
			coef++;
			data += nch;
		}
		if (data >= fir_end)
			data -= fir_delay_length;

		for (i = 0; i < n2; i += nch) {
			y0 += (int64_t)(*coef) * (*data);
			coef++;
			data += nch;
		}

		*wp = sat_int32(y0 >> qshift);
		wp++;
	}
}

#else /* 32bit coefficients version */

static inline void fir_filter_generic(int32_t *rp, const void *cp, int32_t *wp0,
				      int32_t *fir_start, int32_t *fir_end,
				      int fir_delay_length,
				      const int taps_x_nch, const int shift,
				      const int nch)
{
	int64_t y0;
	int64_t y1;
	int32_t *data;
	const int32_t *coef;
	int i;
	int j;
	int frames;
	int n1;
	int n2;

	const int qshift = 23 + shift; /* Qx.54 -> Qx.31 */
	const int32_t rnd = 1 << (qshift - 1); /* Half LSB */
	int32_t *d = rp;
	int32_t *wp = wp0;

	/* Check for 2ch FIR case */
	if (nch == 2) {
		/* Decrement data pointer to next channel start. Note that
		 * initialization code ensures that circular wrap does not
		 * happen mid-frame.
		 */
		data = d - 1;

		/* Initialize to half LSB for rounding, prepare for FIR core */
		y0 = rnd;
		y1 = rnd;
		coef = (const int32_t *)cp;
		frames = fir_end - data; /* Frames until wrap */
		n1 = ((taps_x_nch < frames) ? taps_x_nch : frames) >> 1;
		n2 = (taps_x_nch >> 1) - n1;

		/* The FIR is calculated as Q1.23 x Q1.31 -> Q2.54. The
		 * output shift includes the shift by 23 for Qx.54 to
		 * Qx.31.
		 */
		for (i = 0; i < n1; i++) {
			y0 += (int64_t)(*coef >> 8) * (*data);
			data++;
			y1 += (int64_t)(*coef >> 8) * (*data);
			data++;
			coef++;
		}
		if (data == fir_end)
			data = fir_start;

		for (i = 0; i < n2; i++) {
			y0 += (int64_t)(*coef >> 8) * (*data);
			data++;
			y1 += (int64_t)(*coef >> 8) * (*data);
			data++;
			coef++;
		}
		*wp = sat_int32(y1 >> qshift);
		*(wp + 1) = sat_int32(y0 >> qshift);
		return;
	}

	for (j = 0; j < nch; j++) {
		/* Decrement data pointer to next channel start. Note that
		 * initialization code ensures that circular wrap does not
		 * happen mid-frame.
		 */
		data = d--;

		/* Initialize to half LSB for rounding, prepare for FIR core */
		y0 = rnd;
		coef = (const int32_t *)cp;
		frames = fir_end - data + nch - j - 1; /* Frames until wrap */
		n1 = (taps_x_nch < frames) ? taps_x_nch : frames;
		n2 = taps_x_nch - n1;

		/* The FIR is calculated as Q1.23 x Q1.31 -> Q2.54. The
		 * output shift includes the shift by 23 for Qx.54 to
		 * Qx.31.
		 */
		for (i = 0; i < n1; i += nch) {
			y0 += (int64_t)(*coef >> 8) * (*data);
			coef++;
			data += nch;
		}
		if (data >= fir_end)
			data -= fir_delay_length;

		for (i = 0; i < n2; i += nch) {
			y0 += (int64_t)(*coef >> 8) * (*data);
			coef++;
			data += nch;
		}
		*wp = sat_int32(y0 >> qshift);
		wp++;
	}
}

#endif /* 32bit coefficients version */

void src_polyphase_stage_cir(struct src_stage_prm *s)
{
	int i;
	int n;
	int m;
	int n_wrap_buf;
	int n_wrap_fir;
	int n_min;
	int32_t *rp;
	int32_t *wp;

	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int32_t *fir_delay = fir->fir_delay;
	int32_t *fir_end = &fir->fir_delay[fir->fir_delay_size];
	int32_t *out_delay_end = &fir->out_delay[fir->out_delay_size];
	const void *cp; /* Can be int32_t or int16_t */
	const size_t out_size = fir->out_delay_size * sizeof(int32_t);
	const int nch = s->nch;
	const int nch_x_odm = cfg->odm * nch;
	const int blk_in_words = nch * cfg->blk_in;
	const int blk_out_words = nch * cfg->num_of_subfilters;
	const int fir_length = fir->fir_delay_size;
	const int rewind = nch * (cfg->blk_in
		+ (cfg->num_of_subfilters - 1) * cfg->idm) - nch;
	const int nch_x_idm = nch * cfg->idm;
	const size_t fir_size = fir->fir_delay_size * sizeof(int32_t);
	const int taps_x_nch = cfg->subfilter_length * nch;
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
		/* Input data, for s24 format s->shift is 8 */
		m = blk_in_words;
		while (m > 0) {
			/* Number of words without circular wrap */
			n_wrap_buf = x_end_addr - x_rptr;
			n_wrap_fir = fir->fir_wp - fir->fir_delay + 1;
			n_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_min) ? m : n_min;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				*fir->fir_wp = *x_rptr << s->shift;
				fir->fir_wp--;
				x_rptr++;
			}
			/* Check for wrap */
			src_dec_wrap(&fir->fir_wp, fir_delay, fir_size);
			src_inc_wrap(&x_rptr, x_end_addr, s->x_size);
		}

		/* Filter */
		cp = cfg->coefs; /* Reset to 1st coefficient */
		rp = fir->fir_wp + rewind;
		src_inc_wrap(&rp, fir_end, fir_size);
		wp = fir->out_rp;
		for (i = 0; i < cfg->num_of_subfilters; i++) {
			fir_filter_generic(rp, cp, wp,
					   fir_delay, fir_end, fir_length,
					   taps_x_nch, cfg->shift, nch);
			wp += nch_x_odm;
			cp += subfilter_size;
			src_inc_wrap(&wp, out_delay_end, out_size);
			rp -= nch_x_idm; /* Next sub-filter start */
			src_dec_wrap(&rp, fir_delay, fir_size);
		}

		/* Output, for s24 format s->shift is 8 */
		m = blk_out_words;
		while (m > 0) {
			n_wrap_fir = out_delay_end - fir->out_rp;
			n_wrap_buf = y_end_addr - y_wptr;
			n_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_min) ? m : n_min;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				*y_wptr = *fir->out_rp >> s->shift;
				y_wptr++;
				fir->out_rp++;
			}
			/* Check wrap */
			src_inc_wrap(&y_wptr, y_end_addr, s->y_size);
			src_inc_wrap(&fir->out_rp, out_delay_end, out_size);
		}
	}
	s->x_rptr = x_rptr;
	s->y_wptr = y_wptr;
}

void src_polyphase_stage_cir_s16(struct src_stage_prm *s)
{
	int i;
	int n;
	int m;
	int n_wrap_buf;
	int n_wrap_fir;
	int n_min;
	int32_t *rp;
	int32_t *wp;

	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int32_t *fir_delay = fir->fir_delay;
	int32_t *fir_end = &fir->fir_delay[fir->fir_delay_size];
	int32_t *out_delay_end = &fir->out_delay[fir->out_delay_size];
	const void *cp; /* Can be int32_t or int16_t */
	const size_t out_size = fir->out_delay_size * sizeof(int32_t);
	const int nch = s->nch;
	const int nch_x_odm = cfg->odm * nch;
	const int blk_in_words = nch * cfg->blk_in;
	const int blk_out_words = nch * cfg->num_of_subfilters;
	const int fir_length = fir->fir_delay_size;
	const int rewind = nch * (cfg->blk_in
		+ (cfg->num_of_subfilters - 1) * cfg->idm) - nch;
	const int nch_x_idm = nch * cfg->idm;
	const size_t fir_size = fir->fir_delay_size * sizeof(int32_t);
	const int taps_x_nch = cfg->subfilter_length * nch;
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
		/* Input data, used fixed shift by 16 */
		m = blk_in_words;
		while (m > 0) {
			/* Number of words without circular wrap */
			n_wrap_buf = x_end_addr - x_rptr;
			n_wrap_fir = fir->fir_wp - fir->fir_delay + 1;
			n_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_min) ? m : n_min;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				*fir->fir_wp = Q_SHIFT_LEFT(*x_rptr, 15, 31);
				fir->fir_wp--;
				x_rptr++;
			}
			/* Check for wrap */
			src_dec_wrap(&fir->fir_wp, fir_delay, fir_size);
			src_inc_wrap_s16(&x_rptr, x_end_addr, s->x_size);
		}

		/* Filter */
		cp = cfg->coefs; /* Reset to 1st coefficient */
		rp = fir->fir_wp + rewind;
		src_inc_wrap(&rp, fir_end, fir_size);
		wp = fir->out_rp;
		for (i = 0; i < cfg->num_of_subfilters; i++) {
			fir_filter_generic(rp, cp, wp,
					   fir_delay, fir_end, fir_length,
					   taps_x_nch, cfg->shift, nch);
			wp += nch_x_odm;
			cp += subfilter_size;
			src_inc_wrap(&wp, out_delay_end, out_size);
			rp -= nch_x_idm; /* Next sub-filter start */
			src_dec_wrap(&rp, fir_delay, fir_size);
		}

		/* Output, use fixed shift by 16 */
		m = blk_out_words;
		while (m > 0) {
			n_wrap_fir = out_delay_end - fir->out_rp;
			n_wrap_buf = y_end_addr - y_wptr;
			n_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_min) ? m : n_min;
			m -= n_min;
			for (i = 0; i < n_min; i++) {
				*y_wptr = Q_SHIFT_RND(*fir->out_rp, 31, 15);
				y_wptr++;
				fir->out_rp++;
			}
			/* Check wrap */
			src_inc_wrap_s16(&y_wptr, y_end_addr, s->y_size);
			src_inc_wrap(&fir->out_rp, out_delay_end, out_size);
		}
	}
	s->x_rptr = x_rptr;
	s->y_wptr = y_wptr;
}

#endif
