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
#include "src_config.h"

/* TODO: These should be defined somewhere else. */
#define SOF_RATES_LENGTH 15
int sof_rates[SOF_RATES_LENGTH] = {8000, 11025, 12000, 16000, 18900,
	22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 176400,
	192000};

/* Calculate ceil() for integer division */
int src_ceil_divide(int a, int b)
{
	int c;

	c = a / b;
	if (c * b < a)
		c++;

	return c;
}

/* Calculates the needed FIR delay line length */
static int src_fir_delay_length(struct src_stage *s)
{
	return s->subfilter_length + (s->num_of_subfilters - 1) * s->idm
		+ s->blk_in;
}

/* Calculates the FIR output delay line length */
static int src_out_delay_length(struct src_stage *s)
{

	return 1 + (s->num_of_subfilters - 1) * s->odm;
}

/* Returns index of a matching sample rate */
static int src_find_fs(int fs_list[], int list_length, int fs)
{
	int i;

	for (i = 0; i < list_length; i++) {
		if (fs_list[i] == fs)
			return i;
	}
	return -EINVAL;
}

/* Match SOF and defined SRC input rates into a bit mask */
int32_t src_input_rates(void)
{
	int n;
	int b;
	int mask = 0;

	for (n = SOF_RATES_LENGTH - 1; n >= 0; n--) {
		b = (src_find_fs(src_in_fs, NUM_IN_FS, sof_rates[n]) >= 0)
			? 1 : 0;
		mask = (mask << 1) | b;
	}
	return mask;
}

/* Match SOF and defined SRC output rates into a bit mask */
int32_t src_output_rates(void)
{
	int n;
	int b;
	int mask = 0;

	for (n = SOF_RATES_LENGTH - 1; n >= 0; n--) {
		b = (src_find_fs(src_out_fs, NUM_OUT_FS, sof_rates[n]) >= 0)
			? 1 : 0;
		mask = (mask << 1) | b;
	}
	return mask;
}

/* Calculates buffers to allocate for a SRC mode */
int src_buffer_lengths(struct src_param *a, int fs_in, int fs_out, int nch,
	int frames, int frames_is_for_source)
{
	struct src_stage *stage1;
	struct src_stage *stage2;
	int k;
	int q;
	int den;
	int num;
	int frames2;

	a->idx_in = src_find_fs(src_in_fs, NUM_IN_FS, fs_in);
	a->idx_out = src_find_fs(src_out_fs, NUM_OUT_FS, fs_out);

	/* Set blk_in, blk_out so that the muted fallback SRC keeps
	 * just source & sink in sync in pipeline without drift.
	 */
	if ((a->idx_in < 0) || (a->idx_out < 0)) {
		k = gcd(fs_in, fs_out);
		a->blk_in = fs_in / k;
		a->blk_out = fs_out / k;
		return -EINVAL;
	}

	stage1 = src_table1[a->idx_out][a->idx_in];
	stage2 = src_table2[a->idx_out][a->idx_in];
	a->fir_s1 = src_fir_delay_length(stage1);
	a->out_s1 = src_out_delay_length(stage1);

	/* Find out how many additional times the SRC can be executed
	   while having block size less or equal to max_frames.
	 */
	if (frames_is_for_source) {
		/* Times that stage1 needs to run to input length of frames */
		a->stage1_times_max = src_ceil_divide(frames, stage1->blk_in);
		q = frames / stage1->blk_in;
		a->stage1_times = MAX(q, 1);
		a->blk_in = a->stage1_times * stage1->blk_in;

		/* Times that stage2 needs to run */
		den = stage2->blk_in * stage1->blk_in;
		num = frames * stage2->blk_out * stage1->blk_out;
		frames2 = src_ceil_divide(num, den);
		a->stage2_times_max = src_ceil_divide(frames2, stage2->blk_out);
		q = frames2 / stage2->blk_out;
		a->stage2_times = MAX(q, 1);
		a->blk_out = a->stage2_times * stage2->blk_out;
	} else {
		/* Times that stage2 needs to run to output length of frames */
		a->stage2_times_max = src_ceil_divide(frames, stage2->blk_out);
		q = frames / stage2->blk_out;
		a->stage2_times = MAX(q, 1);
		a->blk_out = a->stage2_times * stage2->blk_out;

		/* Times that stage1 needs to run */
		num = frames * stage2->blk_in * stage1->blk_in;
		den = stage2->blk_out * stage1->blk_out;
		frames2 = src_ceil_divide(num, den);
		a->stage1_times_max = src_ceil_divide(frames2, stage1->blk_in);
		q = frames2 / stage1->blk_in;
		a->stage1_times = MAX(q, 1);
		a->blk_in = a->stage1_times * stage1->blk_in;
	}

	if (stage2->filter_length == 1) {
		a->fir_s2 = 0;
		a->out_s2 = 0;
		a->stage2_times = 0;
		a->stage2_times_max = 0;
		a->sbuf_length = 0;
	} else {
		a->fir_s2 = src_fir_delay_length(stage2);
		a->out_s2 = src_out_delay_length(stage2);
		/* 2x is an empirically tested length. Since the sink buffer
		 * capability to receive samples varies a shorter stage 2 output
		 * block will create a peak in internal buffer usage.
		 */
		a->sbuf_length = 2 * nch * stage1->blk_out * a->stage1_times_max;
	}

	a->single_src = a->fir_s1 + a->fir_s2 + a->out_s1 + a->out_s2;
	a->total = a->sbuf_length + nch * a->single_src;

	return 0;
}

