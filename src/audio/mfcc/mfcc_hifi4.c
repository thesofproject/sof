// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023-2026 Intel Corporation.
//
// Author: Andrula Song <andrula.song@intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>

#ifdef MFCC_HIFI4

#include <sof/audio/component.h>
#include <sof/audio/audio_stream.h>
#include <sof/math/auditory.h>
#include <sof/math/icomplex16.h>
#include <sof/math/icomplex32.h>
#include <sof/math/matrix.h>
#include <sof/math/sqrt.h>
#include <sof/math/trig.h>
#include <sof/math/window.h>
#include <sof/trace/trace.h>
#include <user/mfcc.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <xtensa/tie/xt_hifi4.h>

/* Setup circular buffer 0 */
static inline void set_circular_buf0(const void *start, const void *end)
{
	AE_SETCBEGIN0(start);
	AE_SETCEND0(end);
}

/* Setup circular for buffer 1 */
static inline void set_circular_buf1(const void *start, const void *end)
{
	AE_SETCBEGIN1(start);
	AE_SETCEND1(end);
}

/*
 * MFCC algorithm code
 */

#if CONFIG_FORMAT_S16LE
void mfcc_source_copy_s16(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	struct audio_stream *source = bsource->data;
	int num_channels = audio_stream_get_channels(source);
	ae_int16 *in = (ae_int16 *)source->r_ptr + source_channel;
	ae_int16 *out = (ae_int16 *)buf->w_ptr;
	ae_int16x4 sample;
	ae_int32x2 temp;
	ae_int16x4 coef;
	ae_int16x4 delay;
	const int in_inc = sizeof(ae_int16) * num_channels;
	const int out_inc = sizeof(ae_int16);
	int i;

	set_circular_buf1(buf->addr, buf->end_addr);
	set_circular_buf0(source->addr, source->end_addr);

	/* Copy from source to pre-buffer for FFT.
	 * The pre-emphasis filter is done in this step.
	 */
	if (emph->enable) {
		delay = emph->delay;
		coef = emph->coef;
		for (i = 0; i < frames; i++) {
			AE_L16_XC(sample, in, in_inc);

			/* Q1.15 -> Q1.31 */
			temp = AE_CVT32X2F16_10(sample);
			AE_MULAF16SS_00(temp, delay, coef);
			delay = sample;
			sample = AE_ROUND16X4F32SSYM(temp, temp);
			AE_S16_0_XC1(sample, out, out_inc);
		}
		emph->delay = delay;
	} else {
		for (i = 0; i < frames; i++) {
			AE_L16_XC(sample, in, in_inc);
			AE_S16_0_XC1(sample, out, out_inc);
		}
	}

	buf->s_avail += frames;
	buf->s_free -= frames;
	buf->w_ptr = (int16_t *)out;
}
#endif /* CONFIG_FORMAT_S16LE */

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
	buf->r_ptr = (int16_t *)in;
}

void mfcc_apply_window(struct mfcc_state *state, int input_shift)
{
	struct mfcc_fft *fft = &state->fft;
	const int fft_inc = sizeof(fft->fft_buf[0]);
	ae_int16 *win_in = (ae_int16 *)state->window;
	const int win_inc = sizeof(ae_int16);
	ae_int32x2 temp;
	ae_int16x4 win;
	int j;

	ae_int32 *fft_in = (ae_int32 *)&fft->fft_buf[fft->fft_fill_start_idx].real;
	ae_int32x2 sample;

	for (j = 0; j < fft->fft_size; j++) {
		AE_L32_IP(sample, fft_in, 0);
		AE_L16_XP(win, win_in, win_inc);
		/* Data is 16-bit in 32-bit container, shift to Q1.31 for fractional multiply */
		sample = AE_SLAI32S(sample, 16);
		temp = AE_MULFP32X16X2RS_L(sample, win);
		temp = AE_SLAA32S(temp, input_shift);
		AE_S32_L_XP(temp, fft_in, fft_inc);
	}
}

