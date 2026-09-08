// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_HIFI(NONE, FILTER) || SOF_USE_RISCV_SIMD(FILTER)

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/fir_generic.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "eq_fir.h"

LOG_MODULE_DECLARE(eq_fir, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_FORMAT_S16LE
void eq_fir_s16(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct fir_state_32x16 *filter;
	int32_t z;
	int16_t *x0, *y0;
	int16_t *x = audio_stream_get_rptr(source);
	int16_t *y = audio_stream_get_wptr(sink);
	int nmax, n, i, j;
	int nch = audio_stream_get_channels(source);
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		nmax = EQ_FIR_BYTES_TO_S16_SAMPLES(audio_stream_bytes_without_wrap(source, x));
		n = MIN(remaining_samples, nmax);
		nmax = EQ_FIR_BYTES_TO_S16_SAMPLES(audio_stream_bytes_without_wrap(sink, y));
		n = MIN(n, nmax);
		for (j = 0; j < nch; j++) {
			x0 = x + j;
			y0 = y + j;
			filter = &fir[j];
			for (i = 0; i < n; i += nch) {
				z = fir_32x16(filter, *x0 << 16);
				*y0 = sat_int16(Q_SHIFT_RND(z, 31, 15));
				x0 += nch;
				y0 += nch;
			}
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_s24(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct fir_state_32x16 *filter;
	int32_t z;
	int32_t *x0, *y0;
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int nmax, n, i, j;
	int nch = audio_stream_get_channels(source);
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		nmax = EQ_FIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(source, x));
		n = MIN(remaining_samples, nmax);
		nmax = EQ_FIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(sink, y));
		n = MIN(n, nmax);
		for (j = 0; j < nch; j++) {
			x0 = x + j;
			y0 = y + j;
			filter = &fir[j];
			for (i = 0; i < n; i += nch) {
				z = fir_32x16(filter, *x0 << 8);
				*y0 = sat_int24(Q_SHIFT_RND(z, 31, 23));
				x0 += nch;
				y0 += nch;
			}
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
void eq_fir_s32(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct fir_state_32x16 *filter;
	int32_t *x0, *y0;
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int nmax, n, i, j;
	int nch = audio_stream_get_channels(source);
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		nmax = EQ_FIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(source, x));
		n = MIN(remaining_samples, nmax);
		nmax = EQ_FIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(sink, y));
		n = MIN(n, nmax);
		for (j = 0; j < nch; j++) {
			x0 = x + j;
			y0 = y + j;
			filter = &fir[j];
			for (i = 0; i < n; i += nch) {
				*y0 = fir_32x16(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_FLOAT
void eq_fir_float(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		  struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct fir_state_float *fir_f = (struct fir_state_float *)fir;
	struct fir_state_float *filter;
	float *x0, *y0;
	float *x = audio_stream_get_rptr(source);
	float *y = audio_stream_get_wptr(sink);
	int nmax, n, i, j;
	int nch = audio_stream_get_channels(source);
	int remaining_samples = frames * nch;

	while (remaining_samples) {
		nmax = EQ_FIR_BYTES_TO_FLOAT_SAMPLES(audio_stream_bytes_without_wrap(source, x));
		n = MIN(remaining_samples, nmax);
		nmax = EQ_FIR_BYTES_TO_FLOAT_SAMPLES(audio_stream_bytes_without_wrap(sink, y));
		n = MIN(n, nmax);
		for (j = 0; j < nch; j++) {
			x0 = x + j;
			y0 = y + j;
			filter = &fir_f[j];
			for (i = 0; i < n; i += nch) {
				*y0 = fir_float(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}

void eq_fir_2x_float(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		     struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct fir_state_float *fir_f = (struct fir_state_float *)fir;
	struct fir_state_float *filter;
	float *x = audio_stream_get_rptr(source);
	float *y = audio_stream_get_wptr(sink);
	float *x0, *x1, *y0, *y1;
	int fmax, f, i, j;
	int nch = audio_stream_get_channels(source);
	int inc_2nch = 2 * nch;
	int remaining_frames = frames;

	while (remaining_frames) {
		fmax = audio_stream_frames_without_wrap(source, x);
		f = MIN(remaining_frames, fmax);
		fmax = audio_stream_frames_without_wrap(sink, y);
		f = MIN(f, fmax);

		if (f >= 2) {
			f &= ~0x1; /* Process even number of frames */
			for (j = 0; j < nch; j++) {
				x0 = x + j;
				x1 = x0 + nch;
				y0 = y + j;
				y1 = y0 + nch;
				filter = &fir_f[j];
				for (i = 0; i < f; i += 2) {
					fir_float_2x(filter, *x0, *x1, y0, y1);
					x0 += inc_2nch;
					x1 += inc_2nch;
					y0 += inc_2nch;
					y1 += inc_2nch;
				}
			}
			remaining_frames -= f;
			x = audio_stream_wrap(source, x + f * nch);
			y = audio_stream_wrap(sink, y + f * nch);
		} else {
			/* Single frame boundary wrap fallback */
			for (j = 0; j < nch; j++) {
				filter = &fir_f[j];
				*(y + j) = fir_float(filter, *(x + j));
			}
			remaining_frames -= 1;
			x = audio_stream_wrap(source, x + nch);
			y = audio_stream_wrap(sink, y + nch);
		}
	}
}
#endif /* CONFIG_FORMAT_FLOAT */

#endif /* FILTER_HIFI_NONE */
