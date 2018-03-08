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

/* Non optimized default C implementation guaranteed to work on any
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

#define trace_src(__e) trace_event(TRACE_CLASS_SRC, __e)
#define tracev_src(__e) tracev_event(TRACE_CLASS_SRC, __e)
#define trace_src_error(__e) trace_error(TRACE_CLASS_SRC, __e)

/* TODO: These should be defined somewhere else. */
#define SOF_RATES_LENGTH 15
int sof_rates[SOF_RATES_LENGTH] = {8000, 11025, 12000, 16000, 18900,
	22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 176400,
	192000};

/* The FIR maximum lengths are per channel so need to multiply them */
#define MAX_FIR_DELAY_SIZE_XNCH (PLATFORM_MAX_CHANNELS * MAX_FIR_DELAY_SIZE)
#define MAX_OUT_DELAY_SIZE_XNCH (PLATFORM_MAX_CHANNELS * MAX_OUT_DELAY_SIZE)

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
	int q;
	int den;
	int num;
	int frames2;

	if (nch > PLATFORM_MAX_CHANNELS) {
		trace_src_error("che");
		tracev_value(nch);
		return -EINVAL;
	}

	a->nch = nch;
	a->idx_in = src_find_fs(src_in_fs, NUM_IN_FS, fs_in);
	a->idx_out = src_find_fs(src_out_fs, NUM_OUT_FS, fs_out);

	/* Check that both in and out rates are supported */
	if ((a->idx_in < 0) || (a->idx_out < 0)) {
		trace_src_error("us1");
		tracev_value(fs_in);
		tracev_value(fs_out);
		return -EINVAL;
	}

	stage1 = src_table1[a->idx_out][a->idx_in];
	stage2 = src_table2[a->idx_out][a->idx_in];

	/* Check from stage1 parameter for a deleted in/out rate combination.*/
	if (stage1->filter_length < 1) {
		trace_src_error("us2");
		tracev_value(fs_in);
		tracev_value(fs_out);
		return -EINVAL;
	}

	a->fir_s1 = nch * src_fir_delay_length(stage1);
	a->out_s1 = nch * src_out_delay_length(stage1);

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
		a->fir_s2 = nch * src_fir_delay_length(stage2);
		a->out_s2 = nch * src_out_delay_length(stage2);
		/* 2x is an empirically tested length. Since the sink buffer
		 * capability to receive samples varies a shorter stage 2 output
		 * block will create a peak in internal buffer usage.
		 */
		a->sbuf_length = 2 * nch * stage1->blk_out * a->stage1_times_max;
	}

	a->src_multich = a->fir_s1 + a->fir_s2 + a->out_s1 + a->out_s2;
	a->total = a->sbuf_length + a->src_multich;

	return 0;
}

