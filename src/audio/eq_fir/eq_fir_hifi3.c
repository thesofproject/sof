// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/math/fir_config.h>

#if FIR_HIFI3

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/eq_fir/eq_fir.h>
#include <sof/math/fir_hifi3.h>
#include <user/fir.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi3.h>
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
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	struct fir_state_32x16 *f;
	ae_int32x2 d0 = 0;
	ae_int32x2 d1 = 0;
	ae_int32 *src = (ae_int32 *)source->r_ptr;
	ae_int32 *snk = (ae_int32 *)sink->w_ptr;
	ae_int32 *x;
	ae_int32 *y0;
	ae_int32 *y1;
	int ch;
	int i;
	int rshift;
	int lshift;
	int shift;
	int nch = source->channels;
	int inc_nch_s = nch * sizeof(int32_t);
	int inc_2nch_s = 2 * inc_nch_s;

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);
		shift = lshift - rshift;

		/* Copy src to x and advance src with dummy load */
		fir_comp_setup_circular(source);
		x = src;
		AE_L32_XC(d0, src, sizeof(int32_t));

		/* Copy snk to y0 and advance snk with dummy load. Pointer
		 * y1 is set to be ahead of y0 with one frame.
		 */
		fir_comp_setup_circular(sink);
		y0 = snk;
		y1 = snk;
		AE_L32_XC(d0, snk, sizeof(int32_t));
		AE_L32_XC(d1, y1, inc_nch_s);

		for (i = 0; i < (frames >> 1); i++) {
			/* Load two input samples via input pointer x */
			fir_comp_setup_circular(source);
			AE_L32_XC(d0, x, inc_nch_s);
			AE_L32_XC(d1, x, inc_nch_s);

			/* Compute FIR */
			fir_core_setup_circular(f);
			fir_32x16_2x_hifi3(f, d0, d1, y0, y1, shift);

			/* Update output pointers y0 and y1 with dummy loads */
			fir_comp_setup_circular(sink);
			AE_L32_XC(d0, y0, inc_2nch_s);
			AE_L32_XC(d1, y1, inc_2nch_s);
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_2x_s24(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		   struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	struct fir_state_32x16 *f;
	ae_int32x2 d0 = 0;
	ae_int32x2 d1 = 0;
	ae_int32 z0;
	ae_int32 z1;
	ae_int32 *src = (ae_int32 *)source->r_ptr;
	ae_int32 *snk = (ae_int32 *)sink->w_ptr;
	ae_int32 *x;
	ae_int32 *y;
	int ch;
	int i;
	int rshift;
	int lshift;
	int shift;
	int nch = source->channels;
	int inc_nch_s = nch * sizeof(int32_t);

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);
		shift = lshift - rshift;

		/* Copy src to x and advance src with dummy load */
		fir_comp_setup_circular(source);
		x = src;
		AE_L32_XC(d0, src, sizeof(int32_t));

		/* Copy snk to y0 and advance snk with dummy load. Pointer
		 * y1 is set to be ahead of y0 with one frame.
		 */
		fir_comp_setup_circular(sink);
		y = snk;
		AE_L32_XC(d0, snk, sizeof(int32_t));

		for (i = 0; i < (frames >> 1); i++) {
			/* Load two input samples via input pointer x */
			fir_comp_setup_circular(source);
			AE_L32_XC(d0, x, inc_nch_s);
			AE_L32_XC(d1, x, inc_nch_s);

			/* Convert Q1.23 to Q1.31 compatible format */
			d0 = AE_SLAA32(d0, 8);
			d1 = AE_SLAA32(d1, 8);

			/* Compute FIR */
			fir_core_setup_circular(f);
			fir_32x16_2x_hifi3(f, d0, d1, &z0, &z1, shift);

			/* Shift and round to Q1.23 format */
			d0 = AE_SRAI32R(z0, 8);
			d0 = AE_SLAI32S(d0, 8);
			d0 = AE_SRAI32(d0, 8);

			d1 = AE_SRAI32R(z1, 8);
			d1 = AE_SLAI32S(d1, 8);
			d1 = AE_SRAI32(d1, 8);

			/* Store output and update output pointers */
			fir_comp_setup_circular(sink);
			AE_S32_L_XC(d0, y, inc_nch_s);
			AE_S32_L_XC(d1, y, inc_nch_s);
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S16LE
void eq_fir_2x_s16(struct fir_state_32x16 fir[], struct input_stream_buffer *bsource,
		   struct output_stream_buffer *bsink, int frames)
{
	struct audio_stream __sparse_cache *source = bsource->data;
	struct audio_stream __sparse_cache *sink = bsink->data;
	struct fir_state_32x16 *f;
	ae_int16x4 d0 = AE_ZERO16();
	ae_int16x4 d1 = AE_ZERO16();
	ae_int32 z0;
	ae_int32 z1;
	ae_int32 x0;
	ae_int32 x1;
	ae_int16 *src = (ae_int16 *)source->r_ptr;
	ae_int16 *snk = (ae_int16 *)sink->w_ptr;
	ae_int16 *x;
	ae_int16 *y;
	int ch;
	int i;
	int rshift;
	int lshift;
	int shift;
	int nch = source->channels;
	int inc_nch_s = nch * sizeof(int16_t);

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);
		shift = lshift - rshift;

		/* Copy src to x and advance src to next channel with
		 * dummy load.
		 */
		fir_comp_setup_circular(source);
		x = src;
		AE_L16_XC(d0, src, sizeof(int16_t));

		/* Copy pointer snk to y0 and advance snk with dummy load.
		 * Pointer y1 is set to be ahead of y0 with one frame.
		 */
		fir_comp_setup_circular(sink);
		y = snk;
		AE_L16_XC(d0, snk, sizeof(int16_t));

		for (i = 0; i < (frames >> 1); i++) {
			/* Load two input samples via input pointer x */
			fir_comp_setup_circular(source);
			AE_L16_XC(d0, x, inc_nch_s);
			AE_L16_XC(d1, x, inc_nch_s);

			/* Convert Q1.15 to Q1.31 compatible format */
			x0 = AE_CVT32X2F16_32(d0);
			x1 = AE_CVT32X2F16_32(d1);

			/* Compute FIR */
			fir_core_setup_circular(f);
			fir_32x16_2x_hifi3(f, x0, x1, &z0, &z1, shift);

			/* Round to Q1.15 format */
			d0 = AE_ROUND16X4F32SSYM(z0, z0);
			d1 = AE_ROUND16X4F32SSYM(z1, z1);

			/* Store output and update output pointers */
			fir_comp_setup_circular(sink);
			AE_S16_0_XC(d0, y, inc_nch_s);
			AE_S16_0_XC(d1, y, inc_nch_s);
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#endif /* FIR_HIFI3 */
