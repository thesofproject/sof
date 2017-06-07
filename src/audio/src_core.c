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

/* Non optimised default C implementation guaranteed to work on any
 * architecture.
 */

#include <stdint.h>

#ifdef MODULE_TEST
#include <stdio.h>
#endif

#include <reef/alloc.h>
#include <reef/audio/format.h>
#include <reef/math/numbers.h>
#include "src_core.h"

/* Include conversion tables */
#include <reef/audio/coefficients/src/src_int24_table.h>

#if 1
/* TODO: These should be defined somewhere else. */
#define SOF_RATES_LENGTH 15
int sof_rates[SOF_RATES_LENGTH] = {7350, 8000, 11025, 12000, 16000, 22050,
	24000, 32000, 44100, 48000, 64000, 88200, 96000, 176400, 192000};
#endif

/* Calculates the needed FIR delay line length */
int src_fir_delay_length(struct src_stage *s)
{
	return s->subfilter_length + (s->num_of_subfilters - 1) * s->idm
		+ s->blk_in;
}

/* Calculates the FIR output delay line length */
int src_out_delay_length(struct src_stage *s)
{

	return(s->num_of_subfilters - 1) * s->odm + 1;
}

/* Calculates the buffer length needed between two SRC stages */
int src_stage_buf_length(struct src_stage *s1, struct src_stage *s2)
{
	int k, s1_times;

	if ((s1->blk_out == 0) || (s2->blk_in == 0))
		return 0;

	k = gcd(s1->blk_out, s2->blk_in);
	s1_times = s2->blk_in / k;

	return s1->blk_out * s1_times;
}

/* Returns index of a matching sample rate */
int src_find_fs(int fs_list[], int list_length, int fs)
{
	int i;
	for (i = 0; i < list_length; i++) {
		if (fs_list[i] == fs)
			return i;
	}
	return -EINVAL;
}

/* Match SOF and defined SRC input rates into a bit mask */
int32_t src_input_rates()
{
	int n, b;
	int mask = 0;
	for (n = SOF_RATES_LENGTH - 1; n >= 0; n--) {
		b = (src_find_fs(src_in_fs, NUM_IN_FS, sof_rates[n]) >= 0)
			? 1 : 0;
		mask = (mask << 1) | b;
	}
	return mask;
}

/* Match SOF and defined SRC output rates into a bit mask */
int32_t src_output_rates()
{
	int n, b;
	int mask = 0;
	for (n = SOF_RATES_LENGTH - 1; n >= 0; n--) {
		b = (src_find_fs(src_out_fs, NUM_OUT_FS, sof_rates[n]) >= 0)
			? 1 : 0;
		mask = (mask << 1) | b;
	}
	return mask;
}

/* Calculates buffers to allocate for a SRC mode */
int src_buffer_lengths(struct src_alloc *a, int fs_in, int fs_out, int nch)
{
	int idx_in, idx_out;
	struct src_stage *stage1, *stage2;

	idx_in = src_find_fs(src_in_fs, NUM_IN_FS, fs_in);
	idx_out = src_find_fs(src_out_fs, NUM_OUT_FS, fs_out);

	/* Return an error if no in and out match was found. */
	if ((idx_in < 0) || (idx_out < 0))
		return -EINVAL;

	stage1 = src_table1[idx_out][idx_in];
	stage2 = src_table2[idx_out][idx_in];
	a->fir_s1 = src_fir_delay_length(stage1);
	a->out_s1 = src_out_delay_length(stage1);

	if (stage2->filter_length == 1) {
		a->fir_s2 = 0;
		a->out_s2 = 0;
		a->scratch = 0;
	} else {
		a->fir_s2 = src_fir_delay_length(stage2);
		a->out_s2 = src_out_delay_length(stage2);
		a->scratch = src_stage_buf_length(stage1, stage2);
	}
	a->single_src = a->fir_s1 + a->fir_s2 + a->out_s1 + a->out_s2;
	a->total = a->scratch + nch * a->single_src;

	return 0;
}

static void src_state_reset(struct src_state* state)
{

	state->fir_delay_size = 0;
	state->out_delay_size = 0;
	state->fir_wi = 0;
	state->fir_ri = 0;
	state->out_wi = 0;
	state->out_ri = 0;
}

