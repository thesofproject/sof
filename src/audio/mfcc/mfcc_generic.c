// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022-2026 Intel Corporation.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>
#ifdef MFCC_GENERIC

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

/*
 * MFCC algorithm code
 */

void mfcc_fill_prev_samples(struct mfcc_buffer *buf, int16_t *prev_data,
			    int prev_data_length)
{
	/* Fill prev_data from input buffer */
	int16_t *r = buf->r_ptr;
	int16_t *p = prev_data;
	int copied;
	int nmax;
	int n;

	for (copied = 0; copied < prev_data_length; copied += n) {
		nmax = prev_data_length - copied;
		n = mfcc_buffer_samples_without_wrap(buf, r);
		n = MIN(n, nmax);
		memcpy(p, r, sizeof(int16_t) * n); /* Not using memcpy_s() due to speed need */
		p += n;
		r += n;
		r = mfcc_buffer_wrap(buf, r);
	}

	buf->s_avail -= copied;
	buf->s_free += copied;
	buf->r_ptr = r;
}

void mfcc_apply_window(struct mfcc_state *state, int input_shift)
{
	struct mfcc_fft *fft = &state->fft;
	int j;
	int i = fft->fft_fill_start_idx;

	/* TODO: Use proper multiply and saturate function to make sure no overflows */
	int s = input_shift + 1; /* To convert 16 -> 32 with Q1.15 x Q1.15 -> Q30 -> Q31 */

	for (j = 0; j < fft->fft_size; j++)
		fft->fft_buf[i + j].real = (fft->fft_buf[i + j].real * state->window[j]) << s;
}

#if CONFIG_FORMAT_S16LE
void mfcc_source_copy_s16(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	struct audio_stream *source = bsource->data;
	int32_t s;
	int16_t *x0;
	int16_t *x = audio_stream_get_rptr(source);
	int16_t *w = buf->w_ptr;
	int copied;
	int nmax;
	int n1;
	int n2;
	int n;
	int i;
	int num_channels = audio_stream_get_channels(source);

	/* Copy from source to pre-buffer for FFT.
	 * The pre-emphasis filter is done in this step.
	 */
	for (copied = 0; copied < frames; copied += n) {
		nmax = frames - copied;
		n1 = audio_stream_frames_without_wrap(source, x);
		n2 = mfcc_buffer_samples_without_wrap(buf, w);
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		x0 = x + source_channel;
		for (i = 0; i < n; i++) {
			if (emph->enable) {
				/* Q1.15 x Q1.15 -> Q2.30 */
				s = (int32_t)emph->delay * emph->coef + Q_SHIFT_LEFT(*x0, 15, 30);
				*w = sat_int16(Q_SHIFT_RND(s, 30, 15));
				emph->delay = *x0;
			} else {
				*w = *x0;
			}
			x0 += num_channels;
			w++;
		}

		x = audio_stream_wrap(source, x + n * audio_stream_get_channels(source));
		w = mfcc_buffer_wrap(buf, w);
	}
	buf->s_avail += copied;
	buf->s_free -= copied;
	buf->w_ptr = w;
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE

void mfcc_source_copy_s24(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	struct audio_stream *source = bsource->data;
	int32_t tmp, s;
	int32_t *x0;
	int32_t *x = audio_stream_get_rptr(source);
	int16_t *w = buf->w_ptr;
	int copied;
	int nmax;
	int n1;
	int n2;
	int n;
	int i;
	int num_channels = audio_stream_get_channels(source);

	/* Copy from source to pre-buffer for FFT.
	 * The pre-emphasis filter is done in this step.
	 * S24_4LE data is in 32-bit container, shift left by 8 to Q1.31,
	 * then convert to Q1.15 with rounding.
	 */
	for (copied = 0; copied < frames; copied += n) {
		nmax = frames - copied;
		n1 = audio_stream_frames_without_wrap(source, x);
		n2 = mfcc_buffer_samples_without_wrap(buf, w);
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		x0 = x + source_channel;
		for (i = 0; i < n; i++) {
			if (emph->enable) {
				/* Convert to Q1.31, ignore highest byte */
				s = (int32_t)((uint32_t)*x0 << 8);
				/* Q1.15 x Q1.15 -> Q2.30 */
				tmp = (int32_t)emph->delay * emph->coef + Q_SHIFT(s, 31, 30);
				*w = sat_int16(Q_SHIFT_RND(tmp, 30, 15));
				emph->delay = sat_int16(Q_SHIFT_RND(s, 31, 15));
			} else {
				/* Convert to Q1.31, ignore highest byte */
				s = (int32_t)((uint32_t)*x0 << 8);
				*w = sat_int16(Q_SHIFT_RND(s, 31, 15));
			}
			x0 += num_channels;
			w++;
		}

		x = audio_stream_wrap(source, x + n * audio_stream_get_channels(source));
		w = mfcc_buffer_wrap(buf, w);
	}
	buf->s_avail += copied;
	buf->s_free -= copied;
	buf->w_ptr = w;
}

#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE

void mfcc_source_copy_s32(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel)
{
	struct audio_stream *source = bsource->data;
	int32_t s;
	int32_t *x0;
	int32_t *x = audio_stream_get_rptr(source);
	int16_t *w = buf->w_ptr;
	int copied;
	int nmax;
	int n1;
	int n2;
	int n;
	int i;
	int num_channels = audio_stream_get_channels(source);

	/* Copy from source to pre-buffer for FFT.
	 * The pre-emphasis filter is done in this step.
	 * S32 data is in 32-bit container, shift right by 16 to get 16-bit.
	 */
	for (copied = 0; copied < frames; copied += n) {
		nmax = frames - copied;
		n1 = audio_stream_frames_without_wrap(source, x);
		n2 = mfcc_buffer_samples_without_wrap(buf, w);
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		x0 = x + source_channel;
		for (i = 0; i < n; i++) {
			if (emph->enable) {
				/* Q1.15 x Q1.15 -> Q2.30 */
				s = (int32_t)emph->delay * emph->coef + Q_SHIFT(*x0, 31, 30);
				*w = sat_int16(Q_SHIFT_RND(s, 30, 15));
				emph->delay = sat_int16(Q_SHIFT_RND(*x0, 31, 15));
			} else {
				*w = sat_int16(Q_SHIFT_RND(*x0, 31, 15));
			}
			x0 += num_channels;
			w++;
		}

		x = audio_stream_wrap(source, x + n * audio_stream_get_channels(source));
		w = mfcc_buffer_wrap(buf, w);
	}
	buf->s_avail += copied;
	buf->s_free -= copied;
	buf->w_ptr = w;
}
#endif /* CONFIG_FORMAT_S32LE */

#endif /* MFCC_GENERIC */
