// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_ARM_SIMD(FILTER)

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/format.h>
#include <sof/audio/arm_simd.h>
#include <sof/math/fir_generic.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "eq_fir.h"

LOG_MODULE_DECLARE(eq_fir, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_FORMAT_S32LE
void eq_fir_2x_s32(struct fir_state_32x16 fir[], const struct cir_buf_source *source,
		   struct cir_buf_sink *sink, size_t frames, unsigned int channels)
{
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t remaining_frames = frames;
	const size_t inc_2nch = 2 * channels;

	while (remaining_frames) {
		size_t source_frames =
			cir_buf_samples_without_wrap_s32(src, source->buf_end) / channels;
		size_t sink_frames =
			cir_buf_samples_without_wrap_s32(dst, sink->buf_end) / channels;
		size_t chunk_frames = MIN(remaining_frames, MIN(source_frames, sink_frames));

		if (!chunk_frames) {
			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
			continue;
		}

		size_t even_frames = chunk_frames & ~1;
		size_t even_samples = even_frames * channels;

		for (unsigned int ch = 0; ch < channels; ch++) {
			struct fir_state_32x16 *f = &fir[ch];
			const int32_t *x = src + ch;
			int32_t *y = dst + ch;

			for (size_t i = 0; i < even_samples; i += inc_2nch) {
				int32_t z0, z1;

				fir_32x16_2x(f, x[0], x[channels], &z0, &z1);
				y[0] = z0;
				y[channels] = z1;
				x += inc_2nch;
				y += inc_2nch;
			}

			if (chunk_frames & 1)
				*y = fir_32x16(f, *x);
		}

		size_t chunk_samples = chunk_frames * channels;

		remaining_frames -= chunk_frames;
		src = source_cir_buf_wrap(src + chunk_samples, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + chunk_samples, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_2x_s24(struct fir_state_32x16 fir[], const struct cir_buf_source *source,
		   struct cir_buf_sink *sink, size_t frames, unsigned int channels)
{
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t remaining_frames = frames;
	const size_t inc_2nch = 2 * channels;

	while (remaining_frames) {
		size_t source_frames =
			cir_buf_samples_without_wrap_s32(src, source->buf_end) / channels;
		size_t sink_frames =
			cir_buf_samples_without_wrap_s32(dst, sink->buf_end) / channels;
		size_t chunk_frames = MIN(remaining_frames, MIN(source_frames, sink_frames));

		if (!chunk_frames) {
			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
			continue;
		}

		size_t even_frames = chunk_frames & ~1;
		size_t even_samples = even_frames * channels;

		for (unsigned int ch = 0; ch < channels; ch++) {
			struct fir_state_32x16 *f = &fir[ch];
			const int32_t *x = src + ch;
			int32_t *y = dst + ch;

			for (size_t i = 0; i < even_samples; i += inc_2nch) {
				int32_t z0, z1;

				fir_32x16_2x(f, x[0] << 8, x[channels] << 8, &z0, &z1);
				y[0] = sat_int24(Q_SHIFT_RND(z0, 31, 23));
				y[channels] = sat_int24(Q_SHIFT_RND(z1, 31, 23));
				x += inc_2nch;
				y += inc_2nch;
			}

			if (chunk_frames & 1) {
				int32_t z = fir_32x16(f, *x << 8);

				*y = sat_int24(Q_SHIFT_RND(z, 31, 23));
			}
		}

		size_t chunk_samples = chunk_frames * channels;

		remaining_frames -= chunk_frames;
		src = source_cir_buf_wrap(src + chunk_samples, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + chunk_samples, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S16LE
void eq_fir_2x_s16(struct fir_state_32x16 fir[], const struct cir_buf_source *source,
		   struct cir_buf_sink *sink, size_t frames, unsigned int channels)
{
	const int16_t *src = source->ptr;
	int16_t *dst = sink->ptr;
	size_t remaining_frames = frames;
	const size_t inc_2nch = 2 * channels;

	while (remaining_frames) {
		size_t source_frames =
			cir_buf_samples_without_wrap_s16(src, source->buf_end) / channels;
		size_t sink_frames =
			cir_buf_samples_without_wrap_s16(dst, sink->buf_end) / channels;
		size_t chunk_frames = MIN(remaining_frames, MIN(source_frames, sink_frames));

		if (!chunk_frames) {
			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
			continue;
		}

		size_t even_frames = chunk_frames & ~1;
		size_t even_samples = even_frames * channels;

		for (unsigned int ch = 0; ch < channels; ch++) {
			struct fir_state_32x16 *f = &fir[ch];
			const int16_t *x = src + ch;
			int16_t *y = dst + ch;

			for (size_t i = 0; i < even_samples; i += inc_2nch) {
				int32_t z0, z1;

				fir_32x16_2x(f, (int32_t)x[0] << 16, (int32_t)x[channels] << 16, &z0, &z1);
				y[0] = arm_sat_s16(Q_SHIFT_RND(z0, 31, 15));
				y[channels] = arm_sat_s16(Q_SHIFT_RND(z1, 31, 15));
				x += inc_2nch;
				y += inc_2nch;
			}

			if (chunk_frames & 1) {
				int32_t z = fir_32x16(f, (int32_t)*x << 16);

				*y = arm_sat_s16(Q_SHIFT_RND(z, 31, 15));
			}
		}

		size_t chunk_samples = chunk_frames * channels;

		remaining_frames -= chunk_frames;
		src = source_cir_buf_wrap(src + chunk_samples, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + chunk_samples, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#endif /* SOF_USE_ARM_SIMD(FILTER) */