static void src_state_reset(struct src_state *state)
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
	struct polyphase_src *src, struct src_param *p,
	int n, int32_t *delay_lines_start)
{
	/* Clear FIR state */
	src_state_reset(&src->state1);
	src_state_reset(&src->state2);

	src->number_of_stages = n;
	src->stage1 = stage1;
	src->stage2 = stage2;
	src->blk_in = p->blk_in;
	src->blk_out = p->blk_out;
	if (n == 1) {
		src->stage1_times = p->stage1_times;
		src->stage2_times = 0;
		if (stage1->blk_out == 0)
			return -EINVAL;
	} else {
		src->stage1_times = p->stage1_times;
		src->stage2_times = p->stage2_times;
	}

	/* Delay line sizes */
	src->state1.fir_delay_size = p->fir_s1;
	src->state1.out_delay_size = p->out_s1;
	src->state1.fir_delay = delay_lines_start;
	src->state1.out_delay =
		src->state1.fir_delay + src->state1.fir_delay_size;
	if (n > 1) {
		src->state2.fir_delay_size = p->fir_s2;
		src->state2.out_delay_size = p->out_s2;
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

	src->mute = 0;
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

int src_polyphase_init(struct polyphase_src *src, struct src_param *p,
	int32_t *delay_lines_start)
{
	int n_stages;
	int ret;
	struct src_stage *stage1;
	struct src_stage *stage2;

	if ((p->idx_in < 0) || (p->idx_out < 0)) {
		src->blk_in = p->blk_in;
		src->blk_out = p->blk_out;
		return -EINVAL;
	}

	/* Get setup for 2 stage conversion */
	stage1 = src_table1[p->idx_out][p->idx_in];
	stage2 = src_table2[p->idx_out][p->idx_in];
	ret = init_stages(stage1, stage2, src, p, 2, delay_lines_start);
	if (ret < 0)
		return -EINVAL;

	/* Get number of stages used for optimize opportunity. 2nd
	 * stage length is one if conversion needs only one stage.
	 * If input and output rate is the same return 0 to
	 * use a simple copy function instead of 1 stage FIR with one
	 * tap.
	 */
	n_stages = (src->stage2->filter_length == 1) ? 1 : 2;
	if (p->idx_in == p->idx_out)
		n_stages = 0;

	/* If filter length for first stage is zero this is a deleted
	 * mode from in/out matrix. Computing of such SRC mode needs
	 * to be prevented.
	 */
	if (src->stage1->filter_length == 0)
		return -EINVAL;

	return n_stages;
}

#if SRC_SHORT == 1

/* Calculate a FIR filter part that does not need circular modification */
static inline void fir_part(int64_t *y, int ntaps, const int16_t c[], int *ic,
	int32_t d[], int *id)
{
	int n;
	int64_t a = 0;

	/* Data is Q1.31, coef is Q1.15, product is Q2.46 */
	for (n = 0; n < (ntaps >> 1); n++) {
		a += (int64_t) c[*ic] * d[*id]
			+ (int64_t) c[*ic + 1] * d[*id - 1];
		*ic += 2;
		*id -= 2;
	}
	if (ntaps & 1)
		a += (int64_t) c[(*ic)++] * d[(*id)--];

	*y += a;
}
#else

/* Calculate a FIR filter part that does not need circular modification */
static inline void fir_part(int64_t *y, int ntaps, const int32_t c[], int *ic,
	int32_t d[], int *id)
{
	int n;
	int64_t a = 0;

	/* Data is Q8.24, coef is Q1.23, product is Q9.47 */
	for (n = 0; n < (ntaps >> 1); n++) {
		a += (int64_t) c[*ic] * d[*id]
			+ (int64_t) c[*ic + 1] * d[*id - 1];
		*ic += 2;
		*id -= 2;
	}
	if (ntaps & 1)
		a += (int64_t) c[(*ic)++] * d[(*id)--];

	*y += a;
}
#endif

#if SRC_SHORT == 1

static inline int32_t fir_filter(
	struct src_state *fir, const int16_t coefs[],
	int *coefi, int filter_length, int shift)
{
	int64_t y = 0;
	int n1;
	int n2;

	n1 = fir->fir_ri + 1;
	if (n1 > filter_length) {
		/* No need to un-wrap fir read index, make sure fir_fi
		 * is ge 0 after FIR computation.
		 */
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
	/* Q2.46 -> Q2.31, saturate to Q1.31 */
	y = y >> (15 + shift);

	return(int32_t) sat_int32(y);
}
#else

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
		 * is ge 0 after FIR computation.
		 */
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
#endif

void src_polyphase_stage_cir(struct src_stage_prm * s)
{
	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int n;
	int m;
	int f;
	int c;
	int r;
	int n_wrap_fir;
	int n_wrap_buf;
	int n_wrap_min;
	int n_min;
	int32_t z;

	for (n = 0; n < s->times; n++) {
		/* Input data */
		m = s->x_inc * cfg->blk_in;
		while (m > 0) {
			n_wrap_fir = (fir->fir_delay_size - fir->fir_wi)
				* s->x_inc;
			n_wrap_buf = s->x_end_addr - s->x_rptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_wrap_min) ? m : n_wrap_min;
			while (n_min > 0) {
				fir->fir_delay[fir->fir_wi++] = *s->x_rptr;
				s->x_rptr += s->x_inc;
				n_min -= s->x_inc;
				m -= s->x_inc;
			}
			/* Check for wrap */
			src_circ_inc_wrap(&s->x_rptr, s->x_end_addr, s->x_size);
			if (fir->fir_wi == fir->fir_delay_size)
				fir->fir_wi = 0;
		}

		/* Filter */
		c = 0;
		r = fir->fir_wi - cfg->blk_in
			- (cfg->num_of_subfilters - 1) * cfg->idm;
		if (r < 0)
			r += fir->fir_delay_size;

		fir->out_wi = fir->out_ri;
		for (f = 0; f < cfg->num_of_subfilters; f++) {
			fir->fir_ri = r;
			z = fir_filter(fir, cfg->coefs, &c,
				cfg->subfilter_length, cfg->shift);
			r += cfg->idm;
			if (r >= fir->fir_delay_size)
				r -= fir->fir_delay_size;

			fir->out_delay[fir->out_wi] = z;
			fir->out_wi += cfg->odm;
			if (fir->out_wi >= fir->out_delay_size)
				fir->out_wi -= fir->out_delay_size;
		}

		/* Output */
		m = s->y_inc * cfg->num_of_subfilters;
		while (m > 0) {
			n_wrap_fir = (fir->out_delay_size - fir->out_ri)
				* s->y_inc;
			n_wrap_buf = s->y_end_addr - s->y_wptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_wrap_min) ? m : n_wrap_min;
			while (n_min > 0) {
				*s->y_wptr = fir->out_delay[fir->out_ri++];
				s->y_wptr += s->y_inc;
				n_min -= s->y_inc;
				m -= s->y_inc;
			}
			/* Check wrap */
			src_circ_inc_wrap(&s->y_wptr, s->y_end_addr, s->y_size);
			if (fir->out_ri == fir->out_delay_size)
				fir->out_ri = 0;
		}
	}
}

void src_polyphase_stage_cir_s24(struct src_stage_prm * s)
{
	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int n;
	int m;
	int f;
	int c;
	int r;
	int n_wrap_fir;
	int n_wrap_buf;
	int n_wrap_min;
	int n_min;
	int32_t z;
	int32_t se;

	for (n = 0; n < s->times; n++) {
		/* Input data */
		m = s->x_inc * cfg->blk_in;
		while (m > 0) {
			n_wrap_fir = (fir->fir_delay_size - fir->fir_wi)
				* s->x_inc;
			n_wrap_buf = s->x_end_addr - s->x_rptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_wrap_min) ? m : n_wrap_min;
			while (n_min > 0) {
				se = *s->x_rptr << 8;
				fir->fir_delay[fir->fir_wi++] = se >> 8;
				s->x_rptr += s->x_inc;
				n_min -= s->x_inc;
				m -= s->x_inc;
			}
			/* Check for wrap */
			src_circ_inc_wrap(&s->x_rptr, s->x_end_addr, s->x_size);
			if (fir->fir_wi == fir->fir_delay_size)
				fir->fir_wi = 0;
		}

		/* Filter */
		c = 0;
		r = fir->fir_wi - cfg->blk_in
			- (cfg->num_of_subfilters - 1) * cfg->idm;
		if (r < 0)
			r += fir->fir_delay_size;

		fir->out_wi = fir->out_ri;
		for (f = 0; f < cfg->num_of_subfilters; f++) {
			fir->fir_ri = r;
			z = fir_filter(fir, cfg->coefs, &c,
				cfg->subfilter_length, cfg->shift);
			r += cfg->idm;
			if (r >= fir->fir_delay_size)
				r -= fir->fir_delay_size;

			fir->out_delay[fir->out_wi] = z;
			fir->out_wi += cfg->odm;
			if (fir->out_wi >= fir->out_delay_size)
				fir->out_wi -= fir->out_delay_size;
		}

		/* Output */
		m = s->y_inc * cfg->num_of_subfilters;
		while (m > 0) {
			n_wrap_fir = (fir->out_delay_size - fir->out_ri)
				* s->y_inc;
			n_wrap_buf = s->y_end_addr - s->y_wptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_wrap_min) ? m : n_wrap_min;
			while (n_min > 0) {
				*s->y_wptr = fir->out_delay[fir->out_ri++];
				s->y_wptr += s->y_inc;
				n_min -= s->y_inc;
				m -= s->y_inc;
			}
			/* Check wrap */
			src_circ_inc_wrap(&s->y_wptr, s->y_end_addr, s->y_size);
			if (fir->out_ri == fir->out_delay_size)
				fir->out_ri = 0;
		}
	}
}