static int init_stages(
	struct src_stage *stage1, struct src_stage *stage2,
	struct polyphase_src *src, int n, int32_t *delay_lines_start)
{
	int k;

	/* Clear FIR state */
	src_state_reset(&src->state1);
	src_state_reset(&src->state2);

	src->number_of_stages = n;
	src->stage1 = stage1;
	src->stage2 = stage2;
	if (n == 1) {
		src->blk_in = stage1->blk_in;
		src->blk_out = stage1->blk_out;
		src->stage1_times = 1;
		src->stage2_times = 0;
		if (stage1->blk_out == 0)
			return -EINVAL;
	} else {
		if ((stage1->blk_out == 0) || (stage1->blk_in == 0))
			return -EINVAL;

		k = gcd(stage1->blk_out, stage2->blk_in);
		src->stage1_times = stage2->blk_in / k;
		src->stage2_times =
			src->stage1_times * stage1->blk_out / stage2->blk_in;
		src->blk_in = src->stage1_times * stage1->blk_in;
		src->blk_out = src->stage2_times * stage2->blk_out;
	}

	/* Delay line sizes */
	src->state1.fir_delay_size = src_fir_delay_length(stage1);
	src->state1.out_delay_size = src_out_delay_length(stage1);
	src->state1.fir_delay = delay_lines_start;
	src->state1.out_delay =
		src->state1.fir_delay + src->state1.fir_delay_size;
	if (n > 1) {
		src->state2.fir_delay_size = src_fir_delay_length(stage2);
		src->state2.out_delay_size = src_out_delay_length(stage2);
		src->state2.fir_delay =
			src->state1.out_delay + src->state1.out_delay_size;
		src->state2.out_delay =
			src->state2.fir_delay + src->state2.fir_delay_size;
	} else {
		src->state2.fir_delay_size = 0;
		src->state2.out_delay_size = 0;
		src->state2.fir_delay = NULL;
		src->state2.out_delay = NULL;
	}

	/* Check the sizes are less than MAX */
	if ((src->state1.fir_delay_size > MAX_FIR_DELAY_SIZE)
		|| (src->state1.out_delay_size > MAX_OUT_DELAY_SIZE)
		|| (src->state2.fir_delay_size > MAX_FIR_DELAY_SIZE)
		|| (src->state2.out_delay_size > MAX_OUT_DELAY_SIZE)) {
		src->state1.fir_delay = NULL;
		src->state1.out_delay = NULL;
		src->state2.fir_delay = NULL;
		src->state2.out_delay = NULL;
		return -EINVAL;
	}

	return 0;

}

void src_polyphase_reset(struct polyphase_src *src)
{

	src->mute = 1;
	src->number_of_stages = 0;
	src->blk_in = 0;
	src->blk_out = 0;
	src->stage1_times = 0;
	src->stage2_times = 0;
	src->stage1 = NULL;
	src->stage2 = NULL;
	src_state_reset(&src->state1);
	src_state_reset(&src->state2);
}

int src_polyphase_init(struct polyphase_src *src, int fs1, int fs2,
	int32_t *delay_lines_start)
{
	int f, idx_in, idx_out, n_stages, ret;
	struct src_stage *stage1, *stage2;

	idx_in = src_find_fs(src_in_fs, NUM_IN_FS, fs1);
	idx_out = src_find_fs(src_out_fs, NUM_OUT_FS, fs2);

	if ((idx_in < 0) || (idx_out < 0)) {
		ret = -EINVAL;
	} else {
		/* Get setup for 2 stage conversion */
		stage1 = src_table1[idx_out][idx_in];
		stage2 = src_table2[idx_out][idx_in];
		ret = init_stages(stage1, stage2, src, 2, delay_lines_start);

		/* Get number of stages used for optimize opportunity. 2nd
		 * stage lenth is one if conversion needs only one stage.
		 */
		n_stages = (src->stage2->filter_length == 1) ? 1 : 2;

		/* If filter length for first stage is zero this is a deleted
		 * mode from in/out matrix. Computing of such SRC mode needs
		 * to be prevented.
		 */
		if (src->stage2->filter_length == 0)
			ret = -EINVAL;
	}

	if (ret < 0) {
		/* Set blk_in, blk_out so that the muted fallback SRC keeps
		 * just source & sink in sync in pipeline without drift.
		 */
		f = gcd(fs1, fs2);
		src->blk_in = fs1 / f;
		src->blk_out = fs2 / f;

		return -EINVAL;
	}

	return n_stages;
}

