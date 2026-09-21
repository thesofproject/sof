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
	struct fir_state_32x16 *f;
	ae_int32x2 d0 = 0;
	ae_int32x2 d1 = 0;
	ae_int32 *src = (ae_int32 *)source->ptr;
	ae_int32 *dst = (ae_int32 *)sink->ptr;
	ae_int32 *x;
	ae_int32 *y0;
	ae_int32 *y1;
	int ch;
	int i;
	int rshift;
	int lshift;
	int shift;
	int nch = channels;
	int inc_nch_s = nch * sizeof(int32_t);
	int inc_2nch_s = 2 * inc_nch_s;
	int remaining_frames = frames;

	while (remaining_frames) {
		int source_frames = cir_buf_samples_without_wrap_s32(src, source->buf_end) / nch;
		int sink_frames = cir_buf_samples_without_wrap_s32(dst, sink->buf_end) / nch;
		int chunk_frames = MIN(remaining_frames, MIN(source_frames, sink_frames));

		chunk_frames &= ~0x1;
		if (!chunk_frames) {
			for (ch = 0; ch < nch; ch++) {
				f = &fir[ch];
				fir_get_lrshifts(f, &lshift, &rshift);
				shift = lshift - rshift;
				fir_core_setup_circular(f);
				fir_32x16(f, src[ch], dst + ch, shift);
			}
			src = (ae_int32 *)source_cir_buf_wrap(src + nch, source->buf_start,
							     source->buf_end);
			dst = cir_buf_wrap(dst + nch, sink->buf_start, sink->buf_end);
			remaining_frames--;
			continue;
		}

		for (ch = 0; ch < nch; ch++) {
			/* Get FIR instance and get shifts.*/
			f = &fir[ch];
			fir_get_lrshifts(f, &lshift, &rshift);
			shift = lshift - rshift;
			/* set f->delay as circular buffer */
			fir_core_setup_circular(f);

			x = src + ch;
			y0 = dst + ch;
			y1 = y0 + nch;

			for (i = 0; i < (chunk_frames >> 1); i++) {
				/* Load two input samples via input pointer x */
				AE_L32_XP(d0, x, inc_nch_s);
				AE_L32_XP(d1, x, inc_nch_s);
				fir_32x16_2x(f, d0, d1, y0, y1, shift);
				AE_L32_XC(d0, y0, inc_2nch_s);
				AE_L32_XC(d1, y1, inc_2nch_s);
			}
		}
		dst = cir_buf_wrap(dst + chunk_frames * nch,
				   sink->buf_start, sink->buf_end);
		src = (ae_int32 *)source_cir_buf_wrap(src + chunk_frames * nch,
						      source->buf_start, source->buf_end);
		remaining_frames -= chunk_frames;
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_2x_s24(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		   struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *f;
	ae_int32x2 d0 = 0;
	ae_int32x2 d1 = 0;
	ae_int32 z0;
	ae_int32 z1;
	ae_int32 *src = (ae_int32 *)source->ptr;
	ae_int32 *dst = (ae_int32 *)sink->ptr;
	ae_int32 *x;
	ae_int32 *y;
	int ch;
	int i;
	int rshift;
	int lshift;
	int shift;
	int nch = channels;
	int inc_nch_s = nch * sizeof(int32_t);
	int remaining_frames = frames;

	while (remaining_frames) {
		int source_frames = cir_buf_samples_without_wrap_s32(src, source->buf_end) / nch;
		int sink_frames = cir_buf_samples_without_wrap_s32(dst, sink->buf_end) / nch;
		int chunk_frames = MIN(remaining_frames, MIN(source_frames, sink_frames));

		chunk_frames &= ~0x1;
		if (!chunk_frames) {
			for (ch = 0; ch < nch; ch++) {
				int32_t input = ((int32_t *)src)[ch] << 8;
				int32_t output;

				f = &fir[ch];
				fir_get_lrshifts(f, &lshift, &rshift);
				shift = lshift - rshift;
				fir_core_setup_circular(f);
				fir_32x16(f, input, (ae_int32 *)&output, shift);
				dst[ch] = sat_int24(Q_SHIFT_RND(output, 31, 23));
			}
			src = (ae_int32 *)source_cir_buf_wrap(src + nch, source->buf_start,
							     source->buf_end);
			dst = cir_buf_wrap(dst + nch, sink->buf_start, sink->buf_end);
			remaining_frames--;
			continue;
		}

		for (ch = 0; ch < nch; ch++) {
			/* Get FIR instance and get shifts.*/
			f = &fir[ch];
			fir_get_lrshifts(f, &lshift, &rshift);
			shift = lshift - rshift;
			/* set f->delay as circular buffer */
			fir_core_setup_circular(f);

			x = src + ch;
			y = dst + ch;

			for (i = 0; i < (chunk_frames >> 1); i++) {
				/* Load two input samples via input pointer x */
				AE_L32_XP(d0, x, inc_nch_s);
				AE_L32_XP(d1, x, inc_nch_s);

				/* Convert Q1.23 to Q1.31 compatible format */
				d0 = AE_SLAA32(d0, 8);
				d1 = AE_SLAA32(d1, 8);

				fir_32x16_2x(f, d0, d1,  &z0, &z1, shift);

				/* Shift and round to Q1.23 format */
				d0 = AE_SRAI32R(z0, 8);
				d0 = AE_SLAI32S(d0, 8);
				d0 = AE_SRAI32(d0, 8);

				d1 = AE_SRAI32R(z1, 8);
				d1 = AE_SLAI32S(d1, 8);
				d1 = AE_SRAI32(d1, 8);

				/* Store output and update output pointers */
				AE_S32_L_XC(d0, y, inc_nch_s);
				AE_S32_L_XC(d1, y, inc_nch_s);
			}
		}
		dst = cir_buf_wrap(dst + chunk_frames * nch,
				   sink->buf_start, sink->buf_end);
		src = (ae_int32 *)source_cir_buf_wrap(src + chunk_frames * nch,
						      source->buf_start, source->buf_end);
		remaining_frames -= chunk_frames;
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S16LE
void eq_fir_2x_s16(struct fir_state_32x16 fir[], struct cir_buf_source *source,
		   struct cir_buf_sink *sink, int frames, int channels)
{
	struct fir_state_32x16 *f;
	ae_int16x4 d0 = AE_ZERO16();
	ae_int16x4 d1 = AE_ZERO16();
	ae_int32 z0;
	ae_int32 z1;
	ae_int32 x0;
	ae_int32 x1;
	ae_int16 *src = (ae_int16 *)source->ptr;
	ae_int16 *dst = (ae_int16 *)sink->ptr;
	ae_int16 *x;
	ae_int16 *y;
	int ch;
	int i;
	int rshift;
	int lshift;
	int shift;
	int nch = channels;
	int inc_nch_s = nch * sizeof(int16_t);
	int remaining_frames = frames;

	while (remaining_frames) {
		int source_frames = cir_buf_samples_without_wrap_s16(src, source->buf_end) / nch;
		int sink_frames = cir_buf_samples_without_wrap_s16(dst, sink->buf_end) / nch;
		int chunk_frames = MIN(remaining_frames, MIN(source_frames, sink_frames));

		chunk_frames &= ~0x1;
		if (!chunk_frames) {
			for (ch = 0; ch < nch; ch++) {
				int32_t input = ((int16_t *)src)[ch] << 16;
				int32_t output;

				f = &fir[ch];
				fir_get_lrshifts(f, &lshift, &rshift);
				shift = lshift - rshift;
				fir_core_setup_circular(f);
				fir_32x16(f, input, (ae_int32 *)&output, shift);
				dst[ch] = sat_int16(Q_SHIFT_RND(output, 31, 15));
			}
			src = (ae_int16 *)source_cir_buf_wrap(src + nch, source->buf_start,
							     source->buf_end);
			dst = cir_buf_wrap(dst + nch, sink->buf_start, sink->buf_end);
			remaining_frames--;
			continue;
		}

		for (ch = 0; ch < nch; ch++) {
			/* Get FIR instance and get shifts.*/
			f = &fir[ch];
			fir_get_lrshifts(f, &lshift, &rshift);
			shift = lshift - rshift;
			/* set f->delay as circular buffer */
			fir_core_setup_circular(f);

			x = src + ch;
			y = dst + ch;

			for (i = 0; i < (chunk_frames >> 1); i++) {
				/* Load two input samples via input pointer x */
				AE_L16_XP(d0, x, inc_nch_s);
				AE_L16_XP(d1, x, inc_nch_s);

				/* Convert Q1.15 to Q1.31 compatible format */
				x0 = AE_CVT32X2F16_32(d0);
				x1 = AE_CVT32X2F16_32(d1);

				fir_32x16_2x(f, x0, x1,  &z0, &z1, shift);

				/* Round to Q1.15 format */
				d0 = AE_ROUND16X4F32SSYM(z0, z0);
				d1 = AE_ROUND16X4F32SSYM(z1, z1);

				/* Store output and update output pointers */
				AE_S16_0_XC(d0, y, inc_nch_s);
				AE_S16_0_XC(d1, y, inc_nch_s);
			}
		}
		dst = cir_buf_wrap(dst + chunk_frames * nch,
				   sink->buf_start, sink->buf_end);
		src = (ae_int16 *)source_cir_buf_wrap(src + chunk_frames * nch,
						      source->buf_start, source->buf_end);
		remaining_frames -= chunk_frames;
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#endif /* FIR_HIFI3 */
