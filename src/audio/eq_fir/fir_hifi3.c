// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <sof/audio/eq_fir/fir_config.h>

#if FIR_HIFI3

#include <sof/audio/buffer.h>
#include <sof/audio/eq_fir/fir_hifi3.h>
#include <user/eq.h>
#include <xtensa/config/defs.h>
#include <xtensa/tie/xt_hifi3.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/*
 * EQ FIR algorithm code
 */

void fir_reset(struct fir_state_32x16 *fir)
{
	fir->taps = 0;
	fir->length = 0;
	fir->out_shift = 0;
	fir->coef = NULL;
	/* There may need to know the beginning of dynamic allocation after
	 * reset so omitting setting also fir->delay to NULL.
	 */
}

int fir_delay_size(struct sof_eq_fir_coef_data *config)
{
	/* Check FIR tap count for implementation specific constraints */
	if (config->length > SOF_EQ_FIR_MAX_LENGTH || config->length < 4)
		return -EINVAL;

	/* The optimization requires the tap count to be multiple of four */
	if (config->length & 0x3)
		return -EINVAL;

	/* The dual sample version needs one more delay entry. To preserve
	 * align for 64 bits need to add two.
	 */
	return (config->length + 2) * sizeof(int32_t);
}

int fir_init_coef(struct fir_state_32x16 *fir,
		  struct sof_eq_fir_coef_data *config)
{
	/* The length is taps plus two since the filter computes two
	 * samples per call. Length plus one would be minimum but the add
	 * must be even. The even length is needed for 64 bit loads from delay
	 * lines with 32 bit samples.
	 */
	fir->taps = (int)config->length;
	fir->length = fir->taps + 2;
	fir->out_shift = (int)config->out_shift;
	fir->coef = (ae_f16x4 *)&config->coef[0];
	return 0;
}

void fir_init_delay(struct fir_state_32x16 *fir, int32_t **data)
{
	fir->delay = (ae_int32 *) *data;
	fir->delay_end = fir->delay + fir->length;
	fir->rwp = (ae_int32 *)(fir->delay + fir->length - 1);
	*data += fir->length; /* Point to next delay line start */
}

void fir_get_lrshifts(struct fir_state_32x16 *fir, int *lshift,
		      int *rshift)
{
	*lshift = (fir->out_shift < 0) ? -fir->out_shift : 0;
	*rshift = (fir->out_shift > 0) ? fir->out_shift : 0;
}

#if CONFIG_FORMAT_S32LE
/* For even frame lengths use FIR filter that processes two sequential
 * sample per call.
 */
void eq_fir_2x_s32_hifi3(struct fir_state_32x16 fir[],
			 const struct audio_stream *source,
			 struct audio_stream *sink, int frames, int nch)
{
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

/* FIR for any number of frames */
void eq_fir_s32_hifi3(struct fir_state_32x16 fir[],
		      const struct audio_stream *source,
		      struct audio_stream *sink, int frames, int nch)
{
	struct fir_state_32x16 *f;
	ae_int32x2 in = 0;
	ae_int32 out;
	ae_int32 *x;
	ae_int32 *y;
	ae_int32 *src = (ae_int32 *)source->r_ptr;
	ae_int32 *snk = (ae_int32 *)sink->w_ptr;
	int ch;
	int i;
	int rshift;
	int lshift;
	int shift;
	int inc = nch * sizeof(int32_t);

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts to e.g. apply mute
		 * without overhead.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);
		shift = lshift - rshift;

		/* Copy src to x and advance src to next channel with
		 * dummy load.
		 */
		fir_comp_setup_circular(source);
		x = src;
		AE_L32_XC(in, src, sizeof(int32_t));

		/* Copy snk to y and advance snk to next channel with
		 * dummy load.
		 */
		fir_comp_setup_circular(sink);
		y = snk;
		AE_L32_XC(in, snk, sizeof(int32_t));

