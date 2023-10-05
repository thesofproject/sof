// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>

#ifdef MFCC_HIFI3

#include <sof/audio/component.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/auditory.h>
#include <sof/math/matrix.h>
#include <sof/math/sqrt.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>
#include <user/mfcc.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <xtensa/tie/xt_hifi3.h>

/* Setup circular for buffer 0 */
static inline void set_circular_buf0(const void *start, const void *end)
{
	AE_SETCBEGIN0(start);
	AE_SETCEND0(end);
}

/*
 * MFCC algorithm code
 */

void mfcc_source_copy_s16(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	struct audio_stream *source = bsource->data;
	int copied;
	int nmax;
	int n;
	int i;
	int num_channels = audio_stream_get_channels(source);
	ae_int16 *in;
	ae_int16 *x = (ae_int16 *)audio_stream_get_rptr(source);
	ae_int16 *out = (ae_int16 *)buf->w_ptr;
	ae_int16x4 sample;
	ae_int32x2 temp;
	ae_int16x4 coef = emph->coef;
	ae_int16x4 delay;
	const int in_inc = sizeof(ae_int16) * num_channels;

	/* Copy from source to pre-buffer for FFT.
	 * The pre-emphasis filter is done in this step.
	 */
	for (copied = 0; copied < frames; copied += n) {
		nmax = frames - copied;
		n = audio_stream_frames_without_wrap(source, x);
		n = MIN(n, nmax);
		nmax = mfcc_buffer_samples_without_wrap(buf, (int16_t *)out);
		n = MIN(n, nmax);
		in = x + source_channel;
		if (emph->enable) {
			delay = emph->delay;
			for (i = 0; i < n; i++) {
				AE_L16_XP(sample, in, in_inc);
				/* Q1.15 -> Q1.31 */
				temp = AE_CVT32X2F16_10(sample);
				AE_MULAF16SS_00(temp, delay, coef);
				delay = sample;
				sample = AE_ROUND16X4F32SSYM(temp, temp);
				/* 2 = sizeof(ae_int16)*/
				AE_S16_0_IP(sample, out, 2);
			}
			emph->delay = delay;

		} else {
			for (i = 0; i < n; i++) {
				AE_L16_XP(sample, in, in_inc);
				/* 2 = sizeof(ae_int16)*/
				AE_S16_0_IP(sample, out, 2);
			}
		}

		x = audio_stream_wrap(source, x + n * num_channels);
		out = (ae_int16 *)mfcc_buffer_wrap(buf, (int16_t *)out);
	}
	buf->s_avail += copied;
	buf->s_free -= copied;
	buf->w_ptr = (int16_t *)out;
}

void mfcc_fill_prev_samples(struct mfcc_buffer *buf, int16_t *prev_data,
			    int prev_data_length)
{
	/* Fill prev_data from input buffer */
	ae_int32 *out = (ae_int32 *)prev_data;
	ae_int32 *in = (ae_int32 *)buf->r_ptr;
	ae_int32x2 in_sample;
	ae_int16x4 sample;
	const int inc = sizeof(ae_int32);
	int n = prev_data_length >> 1;
	int i;

	/* Set buf as circular buffer 0 */
	set_circular_buf0(buf->addr, buf->end_addr);

	/* very strange this align load is unexpected
	 * so use load a 32bit to replace 16x4 align load.
	 */
	for (i = 0; i < n; i++) {
		AE_L32_XC(in_sample, in, inc);
		/* sizeof(ae_int32) = 4 */
		AE_S32_L_IP(in_sample, out, 4);
	}
	if (prev_data_length & 0x01) {
		AE_L16_XC(sample, (ae_int16 *)in, sizeof(ae_int16));
		AE_S16_0_IP(sample, (ae_int16 *)out, sizeof(ae_int16));
	}

	buf->s_avail -= prev_data_length;
	buf->s_free += prev_data_length;
	buf->r_ptr = (void *)in; /* int16_t pointer but direct cast is not possible */
}

void mfcc_fill_fft_buffer(struct mfcc_state *state)
{
	struct mfcc_buffer *buf = &state->buf;
	struct mfcc_fft *fft = &state->fft;
	int idx = fft->fft_fill_start_idx;
	ae_int16 *out = (ae_int16 *)&fft->fft_buf[idx].real;
	ae_int16 *in = (ae_int16 *)state->prev_data;
	ae_int16x4 sample;
	const int buf_inc = sizeof(ae_int16);
	const int fft_inc = sizeof(fft->fft_buf[0]);
	int j;

	/* Copy overlapped samples from state buffer. Imaginary part of input
	 * remains zero.
	 */
	for (j = 0; j < state->prev_data_size; j++) {
		AE_L16_XP(sample, in, buf_inc);
		AE_S16_0_XP(sample, out, fft_inc);
	}

	/* Copy hop size of new data from circular buffer */
	idx += state->prev_data_size;
	in = (ae_int16 *)buf->r_ptr;
	out = (ae_int16 *)&fft->fft_buf[idx].real;
	set_circular_buf0(buf->addr, buf->end_addr);
	for (j = 0; j < fft->fft_hop_size; j++) {
		AE_L16_XC(sample, in, buf_inc);
		AE_S16_0_XP(sample, out, fft_inc);
	}

	buf->s_avail -= fft->fft_hop_size;
	buf->s_free += fft->fft_hop_size;
	buf->r_ptr = (int16_t *)in;

	/* Copy for next time data back to overlap buffer */
	idx = fft->fft_fill_start_idx + fft->fft_hop_size;
	in = (ae_int16 *)&fft->fft_buf[idx].real;
	out = (ae_int16 *)state->prev_data;
	for (j = 0; j < state->prev_data_size; j++) {
		AE_L16_XP(sample, in, fft_inc);
		AE_S16_0_XP(sample, out, buf_inc);
	}
}

