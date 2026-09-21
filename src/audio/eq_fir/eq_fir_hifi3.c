// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_MIN_HIFI(3, FILTER)

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/fir_hifi3.h>
#include <user/fir.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi3.h>
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
	ae_int32x2 d0 = 0;
	ae_int32x2 d1 = 0;
	ae_int32 *src = (ae_int32 *)source->ptr;
	ae_int32 *dst = (ae_int32 *)sink->ptr;
	ae_int32 *src_channel;
	ae_int32 *dst_first;
	ae_int32 *dst_second;
	int channel;
	int pair_index;
	int rshift;
	int lshift;
	int shift;
	int channel_count = channels;
	int channel_stride_bytes = channel_count * sizeof(int32_t);
	int pair_stride_bytes = 2 * channel_stride_bytes;
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
				shift = lshift - rshift;
				fir_core_setup_circular(filter);
				fir_32x16(filter, src[channel], dst + channel, shift);
			}
			src = (ae_int32 *)source_cir_buf_wrap(src + channel_count,
							      source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst + channel_count, sink->buf_start, sink->buf_end);
			remaining_frames--;
			continue;
		}

		for (channel = 0; channel < channel_count; channel++) {
			/* Get FIR instance and get shifts.*/
			filter = &fir[channel];
			fir_get_lrshifts(filter, &lshift, &rshift);
			shift = lshift - rshift;
			/* Set filter->delay as circular buffer. */
			fir_core_setup_circular(filter);

			src_channel = src + channel;
			dst_first = dst + channel;
			dst_second = dst_first + channel_count;

			for (pair_index = 0; pair_index < (chunk_frames >> 1); pair_index++) {
				/* Load two input samples via the channel source pointer. */
				AE_L32_XP(d0, src_channel, channel_stride_bytes);
				AE_L32_XP(d1, src_channel, channel_stride_bytes);
				fir_32x16_2x(filter, d0, d1, dst_first, dst_second, shift);
				AE_L32_XC(d0, dst_first, pair_stride_bytes);
				AE_L32_XC(d1, dst_second, pair_stride_bytes);
			}
		}
		dst = cir_buf_wrap(dst + chunk_frames * channel_count, sink->buf_start,
				   sink->buf_end);
		src = (ae_int32 *)source_cir_buf_wrap(src + chunk_frames * channel_count,
						      source->buf_start, source->buf_end);
		remaining_frames -= chunk_frames;
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_2x_s24(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		   struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *filter;
	ae_int32x2 d0 = 0;
	ae_int32x2 d1 = 0;
	ae_int32 filtered_sample_0;
	ae_int32 filtered_sample_1;
	ae_int32 *src = (ae_int32 *)source->ptr;
	ae_int32 *dst = (ae_int32 *)sink->ptr;
	ae_int32 *src_channel;
	ae_int32 *dst_channel;
	int channel;
	int pair_index;
	int rshift;
	int lshift;
	int shift;
	int channel_count = channels;
	int channel_stride_bytes = channel_count * sizeof(int32_t);
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
				ae_int32 input = src[channel] << 8;
				int32_t output;

				filter = &fir[channel];
				fir_get_lrshifts(filter, &lshift, &rshift);
				shift = lshift - rshift;
				fir_core_setup_circular(filter);
				fir_32x16(filter, input, &output, shift);
				dst[channel] = sat_int24(Q_SHIFT_RND(output, 31, 23));
			}
			src = (ae_int32 *)source_cir_buf_wrap(src + channel_count,
							      source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst + channel_count, sink->buf_start, sink->buf_end);
			remaining_frames--;
			continue;
		}

		for (channel = 0; channel < channel_count; channel++) {
			/* Get FIR instance and get shifts.*/
			filter = &fir[channel];
			fir_get_lrshifts(filter, &lshift, &rshift);
			shift = lshift - rshift;
			/* Set filter->delay as circular buffer. */
			fir_core_setup_circular(filter);

			src_channel = src + channel;
			dst_channel = dst + channel;

			for (pair_index = 0; pair_index < (chunk_frames >> 1); pair_index++) {
				/* Load two input samples via the channel source pointer. */
				AE_L32_XP(d0, src_channel, channel_stride_bytes);
				AE_L32_XP(d1, src_channel, channel_stride_bytes);

				/* Convert Q1.23 to Q1.31 compatible format */
				d0 = AE_SLAA32(d0, 8);
				d1 = AE_SLAA32(d1, 8);

				fir_32x16_2x(filter, d0, d1, &filtered_sample_0, &filtered_sample_1,
					     shift);

				/* Shift and round to Q1.23 format */
				d0 = AE_SRAI32R(filtered_sample_0, 8);
				d0 = AE_SLAI32S(d0, 8);
				d0 = AE_SRAI32(d0, 8);

				d1 = AE_SRAI32R(filtered_sample_1, 8);
				d1 = AE_SLAI32S(d1, 8);
				d1 = AE_SRAI32(d1, 8);

				/* Store output and update output pointers */
				AE_S32_L_XC(d0, dst_channel, channel_stride_bytes);
				AE_S32_L_XC(d1, dst_channel, channel_stride_bytes);
			}
		}
		dst = cir_buf_wrap(dst + chunk_frames * channel_count, sink->buf_start,
				   sink->buf_end);
		src = (ae_int32 *)source_cir_buf_wrap(src + chunk_frames * channel_count,
						      source->buf_start, source->buf_end);
		remaining_frames -= chunk_frames;
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S16LE
void eq_fir_2x_s16(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		   struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *filter;
	ae_int16x4 d0 = AE_ZERO16();
	ae_int16x4 d1 = AE_ZERO16();
	ae_int32 filtered_sample_0;
	ae_int32 filtered_sample_1;
	ae_int32 input_sample_0;
	ae_int32 input_sample_1;
	ae_int16 *src = (ae_int16 *)source->ptr;
	ae_int16 *dst = (ae_int16 *)sink->ptr;
	ae_int16 *src_channel;
	ae_int16 *dst_channel;
	int channel;
	int pair_index;
	int rshift;
	int lshift;
	int shift;
	int channel_count = channels;
	int channel_stride_bytes = channel_count * sizeof(int16_t);
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
				int32_t input = ((int16_t *)src)[channel] << 16;
				int32_t output;

				filter = &fir[channel];
				fir_get_lrshifts(filter, &lshift, &rshift);
				shift = lshift - rshift;
				fir_core_setup_circular(filter);
				fir_32x16(filter, input, &output, shift);
				dst[channel] = sat_int16(Q_SHIFT_RND(output, 31, 15));
			}
			src = (ae_int16 *)source_cir_buf_wrap(src + channel_count,
							      source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst + channel_count, sink->buf_start, sink->buf_end);
			remaining_frames--;
			continue;
		}

		for (channel = 0; channel < channel_count; channel++) {
			/* Get FIR instance and get shifts.*/
			filter = &fir[channel];
			fir_get_lrshifts(filter, &lshift, &rshift);
			shift = lshift - rshift;
			/* Set filter->delay as circular buffer. */
			fir_core_setup_circular(filter);

			src_channel = src + channel;
			dst_channel = dst + channel;

			for (pair_index = 0; pair_index < (chunk_frames >> 1); pair_index++) {
				/* Load two input samples via the channel source pointer. */
				AE_L16_XP(d0, src_channel, channel_stride_bytes);
				AE_L16_XP(d1, src_channel, channel_stride_bytes);

				/* Convert Q1.15 to Q1.31 compatible format */
				input_sample_0 = AE_CVT32X2F16_32(d0);
				input_sample_1 = AE_CVT32X2F16_32(d1);

				fir_32x16_2x(filter, input_sample_0, input_sample_1,
					     &filtered_sample_0, &filtered_sample_1, shift);

				/* Round to Q1.15 format */
				d0 = AE_ROUND16X4F32SSYM(filtered_sample_0, filtered_sample_0);
				d1 = AE_ROUND16X4F32SSYM(filtered_sample_1, filtered_sample_1);

				/* Store output and update output pointers */
				AE_S16_0_XC(d0, dst_channel, channel_stride_bytes);
				AE_S16_0_XC(d1, dst_channel, channel_stride_bytes);
			}
		}
		dst = cir_buf_wrap(dst + chunk_frames * channel_count, sink->buf_start,
				   sink->buf_end);
		src = (ae_int16 *)source_cir_buf_wrap(src + chunk_frames * channel_count,
						      source->buf_start, source->buf_end);
		remaining_frames -= chunk_frames;
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#endif /* FIR_HIFI3 */
