// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_HIFI(2, FILTER)

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/math/fir_hifi2ep.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi2.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "eq_fir.h"

LOG_MODULE_DECLARE(eq_fir, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_FORMAT_S32LE
/* For even frame lengths use FIR filter that processes two sequential
 * sample per call.
 */
void eq_fir_2x_s32(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		   struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *filter;
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	const int32_t *src_first;
	int32_t *dst_first;
	const int32_t *src_second;
	int32_t *dst_second;
	int channel;
	int pair_index;
	int rshift;
	int lshift;
	int channel_count = channels;
	int remaining_frames = frames;

	while (remaining_frames) {
		int source_frames =
			cir_buf_samples_without_wrap_s32(src, source->buf_end) / channel_count;
		int sink_frames =
			cir_buf_samples_without_wrap_s32(dst, sink->buf_end) / channel_count;
		int chunk_frames = MIN(remaining_frames, MIN(source_frames, sink_frames));

		chunk_frames &= ~0x1;
		if (!chunk_frames) {
			for (channel = 0; channel < channel_count; channel++) {
				filter = &fir[channel];
				fir_get_lrshifts(filter, &lshift, &rshift);
				fir_hifiep_setup_circular(filter);
				dst_first = dst + channel;
				fir_32x16(filter, src[channel], dst_first, lshift, rshift);
			}
			src = source_cir_buf_wrap(src + channel_count, source->buf_start,
						  source->buf_end);
			dst = cir_buf_wrap(dst + channel_count, sink->buf_start, sink->buf_end);
			remaining_frames--;
			continue;
		}

		for (channel = 0; channel < channel_count; channel++) {
			/* Get FIR instance and get shifts to e.g. apply mute
			 * without overhead.
			 */
			filter = &fir[channel];
			fir_get_lrshifts(filter, &lshift, &rshift);

			/* Setup circular buffer for FIR input data delay */
			fir_hifiep_setup_circular(filter);

			src_first = src + channel;
			dst_first = dst + channel;
			for (pair_index = 0; pair_index < (chunk_frames >> 1); pair_index++) {
				src_second = src_first + channel_count;
				dst_second = dst_first + channel_count;
				fir_32x16_2x(filter, *src_first, *src_second, dst_first, dst_second,
					     lshift, rshift);
				src_first += 2 * channel_count;
				dst_first += 2 * channel_count;
			}
		}

		src = source_cir_buf_wrap(src + chunk_frames * channel_count, source->buf_start,
					  source->buf_end);
		dst = cir_buf_wrap(dst + chunk_frames * channel_count, sink->buf_start,
				   sink->buf_end);
		remaining_frames -= chunk_frames;
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_2x_s24(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		   struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *filter;
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	const int32_t *src_first;
	int32_t *dst_first;
	const int32_t *src_second;
	int32_t *dst_second;
	int32_t filtered_sample_0;
	int32_t filtered_sample_1;
	int channel;
	int pair_index;
	int rshift;
	int lshift;
	int channel_count = channels;
	int remaining_frames = frames;

	while (remaining_frames) {
		int source_frames =
			cir_buf_samples_without_wrap_s32(src, source->buf_end) / channel_count;
		int sink_frames =
			cir_buf_samples_without_wrap_s32(dst, sink->buf_end) / channel_count;
		int chunk_frames = MIN(remaining_frames, MIN(source_frames, sink_frames));

		chunk_frames &= ~0x1;
		if (!chunk_frames) {
			for (channel = 0; channel < channel_count; channel++) {
				int32_t filtered_sample;

				filter = &fir[channel];
				fir_get_lrshifts(filter, &lshift, &rshift);
				fir_hifiep_setup_circular(filter);
				fir_32x16(filter, src[channel] << 8, &filtered_sample, lshift,
					  rshift);
				dst[channel] = sat_int24(Q_SHIFT_RND(filtered_sample, 31, 23));
			}
			src = source_cir_buf_wrap(src + channel_count, source->buf_start,
						  source->buf_end);
			dst = cir_buf_wrap(dst + channel_count, sink->buf_start, sink->buf_end);
			remaining_frames--;
			continue;
		}

		for (channel = 0; channel < channel_count; channel++) {
			/* Get FIR instance and get shifts to e.g. apply mute
			 * without overhead.
			 */
			filter = &fir[channel];
			fir_get_lrshifts(filter, &lshift, &rshift);

			/* Setup circular buffer for FIR input data delay */
			fir_hifiep_setup_circular(filter);

			src_first = src + channel;
			dst_first = dst + channel;
			for (pair_index = 0; pair_index < (chunk_frames >> 1); pair_index++) {
				src_second = src_first + channel_count;
				dst_second = dst_first + channel_count;
				fir_32x16_2x(filter, *src_first << 8, *src_second << 8,
					     &filtered_sample_0, &filtered_sample_1, lshift,
					     rshift);
				*dst_first = sat_int24(Q_SHIFT_RND(filtered_sample_0, 31, 23));
				*dst_second = sat_int24(Q_SHIFT_RND(filtered_sample_1, 31, 23));
				src_first += 2 * channel_count;
				dst_first += 2 * channel_count;
			}
		}

		src = source_cir_buf_wrap(src + chunk_frames * channel_count, source->buf_start,
					  source->buf_end);
		dst = cir_buf_wrap(dst + chunk_frames * channel_count, sink->buf_start,
				   sink->buf_end);
		remaining_frames -= chunk_frames;
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S16LE
void eq_fir_2x_s16(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		   struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *filter;
	const int16_t *src = source->ptr;
	int16_t *dst = sink->ptr;
	const int16_t *src_first;
	int16_t *dst_first;
	const int16_t *src_second;
	int16_t *dst_second;
	int32_t filtered_sample_0;
	int32_t filtered_sample_1;
	int channel;
	int pair_index;
	int rshift;
	int lshift;
	int channel_count = channels;
	int remaining_frames = frames;

	while (remaining_frames) {
		int source_frames =
			cir_buf_samples_without_wrap_s16(src, source->buf_end) / channel_count;
		int sink_frames =
			cir_buf_samples_without_wrap_s16(dst, sink->buf_end) / channel_count;
		int chunk_frames = MIN(remaining_frames, MIN(source_frames, sink_frames));

		chunk_frames &= ~0x1;
		if (!chunk_frames) {
			for (channel = 0; channel < channel_count; channel++) {
				int32_t filtered_sample;

				filter = &fir[channel];
				fir_get_lrshifts(filter, &lshift, &rshift);
				fir_hifiep_setup_circular(filter);
				fir_32x16(filter, src[channel] << 16, &filtered_sample, lshift,
					  rshift);
				dst[channel] = sat_int16(Q_SHIFT_RND(filtered_sample, 31, 15));
			}
			src = source_cir_buf_wrap(src + channel_count, source->buf_start,
						  source->buf_end);
			dst = cir_buf_wrap(dst + channel_count, sink->buf_start, sink->buf_end);
			remaining_frames--;
			continue;
		}

		for (channel = 0; channel < channel_count; channel++) {
			/* Get FIR instance and get shifts to e.g. apply mute
			 * without overhead.
			 */
			filter = &fir[channel];
			fir_get_lrshifts(filter, &lshift, &rshift);

			/* Setup circular buffer for FIR input data delay */
			fir_hifiep_setup_circular(filter);

			src_first = src + channel;
			dst_first = dst + channel;
			for (pair_index = 0; pair_index < (chunk_frames >> 1); pair_index++) {
				src_second = src_first + channel_count;
				dst_second = dst_first + channel_count;
				fir_32x16_2x(filter, *src_first << 16, *src_second << 16,
					     &filtered_sample_0, &filtered_sample_1, lshift,
					     rshift);
				*dst_first = sat_int16(Q_SHIFT_RND(filtered_sample_0, 31, 15));
				*dst_second = sat_int16(Q_SHIFT_RND(filtered_sample_1, 31, 15));
				src_first += 2 * channel_count;
				dst_first += 2 * channel_count;
			}
		}

		src = source_cir_buf_wrap(src + chunk_frames * channel_count, source->buf_start,
					  source->buf_end);
		dst = cir_buf_wrap(dst + chunk_frames * channel_count, sink->buf_start,
				   sink->buf_end);
		remaining_frames -= chunk_frames;
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#endif /* FIR_HIFIEP */