#if CONFIG_FORMAT_S24LE
void mfcc_source_copy_s24(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	struct audio_stream *source = bsource->data;
	int num_channels = audio_stream_get_channels(source);
	ae_int32 *in = (ae_int32 *)source->r_ptr + source_channel;
	ae_int16 *out = (ae_int16 *)buf->w_ptr;
	ae_int32x2 sample32;
	ae_int16x4 sample;
	ae_int32x2 temp;
	ae_int16x4 coef;
	ae_int16x4 delay;
	const int in_inc = sizeof(ae_int32) * num_channels;
	const int out_inc = sizeof(ae_int16);
	int i;

	set_circular_buf1(buf->addr, buf->end_addr);
	set_circular_buf0(source->addr, source->end_addr);

	if (emph->enable) {
		delay = emph->delay;
		coef = emph->coef;
		for (i = 0; i < frames; i++) {
			AE_L32_XC(sample32, in, in_inc);
			/* Shift left by 8 to sign-extend to Q1.31 */
			sample32 = AE_SLAI32(sample32, 8);
			/* Then shift right by 16 to get 16-bit */
			sample32 = AE_SRAI32(sample32, 16);
			sample = AE_SAT16X4(sample32, sample32);
			/* Q1.15 -> Q1.31 */
			temp = AE_CVT32X2F16_10(sample);
			AE_MULAF16SS_00(temp, delay, coef);
			delay = sample;
			sample = AE_ROUND16X4F32SSYM(temp, temp);
			AE_S16_0_XC1(sample, out, out_inc);
		}
		emph->delay = delay;
	} else {
		for (i = 0; i < frames; i++) {
			AE_L32_XC(sample32, in, in_inc);
			/* Shift left by 8 to sign-extend to Q1.31 */
			sample32 = AE_SLAI32(sample32, 8);
			/* Then shift right by 16 to get 16-bit */
			sample32 = AE_SRAI32(sample32, 16);
			sample = AE_SAT16X4(sample32, sample32);
			AE_S16_0_XC1(sample, out, out_inc);
		}
	}

	buf->s_avail += frames;
	buf->s_free -= frames;
	buf->w_ptr = (int16_t *)out;
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
void mfcc_source_copy_s32(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	struct audio_stream *source = bsource->data;
	int num_channels = audio_stream_get_channels(source);
	ae_int32 *in = (ae_int32 *)source->r_ptr + source_channel;
	ae_int16 *out = (ae_int16 *)buf->w_ptr;
	ae_int32x2 sample32;
	ae_int16x4 sample;
	ae_int32x2 temp;
	ae_int16x4 coef;
	ae_int16x4 delay;
	const int in_inc = sizeof(ae_int32) * num_channels;
	const int out_inc = sizeof(ae_int16);
	int i;

	set_circular_buf1(buf->addr, buf->end_addr);
	set_circular_buf0(source->addr, source->end_addr);

	if (emph->enable) {
		delay = emph->delay;
		coef = emph->coef;
		for (i = 0; i < frames; i++) {
			AE_L32_XC(sample32, in, in_inc);
			/* S32: shift right by 16 to get 16-bit */
			sample32 = AE_SRAI32(sample32, 16);
			sample = AE_SAT16X4(sample32, sample32);
			/* Q1.15 -> Q1.31 */
			temp = AE_CVT32X2F16_10(sample);
			AE_MULAF16SS_00(temp, delay, coef);
			delay = sample;
			sample = AE_ROUND16X4F32SSYM(temp, temp);
			AE_S16_0_XC1(sample, out, out_inc);
		}
		emph->delay = delay;
	} else {
		for (i = 0; i < frames; i++) {
			AE_L32_XC(sample32, in, in_inc);
			sample32 = AE_SRAI32(sample32, 16);
			sample = AE_SAT16X4(sample32, sample32);
			AE_S16_0_XC1(sample, out, out_inc);
		}
	}

	buf->s_avail += frames;
	buf->s_free -= frames;
	buf->w_ptr = (int16_t *)out;
}
#endif /* CONFIG_FORMAT_S32LE */

#endif /* MFCC_HIFI4 */