		for (i = 0; i < frames; i++) {
			/* Load input sample */
			fir_comp_setup_circular(source);
			AE_L32_XC(in, x, inc);

			/* Compute FIR */
			fir_core_setup_circular(f);
			fir_32x16_hifi3(f, in, &out, shift);

			/* Store output sample */
			fir_comp_setup_circular(sink);
			AE_S32_L_XC((ae_int32x2)out, y, inc);
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_2x_s24_hifi3(struct fir_state_32x16 fir[],
			 const struct audio_stream *source,
			 struct audio_stream *sink, int frames, int nch)
{
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
			d1 = AE_SRAI32R(z1, 8);

			/* Store output and update output pointers */
			fir_comp_setup_circular(sink);
			AE_S32_L_XC(d0, y, inc_nch_s);
			AE_S32_L_XC(d1, y, inc_nch_s);
		}
	}
}

void eq_fir_s24_hifi3(struct fir_state_32x16 fir[],
		      const struct audio_stream *source,
		      struct audio_stream *sink, int frames, int nch)
{
	struct fir_state_32x16 *f;
	ae_int32 in;
	ae_int32 out;
	ae_int32x2 d = 0;
	ae_int32 *x;
	ae_int32 *y;
	ae_int32 *src = (ae_int32 *)source->r_ptr;
	ae_int32 *snk = (ae_int32 *)sink->w_ptr;
	int ch;
	int i;
	int rshift;
	int lshift;
	int shift;
	int inc = nch * sizeof(int32_t);

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts to e.g. apply mute
		 * without overhead.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);
		shift = lshift - rshift;

		/* Copy src to x and advance src to next channel with
		 * dummy load.
		 */
		fir_comp_setup_circular(source);
		x = src;
		AE_L32_XC(d, src, sizeof(int32_t));

		/* Copy snk to y and advance snk to next channel with
		 * dummy load.
		 */
		fir_comp_setup_circular(sink);
		y = snk;
		AE_L32_XC(d, snk, sizeof(int32_t));

		for (i = 0; i < frames; i++) {
			/* Load input sample and convert with shift left
			 * to Q1.31 compatible format.
			 */
			fir_comp_setup_circular(source);
			AE_L32_XC(d, x, inc);
			in = AE_SLAA32(d, 8);

			/* Compute FIR */
			fir_core_setup_circular(f);
			fir_32x16_hifi3(f, in, &out, shift);

			/* Round to Q1.23 and store output sample */
			fir_comp_setup_circular(sink);
			d = AE_SRAI32R(out, 8);
			AE_S32_L_XC(d, y, inc);
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S16LE
void eq_fir_2x_s16_hifi3(struct fir_state_32x16 fir[],
			 const struct audio_stream *source,
			 struct audio_stream *sink, int frames, int nch)
{
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

void eq_fir_s16_hifi3(struct fir_state_32x16 fir[],
		      const struct audio_stream *source,
		      struct audio_stream *sink, int frames, int nch)
{
	struct fir_state_32x16 *f;
	ae_f16x4 d = AE_ZERO16();
	ae_int32 in;
	ae_int32 out;
	ae_int16 *x;
	ae_int16 *y;
	ae_int16 *src = (ae_int16 *)source->r_ptr;
	ae_int16 *snk = (ae_int16 *)sink->w_ptr;
	int ch;
	int i;
	int rshift;
	int lshift;
	int shift;
	int inc = nch * sizeof(int16_t);

	for (ch = 0; ch < nch; ch++) {
		/* Get FIR instance and get shifts to e.g. apply mute
		 * without overhead.
		 */
		f = &fir[ch];
		fir_get_lrshifts(f, &lshift, &rshift);
		shift = lshift - rshift;

		/* Copy src to x and advance src to next channel with
		 * dummy load.
		 */
		fir_comp_setup_circular(source);
		x = src;
		AE_L16_XC(d, src, sizeof(int16_t));

		/* Copy snk to y and advance snk to next channel with
		 * dummy load.
		 */
		fir_comp_setup_circular(sink);
		y = snk;
		AE_L16_XC(d, snk, sizeof(int16_t));

		for (i = 0; i < frames; i++) {
			/* Load input sample and convert to Q1.31 */
			fir_comp_setup_circular(source);
			AE_L16_XC(d, x, inc);
			in = AE_CVT32X2F16_32(d);

			/* Compute FIR */
			fir_core_setup_circular(f);
			fir_32x16_hifi3(f, in, &out, shift);

			/* Round to Q1.15 and store output sample */
			fir_comp_setup_circular(sink);
			d = AE_ROUND16X4F32SSYM(out, out);
			AE_S16_0_XC(d, y, inc);
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */
#endif