#ifdef MFCC_NORMALIZE_FFT
int mfcc_normalize_fft_buffer(struct mfcc_state *state)
{
	struct mfcc_fft *fft = &state->fft;
	ae_p16s *in = (ae_p16s *)&fft->fft_buf[fft->fft_fill_start_idx].real;
	ae_int32x2 sample;
	ae_int32x2 max = AE_ZERO32();
	const int fft_inc = sizeof(fft->fft_buf[0]);
	int shift;
	int j;

	for (j = 0; j < fft->fft_size; j++) {
		/* load 16-bit data to middle of 32-bit container*/
		AE_L16M_XU(sample, in, fft_inc);
		max = AE_MAXABS32S(max, sample);
	}
	shift = AE_NSAZ32_L(max) - 8;/* 16 bit data */
	shift = MAX(shift, 0);
	shift = MIN(shift, MFCC_NORMALIZE_MAX_SHIFT);
	return shift;
}
#endif
void mfcc_apply_window(struct mfcc_state *state, int input_shift)
{
	struct mfcc_fft *fft = &state->fft;
	const int fft_inc = sizeof(fft->fft_buf[0]);
	ae_int16 *win_in = (ae_int16 *)state->window;
	const int win_inc = sizeof(ae_int16);
	ae_int32x2 temp;
	ae_int16x4 win;
	int j;

#if MFCC_FFT_BITS == 16
	ae_int16 *fft_in = (ae_int16 *)&fft->fft_buf[fft->fft_fill_start_idx].real;
	ae_int16x4 sample;

	for (j = 0; j < fft->fft_size; j++) {
		AE_L16_IP(sample, fft_in, 0);
		AE_L16_XP(win, win_in, win_inc);
		temp = AE_MULF16SS_00(sample, win);
		temp = AE_SLAA32S(temp, input_shift);
		sample = AE_ROUND16X4F32SASYM(temp, temp);
		AE_S16_0_XP(sample, fft_in, fft_inc);
	}
#else
	ae_int32 *fft_in = (ae_int32 *)&fft->fft_buf[fft->fft_fill_start_idx].real;
	ae_int32x2 sample;

	for (j = 0; j < fft->fft_size; j++) {
		AE_L32_IP(sample, fft_in, 0);
		AE_L16_XP(win, win_in, win_inc);
		temp = AE_MULFP32X16X2RS_H(sample, win);
		temp = AE_MULFP32X16X2RS_L(sample, win);
		temp = AE_SLAA32S(temp, input_shift);
		AE_S32_L_XP(temp, fft_in, fft_inc);
	}
#endif
}

#if CONFIG_FORMAT_S16LE

int16_t *mfcc_sink_copy_zero_s16(const struct audio_stream *sink,
				 int16_t *w_ptr, int samples)
{
	int i;
	int n = samples >> 2;
	int m = samples & 0x03;
	ae_int16x4 *out = (ae_int16x4 *)w_ptr;
	const int inc = sizeof(ae_int16);
	ae_valign outu = AE_ZALIGN64();
	ae_int16x4 zero = AE_ZERO16();

	set_circular_buf0(sink->addr, sink->end_addr);

	for (i = 0; i < n; i++)
		AE_SA16X4_IC(zero, outu, out);

	AE_SA64POS_FP(outu, out);
	/* process the left samples that less than 4
	 * one by one to avoid memory access overrun
	 */
	for (i = 0; i < m ; i++)
		AE_S16_0_XC(zero, (ae_int16 *)out, inc);

	return (int16_t *)out;
}

int16_t *mfcc_sink_copy_data_s16(const struct audio_stream *sink, int16_t *w_ptr,
				 int samples, int16_t *r_ptr)
{
	int i;
	int n = samples >> 2;
	int m = samples & 0x03;
	ae_int16x4 *out = (ae_int16x4 *)w_ptr;
	ae_int16x4 *in = (ae_int16x4 *)r_ptr;
	ae_valign outu = AE_ZALIGN64();
	ae_valign inu = AE_ZALIGN64();
	const int inc = sizeof(ae_int16);
	ae_int16x4 in_sample;

	set_circular_buf0(sink->addr, sink->end_addr);

	inu = AE_LA64_PP(in);
	for (i = 0; i < n; i++) {
		AE_LA16X4_IP(in_sample, inu, in);
		AE_SA16X4_IC(in_sample, outu, out);
	}
	AE_SA64POS_FP(outu, out);
	/* process the left samples that less than 4
	 * one by one to avoid memory access overrun
	 */
	for (i = 0; i < m ; i++) {
		AE_L16_XP(in_sample, (ae_int16 *)in, inc);
		AE_S16_0_XC(in_sample, (ae_int16 *)out, inc);
	}

	return (int16_t *)out;
}

#endif /* CONFIG_FORMAT_S16LE */
#endif
