// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_ARM_SIMD(FILTER)

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/arm_simd.h>
#include <sof/math/fir_generic.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "eq_fir.h"

LOG_MODULE_DECLARE(eq_fir, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_FORMAT_S32LE
void eq_fir_2x_s32(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		   struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct fir_state_32x16 *f;
	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dst = audio_stream_get_wptr(sink);
	int32_t *x, *y;
	int32_t z0, z1;
	int ch, i, n, nmax;
	int nch = audio_stream_get_channels(source);
	int inc_2nch = 2 * nch;
	int samples = nch * frames;

	while (samples) {
		nmax = audio_stream_samples_without_wrap_s32(sink, dst);
		n = MIN(nmax, samples);
		nmax = audio_stream_samples_without_wrap_s32(source, src);
		n = MIN(n, nmax);

		int n_frames = n / nch;
		int even_frames = n_frames & ~1;
		int even_samples = even_frames * nch;

		for (ch = 0; ch < nch; ch++) {
			f = &fir[ch];
			x = src + ch;
			y = dst + ch;

			for (i = 0; i < even_samples; i += inc_2nch) {
				fir_32x16_2x(f, x[0], x[nch], &z0, &z1);
				y[0] = z0;
				y[nch] = z1;
				x += inc_2nch;
				y += inc_2nch;
			}

			if (n_frames & 1) {
				*y = fir_32x16(f, *x);
			}
		}

		samples -= n;
		dst = audio_stream_wrap(sink, dst + n);
		src = audio_stream_wrap(source, src + n);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_2x_s24(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		   struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct fir_state_32x16 *f;
	int32_t *src = audio_stream_get_rptr(source);
	int32_t *dst = audio_stream_get_wptr(sink);
	int32_t *x, *y;
	int32_t z0, z1;
	int ch, i, n, nmax;
	int nch = audio_stream_get_channels(source);
	int inc_2nch = 2 * nch;
	int samples = nch * frames;

	while (samples) {
		nmax = audio_stream_samples_without_wrap_s24(sink, dst);
		n = MIN(nmax, samples);
		nmax = audio_stream_samples_without_wrap_s24(source, src);
		n = MIN(n, nmax);

		int n_frames = n / nch;
		int even_frames = n_frames & ~1;
		int even_samples = even_frames * nch;

		for (ch = 0; ch < nch; ch++) {
			f = &fir[ch];
			x = src + ch;
			y = dst + ch;

			for (i = 0; i < even_samples; i += inc_2nch) {
				fir_32x16_2x(f, x[0] << 8, x[nch] << 8, &z0, &z1);
				y[0] = sat_int24(Q_SHIFT_RND(z0, 31, 23));
				y[nch] = sat_int24(Q_SHIFT_RND(z1, 31, 23));
				x += inc_2nch;
				y += inc_2nch;
			}

			if (n_frames & 1) {
				int32_t z = fir_32x16(f, *x << 8);
				*y = sat_int24(Q_SHIFT_RND(z, 31, 23));
			}
		}

		samples -= n;
		dst = audio_stream_wrap(sink, dst + n);
		src = audio_stream_wrap(source, src + n);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S16LE
void eq_fir_2x_s16(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		   struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct fir_state_32x16 *f;
	int16_t *src = audio_stream_get_rptr(source);
	int16_t *dst = audio_stream_get_wptr(sink);
	int16_t *x, *y;
	int32_t z0, z1;
	int ch, i, n, nmax;
	int nch = audio_stream_get_channels(source);
	int inc_2nch = 2 * nch;
	int samples = nch * frames;

	while (samples) {
		nmax = audio_stream_samples_without_wrap_s16(sink, dst);
		n = MIN(nmax, samples);
		nmax = audio_stream_samples_without_wrap_s16(source, src);
		n = MIN(n, nmax);

		int n_frames = n / nch;
		int even_frames = n_frames & ~1;
		int even_samples = even_frames * nch;

		for (ch = 0; ch < nch; ch++) {
			f = &fir[ch];
			x = src + ch;
			y = dst + ch;

			for (i = 0; i < even_samples; i += inc_2nch) {
				fir_32x16_2x(f, (int32_t)x[0] << 16, (int32_t)x[nch] << 16, &z0, &z1);
				y[0] = arm_sat_s16(Q_SHIFT_RND(z0, 31, 15));
				y[nch] = arm_sat_s16(Q_SHIFT_RND(z1, 31, 15));
				x += inc_2nch;
				y += inc_2nch;
			}

			if (n_frames & 1) {
				int32_t z = fir_32x16(f, (int32_t)*x << 16);
				*y = arm_sat_s16(Q_SHIFT_RND(z, 31, 15));
			}
		}

		samples -= n;
		dst = audio_stream_wrap(sink, dst + n);
		src = audio_stream_wrap(source, src + n);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#endif /* SOF_USE_ARM_SIMD(FILTER) */
