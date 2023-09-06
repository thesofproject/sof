// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/mfcc/mfcc_comp.h>
#ifdef MFCC_GENERIC

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

/*
 * MFCC algorithm code
 */

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

void mfcc_fill_fft_buffer(struct mfcc_state *state)
{
	struct mfcc_buffer *buf = &state->buf;
	struct mfcc_fft *fft = &state->fft;
	int16_t *r = buf->r_ptr;
	int copied;
	int nmax;
	int idx = fft->fft_fill_start_idx;
	int j;
	int n;

	/* Copy overlapped samples from state buffer. Imaginary part of input
	 * remains zero.
	 */
	for (j = 0; j < state->prev_data_size; j++)
		fft->fft_buf[idx + j].real = state->prev_data[j];

	/* Copy hop size of new data from circular buffer */
	idx += state->prev_data_size;
	for (copied = 0; copied < fft->fft_hop_size; copied += n) {
		nmax = fft->fft_hop_size - copied;
		n = mfcc_buffer_samples_without_wrap(buf, r);
		n = MIN(n, nmax);
		for (j = 0; j < n; j++) {
			fft->fft_buf[idx].real = *r;
			r++;
			idx++;
		}
		r = mfcc_buffer_wrap(buf, r);
	}

	buf->s_avail -= copied;
	buf->s_free += copied;
	buf->r_ptr = r;

	/* Copy for next time data back to overlap buffer */
	idx = fft->fft_fill_start_idx + fft->fft_hop_size;
	for (j = 0; j < state->prev_data_size; j++)
		state->prev_data[j] = fft->fft_buf[idx + j].real;
}

#ifdef MFCC_NORMALIZE_FFT
int mfcc_normalize_fft_buffer(struct mfcc_state *state)
{
	struct mfcc_fft *fft = &state->fft;
	int32_t absx;
	int32_t smax = 0;
	int32_t x;
	int shift;
	int j;
	int i = fft->fft_fill_start_idx;

	for (j = 0; j < fft->fft_size; j++) {
		x = fft->fft_buf[i + j].real;
		absx = (x < 0) ? -x : x;
		if (smax < absx)
			smax = absx;
	}

	shift = norm_int32(smax << 15) - 1; /* 16 bit data */
	shift = MAX(shift, 0);
	shift = MIN(shift, MFCC_NORMALIZE_MAX_SHIFT);
	return shift;
}
#endif

void mfcc_apply_window(struct mfcc_state *state, int input_shift)
{
	struct mfcc_fft *fft = &state->fft;
	int j;
	int i = fft->fft_fill_start_idx;

#if MFCC_FFT_BITS == 16
	/* TODO: Use proper multiply and saturate function to make sure no overflows */
	int32_t x;
	int s = 14 - input_shift; /* Q1.15 x Q1.15 -> Q30 -> Q15, shift by 15 - 1 for round */

	for (j = 0; j < fft->fft_size; j++) {
		x = (int32_t)fft->fft_buf[i + j].real * state->window[j];
		fft->fft_buf[i + j].real = ((x >> s) + 1) >> 1;
	}
#else
	/* TODO: Use proper multiply and saturate function to make sure no overflows */
	int s = input_shift + 1; /* To convert 16 -> 32 with Q1.15 x Q1.15 -> Q30 -> Q31 */

	for (j = 0; j < fft->fft_size; j++)
		fft->fft_buf[i + j].real = (fft->fft_buf[i + j].real * state->window[j]) << s;
#endif
}

#if CONFIG_FORMAT_S16LE

int16_t *mfcc_sink_copy_zero_s16(const struct audio_stream *sink,
				 int16_t *w_ptr, int samples)
{
	int copied;
	int nmax;
	int i;
	int n;

	for (copied = 0; copied < samples; copied += n) {
		nmax = samples - copied;
		n = audio_stream_samples_without_wrap_s16(sink, w_ptr);
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*w_ptr = 0;
			w_ptr++;
		}

		w_ptr = audio_stream_wrap(sink, w_ptr);
	}

	return w_ptr;
}

int16_t *mfcc_sink_copy_data_s16(const struct audio_stream *sink, int16_t *w_ptr,
				 int samples, int16_t *r_ptr)
{
	int copied;
	int nmax;
	int i;
	int n;

	for (copied = 0; copied < samples; copied += n) {
		nmax = samples - copied;
		n = audio_stream_samples_without_wrap_s16(sink, w_ptr);
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*w_ptr = *r_ptr;
			r_ptr++;
			w_ptr++;
		}

		w_ptr = audio_stream_wrap(sink, w_ptr);
	}

	return w_ptr;
}

#endif /* CONFIG_FORMAT_S16LE */
#endif