/* Calculate a FIR filter part that does not need circular modification */
static inline void fir_part(int64_t *y, int ntaps, const int32_t c[], int *ic,
	int32_t d[], int *id)
{
	int64_t p;
	int n;

	/* Data is Q8.24, coef is Q1.23, product is Q9.47 */
	for (n = 0; n < ntaps; n++) {

		p = (int64_t) c[(*ic)++] * d[(*id)--];
		*y += p;
	}
}

static inline int32_t fir_filter(
	struct src_state *fir, const int32_t coefs[],
	int *coefi, int filter_length, int shift)
{
	int64_t y = 0;
	int n1;
	int n2;

	n1 = fir->fir_ri + 1;
	if (n1 > filter_length) {
		/* No need to un-wrap fir read index, make sure fir_fi
		 * is ge 0 after FIR computation */
		fir_part(&y, filter_length, coefs, coefi, fir->fir_delay,
			&fir->fir_ri);
	} else {
		n2 = filter_length - n1;
		/* Part 1, loop n1 times, fir_ri becomes -1 */
		fir_part(&y, n1, coefs, coefi, fir->fir_delay, &fir->fir_ri);

		/* Part 2, unwrap fir_ri, continue rest of filter */
		fir->fir_ri = fir->fir_delay_size - 1;
		fir_part(&y, n2, coefs, coefi, fir->fir_delay, &fir->fir_ri);
	}
	/* Q9.47 -> Q9.24, saturate to Q8.24 */
	y = y >> (23 + shift);

	return(int32_t) sat_int32(y);
}

void src_polyphase_stage_cir(struct src_stage_prm *s)
{
	int n;
	int m;
	int f;
	int c;
	int r;
	int32_t z;
	int n_wrap_fir;
	int n_wrap_buf;
	int n_wrap_min;

	for (n = 0; n < s->times; n++) {
		/* Input data */
		m = s->x_inc * s->stage->blk_in;
		while (m > 0) {
			n_wrap_fir =
				(s->state->fir_delay_size - s->state->fir_wi)
				* s->x_inc;
			n_wrap_buf = s->x_end_addr - s->x_rptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			if (m < n_wrap_min) {
				/* No circular wrap need */
				while (m > 0) {
					s->state->fir_delay[s->state->fir_wi++]
						= *s->x_rptr;
					s->x_rptr += s->x_inc;
					m -= s->x_inc;
				}
			} else {
				/* Wrap in n_wrap_min/x_inc samples */
				while (n_wrap_min > 0) {
					s->state->fir_delay[s->state->fir_wi++]
						= *s->x_rptr;
					s->x_rptr += s->x_inc;
					n_wrap_min -= s->x_inc;
					m -= s->x_inc;
				}
				/* Check both */
				if (s->x_rptr >= s->x_end_addr)
					s->x_rptr = (int32_t *)
					((size_t) s->x_rptr - s->x_size);
				if (s->state->fir_wi == s->state->fir_delay_size)
					s->state->fir_wi = 0;
			}
		}

		/* Filter */
		c = 0;
		r = s->state->fir_wi - s->stage->blk_in
			- (s->stage->num_of_subfilters - 1) * s->stage->idm;
		if (r < 0)
			r += s->state->fir_delay_size;

		s->state->out_wi = s->state->out_ri;
		for (f = 0; f < s->stage->num_of_subfilters; f++) {
			s->state->fir_ri = r;
			z = fir_filter(s->state, s->stage->coefs, &c,
				s->stage->subfilter_length, s->stage->shift);
			r += s->stage->idm;
			if (r > s->state->fir_delay_size - 1)
				r -= s->state->fir_delay_size;

			s->state->out_delay[s->state->out_wi] = z;
			s->state->out_wi += s->stage->odm;
			if (s->state->out_wi > s->state->out_delay_size - 1)
				s->state->out_wi -= s->state->out_delay_size;
		}

		/* Output */
		m = s->y_inc * s->stage->num_of_subfilters;
		while (m > 0) {
			n_wrap_fir =
				(s->state->out_delay_size - s->state->out_ri)
				* s->y_inc;
			n_wrap_buf = s->y_end_addr - s->y_wptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			if (m < n_wrap_min) {
				/* No circular wrap need */
				while (m > 0) {
					*s->y_wptr = s->state->out_delay[s->state->out_ri++];
					s->y_wptr += s->y_inc;
					m -= s->y_inc;
				}
			} else {
				/* Wrap in n_wrap_min/y_inc samples */
				while (n_wrap_min > 0) {
					*s->y_wptr = s->state->out_delay[s->state->out_ri++];
					s->y_wptr += s->y_inc;
					n_wrap_min -= s->y_inc;
					m -= s->y_inc;
				}
				/* Check both */
				if (s->y_wptr >= s->y_end_addr)
					s->y_wptr =
					(int32_t *)
					((size_t) s->y_wptr - s->y_size);

				if (s->state->out_ri == s->state->out_delay_size)
					s->state->out_ri = 0;
			}
		}
	}
}