static void src_state_reset(struct src_state *state)
{

	state->fir_delay_size = 0;
	state->out_delay_size = 0;
	state->fir_wi = 0;
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
	if ((n == 1) && (stage1->blk_out == 0))
		return -EINVAL;

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
	if ((src->state1.fir_delay_size > MAX_FIR_DELAY_SIZE_XNCH)
		|| (src->state1.out_delay_size > MAX_OUT_DELAY_SIZE_XNCH)
		|| (src->state2.fir_delay_size > MAX_FIR_DELAY_SIZE_XNCH)
		|| (src->state2.out_delay_size > MAX_OUT_DELAY_SIZE_XNCH)) {
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

	src->number_of_stages = 0;
	src->stage1 = NULL;
	src->stage2 = NULL;
	src_state_reset(&src->state1);
	src_state_reset(&src->state2);
}

int src_polyphase_init(struct polyphase_src *src, struct src_param *p,
	int32_t *delay_lines_start)
{
	struct src_stage *stage1;
	struct src_stage *stage2;
	int n_stages;
	int ret;

	if ((p->idx_in < 0) || (p->idx_out < 0)) {
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

static inline void fir_part(int64_t y[], int *id, int *ic,
	const int32_t data[], const int16_t coef[], int nch_x_taps, int nch)
{
	int64_t tap0;
	int64_t tap1;
	int n;
	int64_t a = 0;
	int64_t b = 0;
	int c = *ic;
	int d = *id;
	int d_end = d - nch_x_taps;

	/* Data is Q1.31, coef is Q1.15, product is Q2.46 */
	if (nch == 2) {
		for (n = 0; n < (nch_x_taps >> 2); n++) {
			tap0 = coef[c++];
			tap1 = coef[c++];
			b += data[d--] * tap0;
			a += data[d--] * tap0;
			b += data[d--] * tap1;
			a += data[d--] * tap1;
		}
		if (d > d_end) {
			tap0 = coef[c++];
			b += data[d--] * tap0;
			a += data[d--] * tap0;
		}
		y[1] += b;
		y[0] += a;
	} else {
		while (d > d_end) {
			tap0 = coef[c++];
			for (n = nch - 1; n >= 0; n--)
				y[n] += data[d--] * tap0;
		}
	}
	*ic = c;
	*id = d;
}

#else

static inline void fir_part(int64_t y[], int *id, int *ic,
	const int32_t data[], const int32_t coef[], int nch_x_taps, int nch)
{
	int64_t tap0;
	int64_t tap1;
	int n;
	int64_t a = 0;
	int64_t b = 0;
	int c = *ic;
	int d = *id;
	int d_end = d - nch_x_taps;

	/* Data is Q1.31, coef is Q1.23, product is Q2.54 */
	if (nch == 2) {
		for (n = 0; n < (nch_x_taps >> 2); n++) {
			tap0 = coef[c++];
			tap1 = coef[c++];
			b += data[d--] * tap0;
			a += data[d--] * tap0;
			b += data[d--] * tap1;
			a += data[d--] * tap1;
		}
		if (d > d_end) {
			tap0 = coef[c++];
			b += data[d--] * tap0;
			a += data[d--] * tap0;
		}
		y[1] += b;
		y[0] += a;
	} else {
		while (d > d_end) {
			tap0 = coef[c++];
			for (n = nch - 1; n >= 0; n--)
				y[n] += data[d--] * tap0;
		}
	}
	*ic = c;
	*id = d;
}

#endif

#if SRC_SHORT == 1

static void fir_filter(int ri0, int *ci, int wi0, int32_t in_delay[],
	int32_t out_delay[], const int16_t coefs[], int dsm1, int taps,
	int shift, int nch)
{
	int n2;
	int i;
	int64_t y[PLATFORM_MAX_CHANNELS];
	int ri = ri0;
	int wi = wi0;
	int n1 = ri0 + 1; /* Convert to number of sequential frames */
	int qshift = 15 + shift; /* Q2.46 -> Q2.31 */
	int32_t rnd = 1 << (qshift - 1); /* Half LSB */
	int nch_x_taps = nch * taps;

	/* Initialize to half LSB for rounding */
	for (i = 0; i < nch; i++)
		y[i] = rnd;

	if (n1 >= nch_x_taps) {
		fir_part(y, &ri, ci, in_delay, coefs, nch_x_taps, nch);
	} else {
		n2 = nch_x_taps - n1;
		fir_part(y, &ri, ci, in_delay, coefs, n1, nch);
		ri = dsm1;
		fir_part(y, &ri, ci, in_delay, coefs, n2, nch);
	}

	for (i = 0; i < nch; i++)
		out_delay[wi++] = sat_int32(y[i] >> qshift);
}
#else

static void fir_filter(int ri0, int *ci, int wi0, int32_t in_delay[],
	int32_t out_delay[], const int32_t coefs[], int dsm1, int taps,
	int shift, int nch)
{
	int n2;
	int i;
	int64_t y[PLATFORM_MAX_CHANNELS];
	int ri = ri0;
	int wi = wi0;
	int n1 = ri0 + 1; /* Convert to number of sequential frames */
	int qshift = 23 + shift; /* Q2.54 -> Q2.31 */
	int32_t rnd = 1 << (qshift - 1); /* Half LSB */
	int nch_x_taps = nch * taps;

	/* Initialize to half LSB for rounding */
	for (i = 0; i < nch; i++)
		y[i] = rnd;

	if (n1 >= nch_x_taps) {
		fir_part(y, &ri, ci, in_delay, coefs, nch_x_taps, nch);
	} else {
		n2 = nch_x_taps - n1;
		fir_part(y, &ri, ci, in_delay, coefs, n1, nch);
		ri = dsm1;
		fir_part(y, &ri, ci, in_delay, coefs, n2, nch);
	}

	for (i = 0; i < nch; i++)
		out_delay[wi++] = sat_int32(y[i] >> qshift);

}

#endif

void src_polyphase_stage_cir(struct src_stage_prm * s)
{
	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int n;
	int m;
	int f;
	int ci;
	int ri;
	int n_wrap_fir;
	int n_wrap_buf;
	int n_wrap_min;
	int n_min;
	int wi;
	const void *coef = cfg->coefs;
	int32_t *in_delay = fir->fir_delay;
	int32_t *out_delay = fir->out_delay;
	int dsm1 = fir->fir_delay_size - 1;
	int shift = cfg->shift;
	int nch = s->nch;
	int rewind = -nch * (cfg->blk_in
		+ (cfg->num_of_subfilters - 1) * cfg->idm) + nch - 1;
	int nch_x_idm = cfg->idm * nch;
	int nch_x_odm = cfg->odm * nch;
	size_t sz = sizeof(int32_t);
	int blk_in_bytes = nch * cfg->blk_in * sz;
	int blk_out_bytes = nch * cfg->num_of_subfilters * sz;


	for (n = 0; n < s->times; n++) {
		/* Input data */
		m = blk_in_bytes;
		while (m > 0) {
			n_wrap_fir = (fir->fir_delay_size - fir->fir_wi) * sz;
			n_wrap_buf = s->x_end_addr - s->x_rptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_wrap_min) ? m : n_wrap_min;
			while (n_min > 0) {
				fir->fir_delay[fir->fir_wi++] = *s->x_rptr;
				s->x_rptr++;
				n_min -= sz;
				m -= sz;
			}
			/* Check for wrap */
			src_circ_inc_wrap(&s->x_rptr, s->x_end_addr, s->x_size);
			if (fir->fir_wi == fir->fir_delay_size)
				fir->fir_wi = 0;
		}

		/* Filter */
		ci = 0; /* Reset to 1st coefficient */
		ri = fir->fir_wi + rewind; /* Newest data for last subfilter */
		if (ri < 0)
			ri += fir->fir_delay_size;

		wi = fir->out_ri;
		for (f = 0; f < cfg->num_of_subfilters; f++) {
			fir_filter(ri, &ci, wi, in_delay, out_delay, coef,
				dsm1, cfg->subfilter_length, shift, nch);

			wi += nch_x_odm;
			if (wi >= fir->out_delay_size)
				wi -= fir->out_delay_size;

			ri += nch_x_idm; /* Next sub-filter start */
			if (ri >= fir->fir_delay_size)
				ri -= fir->fir_delay_size;
		}

		/* Output */
		m = blk_out_bytes;
		while (m > 0) {
			n_wrap_fir = (fir->out_delay_size - fir->out_ri) * sz;
			n_wrap_buf = s->y_end_addr - s->y_wptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_wrap_min) ? m : n_wrap_min;
			while (n_min > 0) {
				*s->y_wptr = fir->out_delay[fir->out_ri++];
				s->y_wptr++;
				n_min -= sz;
				m -= sz;
			}
			/* Check wrap */
			src_circ_inc_wrap(&s->y_wptr, s->y_end_addr, s->y_size);
			if (fir->out_ri == fir->out_delay_size)
				fir->out_ri = 0;
		}
	}
}

void src_polyphase_stage_cir_s24(struct src_stage_prm *s)
{
	struct src_state *fir = s->state;
	struct src_stage *cfg = s->stage;
	int n;
	int m;
	int f;
	int ci;
	int ri;
	int n_wrap_fir;
	int n_wrap_buf;
	int n_wrap_min;
	int n_min;
	int wi;
	const void *coef = cfg->coefs;
	int32_t *in_delay = fir->fir_delay;
	int32_t *out_delay = fir->out_delay;
	int dsm1 = fir->fir_delay_size - 1;
	int shift = cfg->shift;
	int nch = s->nch;
	int rewind = -nch * (cfg->blk_in
		+ (cfg->num_of_subfilters - 1) * cfg->idm) + nch - 1;
	int nch_x_idm = cfg->idm * nch;
	int nch_x_odm = cfg->odm * nch;
	size_t sz = sizeof(int32_t);
	int blk_in_bytes = nch * cfg->blk_in * sz;
	int blk_out_bytes = nch * cfg->num_of_subfilters * sz;

	for (n = 0; n < s->times; n++) {
		/* Input data */
		m = blk_in_bytes;
		while (m > 0) {
			n_wrap_fir = (fir->fir_delay_size - fir->fir_wi) * sz;
			n_wrap_buf = s->x_end_addr - s->x_rptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_wrap_min) ? m : n_wrap_min;
			while (n_min > 0) {
				fir->fir_delay[fir->fir_wi++] = *s->x_rptr << 8;
				s->x_rptr++;
				n_min -= sz;
				m -= sz;
			}
			/* Check for wrap */
			src_circ_inc_wrap(&s->x_rptr, s->x_end_addr, s->x_size);
			if (fir->fir_wi == fir->fir_delay_size)
				fir->fir_wi = 0;
		}

		/* Filter */
		ci = 0; /* Reset to 1st coefficient */
		ri = fir->fir_wi + rewind; /* Newest data for last subfilter */
		if (ri < 0)
			ri += fir->fir_delay_size;

		wi = fir->out_ri;
		for (f = 0; f < cfg->num_of_subfilters; f++) {
			fir_filter(ri, &ci, wi, in_delay, out_delay, coef,
				dsm1, cfg->subfilter_length, shift, nch);

			wi += nch_x_odm;
			if (wi >= fir->out_delay_size)
				wi -= fir->out_delay_size;

			ri += nch_x_idm; /* Next sub-filter start */
			if (ri >= fir->fir_delay_size)
				ri -= fir->fir_delay_size;
		}

		/* Output */
		m = blk_out_bytes;
		while (m > 0) {
			n_wrap_fir = (fir->out_delay_size - fir->out_ri) * sz;
			n_wrap_buf = s->y_end_addr - s->y_wptr;
			n_wrap_min = (n_wrap_fir < n_wrap_buf)
				? n_wrap_fir : n_wrap_buf;
			n_min = (m < n_wrap_min) ? m : n_wrap_min;
			while (n_min > 0) {
				*s->y_wptr = fir->out_delay[fir->out_ri++] >> 8;
				s->y_wptr++;
				n_min -= sz;
				m -= sz;
			}
			/* Check wrap */
			src_circ_inc_wrap(&s->y_wptr, s->y_end_addr, s->y_size);
			if (fir->out_ri == fir->out_delay_size)
				fir->out_ri = 0;
		}
	}

}
