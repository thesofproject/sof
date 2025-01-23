// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <sof/common.h>

#include "mixin_mixout.h"

#if SOF_USE_HIFI(3, MIXIN_MIXOUT) || SOF_USE_HIFI(4, MIXIN_MIXOUT)

#if CONFIG_FORMAT_S16LE
static void mix_s16(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
		    const struct cir_buf_ptr *source,
		    int32_t sample_count, uint16_t gain)
{
	int samples_to_mix, samples_to_copy, left_samples;
	int n, nmax, i, m, left;
	ae_int16x4 in_sample;
	ae_int16x4 out_sample;
	ae_int16x4 *in;
	ae_int16x4 *out;
	ae_valign inu;
	ae_valign outu1;
	ae_valign outu2 = AE_ZALIGN64();
	/* cir_buf_wrap() is required and is done below in a loop */
	ae_int16 *dst = (ae_int16 *)sink->ptr + start_sample;
	ae_int16 *src = source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = AE_MIN_32_signed(mixed_samples - start_sample, sample_count);
	samples_to_copy = sample_count - samples_to_mix;
	n = 0;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (ae_int16 *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (ae_int16 *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int16x4 *)src;
		out = (ae_int16x4 *)dst;
		inu = AE_LA64_PP(in);
		outu1 = AE_LA64_PP(out);
		m = n >> 2;
		left = n & 0x03;
		/* process 4 samples per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4_IP(in_sample, inu, in);
			AE_LA16X4_IP(out_sample, outu1, out);
			out--;
			out_sample = AE_ADD16S(in_sample, out_sample);
			AE_SA16X4_IP(out_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			AE_L16_IP(in_sample, (ae_int16 *)in, sizeof(ae_int16));
			AE_L16_IP(out_sample, (ae_int16 *)out, 0);
			out_sample = AE_ADD16S(in_sample, out_sample);
			AE_S16_0_IP(out_sample, (ae_int16 *)out, sizeof(ae_int16));
		}
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (ae_int16 *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (ae_int16 *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int16x4 *)src;
		out = (ae_int16x4 *)dst;
		inu = AE_LA64_PP(in);
		m = n >> 2;
		left = n & 0x03;
		/* process 4 samples per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4_IP(in_sample, inu, in);
			AE_SA16X4_IP(in_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			AE_L16_IP(in_sample, (ae_int16 *)in, sizeof(ae_int16));
			AE_S16_0_IP(in_sample, (ae_int16 *)out, sizeof(ae_int16));
		}
	}
}

static void mix_s16_gain(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
			 const struct cir_buf_ptr *source,
			 int32_t sample_count, uint16_t gain)
{
	int samples_to_mix, samples_to_copy, left_samples;
	int n, nmax, i, m, left;
	ae_int16x4 in_sample;
	ae_int16x4 out_sample;
	ae_int16x4 *in;
	ae_int16x4 *out;
	ae_valign inu;
	ae_valign outu1;
	ae_valign outu2 = AE_ZALIGN64();
	/* cir_buf_wrap() is required and is done below in a loop */
	ae_int16 *dst = (ae_int16 *)sink->ptr + start_sample;
	ae_int16 *src = source->ptr;
	ae_f16x4 gain_vec;

	/* this func does not support unity gain as 1 cannot be represented as Q1.15 value */
	assert(gain < IPC4_MIXIN_UNITY_GAIN);

	gain_vec = AE_L16_I((ae_int16 *)&gain, 0);
	gain_vec = AE_SLAI16S(gain_vec, 5);	/* convert to Q1.15 */

	assert(mixed_samples >= start_sample);
	samples_to_mix = AE_MIN_32_signed(mixed_samples - start_sample, sample_count);
	samples_to_copy = sample_count - samples_to_mix;
	n = 0;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (ae_int16 *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (ae_int16 *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int16x4 *)src;
		out = (ae_int16x4 *)dst;
		inu = AE_LA64_PP(in);
		outu1 = AE_LA64_PP(out);
		m = n >> 2;
		left = n & 0x03;
		/* process 4 samples per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4_IP(in_sample, inu, in);
			/* apply gain to in_sample */
			in_sample = AE_MULFP16X4S(in_sample, gain_vec);
			AE_LA16X4_IP(out_sample, outu1, out);
			out--;
			out_sample = AE_ADD16S(in_sample, out_sample);
			AE_SA16X4_IP(out_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			AE_L16_IP(in_sample, (ae_int16 *)in, sizeof(ae_int16));
			in_sample = AE_MULFP16X4S(in_sample, gain_vec);
			AE_L16_IP(out_sample, (ae_int16 *)out, 0);
			out_sample = AE_ADD16S(in_sample, out_sample);
			AE_S16_0_IP(out_sample, (ae_int16 *)out, sizeof(ae_int16));
		}
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (ae_int16 *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (ae_int16 *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int16x4 *)src;
		out = (ae_int16x4 *)dst;
		inu = AE_LA64_PP(in);
		m = n >> 2;
		left = n & 0x03;
		/* process 4 samples per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4_IP(in_sample, inu, in);
			in_sample = AE_MULFP16X4S(in_sample, gain_vec);
			AE_SA16X4_IP(in_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			AE_L16_IP(in_sample, (ae_int16 *)in, sizeof(ae_int16));
			in_sample = AE_MULFP16X4S(in_sample, gain_vec);
			AE_S16_0_IP(in_sample, (ae_int16 *)out, sizeof(ae_int16));
		}
	}
}
#endif	/* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void mix_s24(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
		    const struct cir_buf_ptr *source,
		    int32_t sample_count, uint16_t gain)
{
	int samples_to_mix, samples_to_copy, left_samples;
	int n, nmax, i, m, left;
	ae_int32x2 in_sample;
	ae_int32x2 out_sample;
	ae_int32x2 *in;
	ae_int32x2 *out;
	ae_valign inu;
	ae_valign outu1;
	ae_valign outu2 = AE_ZALIGN64();
	/* cir_buf_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = AE_MIN_32_signed(mixed_samples - start_sample, sample_count);
	samples_to_copy = sample_count - samples_to_mix;
	n = 0;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int32x2 *)src;
		out = (ae_int32x2 *)dst;
		inu = AE_LA64_PP(in);
		outu1 = AE_LA64_PP(out);
		m = n >> 1;
		left = n & 1;
		/* process 2 samples per time */
		for (i = 0; i < m; i++) {
			AE_LA32X2_IP(in_sample, inu, in);
			AE_LA32X2_IP(out_sample, outu1, out);
			out--;
			/* sign extent in_sample as AE_ADD24S expects Q9.23 arguments */
			in_sample = AE_SLAI24S(AE_MOVF24X2_FROMINT32X2(in_sample), 0);
			/* out_sample is already sign extended by other mixin in a loop below */
			out_sample = AE_ADD24S(in_sample, out_sample);
			AE_SA32X2_IP(out_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			AE_L32_IP(out_sample, (ae_int32 *)out, 0);
			/* sign extension */
			in_sample = AE_SLAI24S(AE_MOVF24X2_FROMINT32X2(in_sample), 0);
			out_sample = AE_ADD24S(in_sample, out_sample);
			AE_S32_L_IP(out_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		nmax = (int32_t *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int32x2 *)src;
		out = (ae_int32x2 *)dst;
		inu = AE_LA64_PP(in);
		m = n >> 1;
		left = n & 1;
		for (i = 0; i < m; i++) {
			AE_LA32X2_IP(in_sample, inu, in);
			/* sign extension */
			in_sample = AE_SLAI24S(AE_MOVF24X2_FROMINT32X2(in_sample), 0);
			AE_SA32X2_IP(in_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);
		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			/* sign extension */
			in_sample = AE_SLAI24S(AE_MOVF24X2_FROMINT32X2(in_sample), 0);
			AE_S32_L_IP(in_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}
}

static void mix_s24_gain(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
			 const struct cir_buf_ptr *source,
			 int32_t sample_count, uint16_t gain)
{
	int samples_to_mix, samples_to_copy, left_samples;
	int n, nmax, i, m, left;
	ae_int32x2 in_sample;
	ae_int32x2 out_sample;
	ae_int32x2 *in;
	ae_int32x2 *out;
	ae_valign inu;
	ae_valign outu1;
	ae_valign outu2 = AE_ZALIGN64();
	/* cir_buf_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;
	ae_f24x2 gain_vec;
	ae_int32 gain32 = (ae_int32)gain;

	/* this func does not support unity gain as 1 cannot be represented as Q1.23 value */
	assert(gain < IPC4_MIXIN_UNITY_GAIN);

	gain_vec = AE_MOVF24X2_FROMINT32X2(AE_L32_I(&gain32, 0));
	gain_vec = AE_SLAI24S(gain_vec, 13);	/* convert to Q1.23 */

	assert(mixed_samples >= start_sample);
	samples_to_mix = AE_MIN_32_signed(mixed_samples - start_sample, sample_count);
	samples_to_copy = sample_count - samples_to_mix;
	n = 0;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int32x2 *)src;
		out = (ae_int32x2 *)dst;
		inu = AE_LA64_PP(in);
		outu1 = AE_LA64_PP(out);
		m = n >> 1;
		left = n & 1;
		/* process 2 samples per time */
		for (i = 0; i < m; i++) {
			AE_LA32X2_IP(in_sample, inu, in);
			in_sample = AE_MULFP24X2R(AE_MOVF24X2_FROMINT32X2(in_sample), gain_vec);
			AE_LA32X2_IP(out_sample, outu1, out);
			out--;
			/* out samples are already sign extended by other mixin in a loop below */
			out_sample = AE_ADD24S(in_sample, out_sample);
			AE_SA32X2_IP(out_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			in_sample = AE_MULFP24X2R(AE_MOVF24X2_FROMINT32X2(in_sample), gain_vec);
			AE_L32_IP(out_sample, (ae_int32 *)out, 0);
			/* out samples are already sign extended by other mixin in a loop below */
			out_sample = AE_ADD24S(in_sample, out_sample);
			AE_S32_L_IP(out_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		nmax = (int32_t *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int32x2 *)src;
		out = (ae_int32x2 *)dst;
		inu = AE_LA64_PP(in);
		m = n >> 1;
		left = n & 1;
		for (i = 0; i < m; i++) {
			AE_LA32X2_IP(in_sample, inu, in);
			in_sample = AE_MULFP24X2R(AE_MOVF24X2_FROMINT32X2(in_sample), gain_vec);
			AE_SA32X2_IP(in_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);
		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			in_sample = AE_MULFP24X2R(AE_MOVF24X2_FROMINT32X2(in_sample), gain_vec);
			AE_S32_L_IP(in_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}
}

#endif	/* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void mix_s32(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
		    const struct cir_buf_ptr *source,
		    int32_t sample_count, uint16_t gain)
{
	int samples_to_mix, samples_to_copy, left_samples;
	int n, nmax, i, m, left;
	ae_int32x2 in_sample;
	ae_int32x2 out_sample;
	ae_int32x2 *in;
	ae_int32x2 *out;
	ae_valign inu;
	ae_valign outu1;
	ae_valign outu2 = AE_ZALIGN64();
	/* cir_buf_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = AE_MIN_32_signed(mixed_samples - start_sample, sample_count);
	samples_to_copy = sample_count - samples_to_mix;
	n = 0;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int32x2 *)src;
		out = (ae_int32x2 *)dst;
		inu = AE_LA64_PP(in);
		outu1 = AE_LA64_PP(out);
		m = n >> 1;
		left = n & 1;
		for (i = 0; i < m; i++) {
			AE_LA32X2_IP(in_sample, inu, in);
			AE_LA32X2_IP(out_sample, outu1, out);
			out--;
			out_sample = AE_ADD32S(in_sample, out_sample);
			AE_SA32X2_IP(out_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			AE_L32_IP(out_sample, (ae_int32 *)out, 0);
			out_sample = AE_ADD32S(in_sample, out_sample);
			AE_S32_L_IP(out_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int32x2 *)src;
		out = (ae_int32x2 *)dst;
		inu = AE_LA64_PP(in);
		m = n >> 1;
		left = n & 1;
		for (i = 0; i < m; i++) {
			AE_LA32X2_IP(in_sample, inu, in);
			AE_SA32X2_IP(in_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			AE_S32_L_IP(in_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}
}

static void mix_s32_gain(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
			 const struct cir_buf_ptr *source,
			 int32_t sample_count, uint16_t gain)
{
	int samples_to_mix, samples_to_copy, left_samples;
	int n, nmax, i, m, left;
	ae_int32x2 in_sample;
	ae_int32x2 out_sample;
	ae_int32x2 *in;
	ae_int32x2 *out;
	ae_valign inu;
	ae_valign outu1;
	ae_valign outu2 = AE_ZALIGN64();
	/* cir_buf_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;
	ae_f16x4 gain_vec;

	/* this func does not support unity gain as 1 cannot be represented as Q1.15 value */
	assert(gain < IPC4_MIXIN_UNITY_GAIN);

	gain_vec = AE_L16_I((ae_int16 *)&gain, 0);
	gain_vec = AE_SLAI16S(gain_vec, 5);	/* convert to Q1.15 */

	assert(mixed_samples >= start_sample);
	samples_to_mix = AE_MIN_32_signed(mixed_samples - start_sample, sample_count);
	samples_to_copy = sample_count - samples_to_mix;
	n = 0;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int32x2 *)src;
		out = (ae_int32x2 *)dst;
		inu = AE_LA64_PP(in);
		outu1 = AE_LA64_PP(out);
		m = n >> 1;
		left = n & 1;
		for (i = 0; i < m; i++) {
			AE_LA32X2_IP(in_sample, inu, in);
			AE_LA32X2_IP(out_sample, outu1, out);
			out--;
			AE_MULAFP32X16X2RS_L(out_sample, in_sample, gain_vec);
			AE_SA32X2_IP(out_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			AE_L32_IP(out_sample, (ae_int32 *)out, 0);
			AE_MULAFP32X16X2RS_L(out_sample, in_sample, gain_vec);
			AE_S32_L_IP(out_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src + n, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + n, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = AE_MIN_32_signed(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int32x2 *)src;
		out = (ae_int32x2 *)dst;
		inu = AE_LA64_PP(in);
		m = n >> 1;
		left = n & 1;
		for (i = 0; i < m; i++) {
			AE_LA32X2_IP(in_sample, inu, in);
			in_sample = AE_MULFP32X16X2RS_L(in_sample, gain_vec);
			AE_SA32X2_IP(in_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			in_sample = AE_MULFP32X16X2RS_L(in_sample, gain_vec);
			AE_S32_L_IP(in_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}
}

#endif	/* CONFIG_FORMAT_S32LE */

__cold_rodata const struct mix_func_map mix_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, mix_s16, mix_s16_gain },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, mix_s24, mix_s24_gain },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, mix_s32, mix_s32_gain }
#endif
};

const size_t mix_count = ARRAY_SIZE(mix_func_map);

#endif