#ifdef MODULE_TEST

void src_print_info(struct polyphase_src * src)
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
	printf("SRC1 number of subfilters %d\n",
		src->stage1->num_of_subfilters);
	printf("SRC1 idm %d\n", src->stage1->idm);
	printf("SRC1 odm %d\n", src->stage1->odm);
	printf("SRC1 input blk %d\n", src->stage1->blk_in);
	printf("SRC1 output blk %d\n", src->stage1->blk_out);
	printf("SRC1 halfband %d\n", src->stage1->halfband);
	printf("SRC1 FIR delay %d\n", src->state1.fir_delay_size);
	printf("SRC1 out delay %d\n", src->state1.out_delay_size);

	printf("SRC2 filter length %d\n", n2);
	printf("SRC2 subfilter length %d\n", src->stage2->subfilter_length);
	printf("SRC2 number of subfilters %d\n",
		src->stage2->num_of_subfilters);
	printf("SRC2 idm %d\n", src->stage2->idm);
	printf("SRC2 odm %d\n", src->stage2->odm);
	printf("SRC2 input blk %d\n", src->stage2->blk_in);
	printf("SRC2 output blk %d\n", src->stage2->blk_out);
	printf("SRC2 halfband %d\n", src->stage2->halfband);
	printf("SRC2 FIR delay %d\n", src->state2.fir_delay_size);
	printf("SRC2 out delay %d\n", src->state2.out_delay_size);
}
#endif