#ifdef MODULE_TEST

void src_print_info(struct polyphase_src *src)
{

	int n1;
	int n2;

	n1 = src->stage1->filter_length;
	n2 = src->stage2->filter_length;
	printf("SRC stages %d\n", src->number_of_stages);
	printf("SRC input blk %d\n", src->blk_in);
	printf("SRC output blk %d\n", src->blk_out);
	printf("SRC stage1 %d times\n", src->stage1_times);
	printf("SRC stage2 %d times\n", src->stage2_times);

	printf("SRC1 filter length %d\n", n1);
	printf("SRC1 subfilter length %d\n", src->stage1->subfilter_length);
	printf("SRC1 number of subfilters %d\n", src->stage1->num_of_subfilters);
	printf("SRC1 idm %d\n", src->stage1->idm);
	printf("SRC1 odm %d\n", src->stage1->odm);
	printf("SRC1 input blk %d\n", src->stage1->blk_in);
	printf("SRC1 output blk %d\n", src->stage1->blk_out);
	printf("SRC1 halfband %d\n", src->stage1->halfband);
	printf("SRC1 coef[0] %d\n", src->stage1->coefs[0]);
	if (n1 > 3) {
		printf("SRC1 coef[1] %d\n", src->stage1->coefs[1]);
		printf("SRC1 coef[2] %d\n", src->stage1->coefs[2]);
		printf("SRC1 coef[%d] %d\n", n1 - 1, src->stage1->coefs[n1 - 1]);
	}
	printf("SRC1 FIR delay %d\n", src->state1.fir_delay_size);
	printf("SRC1 out delay %d\n", src->state1.out_delay_size);

	printf("SRC2 filter length %d\n", n2);
	printf("SRC2 subfilter length %d\n", src->stage2->subfilter_length);
	printf("SRC2 number of subfilters %d\n", src->stage2->num_of_subfilters);
	printf("SRC2 idm %d\n", src->stage2->idm);
	printf("SRC2 odm %d\n", src->stage2->odm);
	printf("SRC2 input blk %d\n", src->stage2->blk_in);
	printf("SRC2 output blk %d\n", src->stage2->blk_out);
	printf("SRC2 halfband %d\n", src->stage2->halfband);
	printf("SRC2 coef[0] %d\n", src->stage2->coefs[0]);
	if (n2 > 3) {
		printf("SRC2 coef[1] %d\n", src->stage2->coefs[1]);
		printf("SRC2 coef[2] %d\n", src->stage2->coefs[2]);
		printf("SRC2 coef[%d] %d\n", n1 - 1, src->stage2->coefs[n2 - 1]);
	}
	printf("SRC2 FIR delay %d\n", src->state2.fir_delay_size);
	printf("SRC2 out delay %d\n", src->state2.out_delay_size);
}
#endif
