// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_HIFI(NONE, FILTER) || SOF_USE_ARM_SIMD(FILTER) || SOF_USE_RISCV_SIMD(FILTER)

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/fir_generic.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "eq_fir.h"

LOG_MODULE_DECLARE(eq_fir, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_FORMAT_S16LE
void eq_fir_s16(struct fir_state_32x16 fir[], const struct cir_buf_source *source,
		struct cir_buf_sink *sink, size_t frames, unsigned int channels)
{
	struct fir_state_32x16 *filter;
	int32_t filtered_sample;
	const int16_t *src_channel;
	int16_t *dst_channel;
	const int16_t *src = source->ptr;
	int16_t *dst = sink->ptr;
	size_t max_samples;
	size_t chunk_samples;
	size_t sample_index;
	size_t remaining_samples = frames * channels;
	unsigned int channel;

	while (remaining_samples) {
		max_samples = cir_buf_samples_without_wrap_s16(src, source->buf_end);
		chunk_samples = MIN(remaining_samples, max_samples);
		max_samples = cir_buf_samples_without_wrap_s16(dst, sink->buf_end);
		chunk_samples = MIN(chunk_samples, max_samples);
		for (channel = 0; channel < channels; channel++) {
			src_channel = src + channel;
			dst_channel = dst + channel;
			filter = &fir[channel];
			for (sample_index = 0; sample_index < chunk_samples;
				sample_index += channels) {
				filtered_sample = fir_32x16(filter, *src_channel << 16);
				*dst_channel = sat_int16(Q_SHIFT_RND(filtered_sample, 31, 15));
				src_channel += channels;
				dst_channel += channels;
			}
		}
		remaining_samples -= chunk_samples;
		src = source_cir_buf_wrap(src + chunk_samples, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + chunk_samples, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_s24(struct fir_state_32x16 fir[], const struct cir_buf_source *source,
		struct cir_buf_sink *sink, size_t frames, unsigned int channels)
{
	struct fir_state_32x16 *filter;
	int32_t filtered_sample;
	const int32_t *src_channel;
	int32_t *dst_channel;
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t max_samples;
	size_t chunk_samples;
	size_t sample_index;
	size_t remaining_samples = frames * channels;
	unsigned int channel;

	while (remaining_samples) {
		max_samples = cir_buf_samples_without_wrap_s32(src, source->buf_end);
		chunk_samples = MIN(remaining_samples, max_samples);
		max_samples = cir_buf_samples_without_wrap_s32(dst, sink->buf_end);
		chunk_samples = MIN(chunk_samples, max_samples);
		for (channel = 0; channel < channels; channel++) {
			src_channel = src + channel;
			dst_channel = dst + channel;
			filter = &fir[channel];
			for (sample_index = 0; sample_index < chunk_samples;
			     sample_index += channels) {
				filtered_sample = fir_32x16(filter, *src_channel << 8);
				*dst_channel = sat_int24(Q_SHIFT_RND(filtered_sample, 31, 23));
				src_channel += channels;
				dst_channel += channels;
			}
		}
		remaining_samples -= chunk_samples;
		src = source_cir_buf_wrap(src + chunk_samples, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + chunk_samples, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
void eq_fir_s32(struct fir_state_32x16 fir[], const struct cir_buf_source *source,
		struct cir_buf_sink *sink, size_t frames, unsigned int channels)
{
	struct fir_state_32x16 *filter;
	const int32_t *src_channel;
	int32_t *dst_channel;
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t max_samples;
	size_t chunk_samples;
	size_t sample_index;
	size_t remaining_samples = frames * channels;
	unsigned int channel;

	while (remaining_samples) {
		max_samples = cir_buf_samples_without_wrap_s32(src, source->buf_end);
		chunk_samples = MIN(remaining_samples, max_samples);
		max_samples = cir_buf_samples_without_wrap_s32(dst, sink->buf_end);
		chunk_samples = MIN(chunk_samples, max_samples);
		for (channel = 0; channel < channels; channel++) {
			src_channel = src + channel;
			dst_channel = dst + channel;
			filter = &fir[channel];
			for (sample_index = 0; sample_index < chunk_samples;
			     sample_index += channels) {
				*dst_channel = fir_32x16(filter, *src_channel);
				src_channel += channels;
				dst_channel += channels;
			}
		}
		remaining_samples -= chunk_samples;
		src = source_cir_buf_wrap(src + chunk_samples, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + chunk_samples, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S32LE */
#endif /* SOF_USE_HIFI(NONE, FILTER) || SOF_USE_ARM_SIMD(FILTER) || SOF_USE_RISCV_SIMD(FILTER) */

#if CONFIG_FORMAT_FLOAT
void eq_fir_float(struct fir_state_32x16 fir[], const struct cir_buf_source *source,
		  struct cir_buf_sink *sink, size_t frames, unsigned int channels)
{
	struct fir_state_float *fir_f = (struct fir_state_float *)fir;
	struct fir_state_float *filter;
	const float *src_channel;
	float *dst_channel;
	const float *src = source->ptr;
	float *dst = sink->ptr;
	size_t max_samples;
	size_t chunk_samples;
	size_t sample_index;
	size_t remaining_samples = frames * channels;
	unsigned int channel;

	while (remaining_samples) {
		max_samples = cir_buf_samples_without_wrap_s32(src, source->buf_end);
		chunk_samples = MIN(remaining_samples, max_samples);
		max_samples = cir_buf_samples_without_wrap_s32(dst, sink->buf_end);
		chunk_samples = MIN(chunk_samples, max_samples);
		for (channel = 0; channel < channels; channel++) {
			src_channel = src + channel;
			dst_channel = dst + channel;
			filter = &fir_f[channel];
			for (sample_index = 0; sample_index < chunk_samples;
			     sample_index += channels) {
				*dst_channel = fir_float(filter, *src_channel);
				src_channel += channels;
				dst_channel += channels;
			}
		}
		remaining_samples -= chunk_samples;
		src = source_cir_buf_wrap(src + chunk_samples, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + chunk_samples, sink->buf_start, sink->buf_end);
	}
}

void eq_fir_2x_float(struct fir_state_32x16 fir[], const struct cir_buf_source *source,
		     struct cir_buf_sink *sink, size_t frames, unsigned int channels)
{
	struct fir_state_float *fir_f = (struct fir_state_float *)fir;
	struct fir_state_float *filter;
	const float *src = source->ptr;
	float *dst = sink->ptr;
	const float *x0, *x1;
	float *y0, *y1;
	size_t max_samples, chunk_samples, f;
	size_t remaining_frames = frames;
	unsigned int channel;
	size_t inc_2channels = 2 * channels;

	while (remaining_frames) {
		max_samples = cir_buf_samples_without_wrap_s32(src, source->buf_end);
		chunk_samples = MIN(remaining_frames * channels, max_samples);
		max_samples = cir_buf_samples_without_wrap_s32(dst, sink->buf_end);
		chunk_samples = MIN(chunk_samples, max_samples);
		f = chunk_samples / channels;

		if (f >= 2) {
			f &= ~0x1; /* Process even number of frames */
			for (channel = 0; channel < channels; channel++) {
				x0 = src + channel;
				x1 = x0 + channels;
				y0 = dst + channel;
				y1 = y0 + channels;
				filter = &fir_f[channel];
				for (size_t i = 0; i < f; i += 2) {
					fir_float_2x(filter, *x0, *x1, y0, y1);
					x0 += inc_2channels;
					x1 += inc_2channels;
					y0 += inc_2channels;
					y1 += inc_2channels;
				}
			}
			remaining_frames -= f;
			src = source_cir_buf_wrap(src + f * channels, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst + f * channels, sink->buf_start, sink->buf_end);
		} else if (f == 1) {
			/* Single frame fallback */
			for (channel = 0; channel < channels; channel++) {
				filter = &fir_f[channel];
				*(dst + channel) = fir_float(filter, *(src + channel));
			}
			remaining_frames -= 1;
			src = source_cir_buf_wrap(src + channels, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst + channels, sink->buf_start, sink->buf_end);
		} else {
			break;
		}
	}
}
#endif /* CONFIG_FORMAT_FLOAT */
