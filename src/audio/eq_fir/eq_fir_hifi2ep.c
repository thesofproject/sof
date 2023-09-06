// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/math/fir_config.h>

#if FIR_HIFIEP

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/eq_fir/eq_fir.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/math/fir_hifi2ep.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi2.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(eq_fir, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_FORMAT_S32LE
/* For even frame lengths use FIR filter that processes two sequential
 * sample per call.
 */
void eq_fir_2x_s32(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		   struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct fir_state_32x16 *f;
	int32_t *src = audio_stream_get_rptr(source);
	int32_t *snk = audio_stream_get_wptr(sink);
	int32_t *x0;
	int32_t *y0;
	int32_t *x1;
	int32_t *y1;
	int ch;
	int i;
	int rshift;
	int lshift;
	int nch = audio_stream_get_channels(source);
	int inc = nch << 1;

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts to e.g. apply mute
		 * without overhead.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);

		/* Setup circular buffer for FIR input data delay */
		fir_hifiep_setup_circular(f);

		x0 = src++;
		y0 = snk++;
		for (i = 0; i < (frames >> 1); i++) {
			x1 = x0 + nch;
			y1 = y0 + nch;
			fir_32x16_2x_hifiep(f, *x0, *x1, y0, y1,
					    lshift, rshift);
			x0 += inc;
			y0 += inc;
		}
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
	int32_t *snk = audio_stream_get_wptr(sink);
	int32_t *x0;
	int32_t *y0;
	int32_t *x1;
	int32_t *y1;
	int32_t z0;
	int32_t z1;
	int ch;
	int i;
	int rshift;
	int lshift;
	int nch = audio_stream_get_channels(source);
	int inc = nch << 1;

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts to e.g. apply mute
		 * without overhead.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);

		/* Setup circular buffer for FIR input data delay */
		fir_hifiep_setup_circular(f);

		x0 = src++;
		y0 = snk++;
		for (i = 0; i < (frames >> 1); i++) {
			x1 = x0 + nch;
			y1 = y0 + nch;
			fir_32x16_2x_hifiep(f, *x0 << 8, *x1 << 8, &z0, &z1,
					    lshift, rshift);
			*y0 = sat_int24(Q_SHIFT_RND(z0, 31, 23));
			*y1 = sat_int24(Q_SHIFT_RND(z1, 31, 23));
			x0 += inc;
			y0 += inc;
		}
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
	int16_t *snk = audio_stream_get_wptr(sink);
	int16_t *x0;
	int16_t *y0;
	int16_t *x1;
	int16_t *y1;
	int32_t z0;
	int32_t z1;
	int ch;
	int i;
	int rshift;
	int lshift;
	int nch = audio_stream_get_channels(source);
	int inc = nch << 1;

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts to e.g. apply mute
		 * without overhead.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);

		/* Setup circular buffer for FIR input data delay */
		fir_hifiep_setup_circular(f);

		x0 = src++;
		y0 = snk++;
		for (i = 0; i < (frames >> 1); i++) {
			x1 = x0 + nch;
			y1 = y0 + nch;
			fir_32x16_2x_hifiep(f, *x0 << 16, *x1 << 16, &z0, &z1,
					    lshift, rshift);
			*y0 = sat_int16(Q_SHIFT_RND(z0, 31, 15));
			*y1 = sat_int16(Q_SHIFT_RND(z1, 31, 15));
			x0 += inc;
			y0 += inc;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#endif /* FIR_HIFIEP */
