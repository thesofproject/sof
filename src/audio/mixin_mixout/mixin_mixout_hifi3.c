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
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu1 = AE_ZALIGN64();
	ae_valign outu2 = AE_ZALIGN64();
	/* cir_buf_wrap() is required and is done below in a loop */
	ae_int16 *dst = (ae_int16 *)sink->ptr + start_sample;
	ae_int16 *src = source->ptr;
	ae_int16x4 gain_vec;
	ae_int32x2 tmpl, tmph;

	gain_vec = AE_L16_I((ae_int16 *)&gain, 0);

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
		/* process 4 frames per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4_IP(in_sample, inu, in);

			/* apply gain to in_sample */
			AE_MUL16X4(tmph, tmpl, in_sample, gain_vec);
			tmpl = AE_SRAI32(tmpl, IPC4_MIXIN_GAIN_SHIFT);
			tmph = AE_SRAI32(tmph, IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_CVT16X4(tmph, tmpl);

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

			AE_MUL16X4(tmph, tmpl, in_sample, gain_vec);
			tmpl = AE_SRAI32(tmpl, IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_CVT16X4(tmpl, tmpl);

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
		/* process 4 frames per loop */
		for (i = 0; i < m; i++) {
			AE_LA16X4_IP(in_sample, inu, in);

			AE_MUL16X4(tmph, tmpl, in_sample, gain_vec);
			tmpl = AE_SRAI32(tmpl, IPC4_MIXIN_GAIN_SHIFT);
			tmph = AE_SRAI32(tmph, IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_CVT16X4(tmph, tmpl);

			AE_SA16X4_IP(in_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left samples that less than 4
		 * one by one to avoid memory access overrun
		 */
		for (i = 0; i < left ; i++) {
			AE_L16_IP(in_sample, (ae_int16 *)in, sizeof(ae_int16));

			AE_MUL16X4(tmph, tmpl, in_sample, gain_vec);
			tmpl = AE_SRAI32(tmpl, IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_CVT16X4(tmpl, tmpl);

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
	ae_int32x2 in_sample, in_sample32;
	ae_int32x2 out_sample;
	ae_int32x2 *in;
	ae_int32x2 *out;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu1 = AE_ZALIGN64();
	ae_valign outu2 = AE_ZALIGN64();
	/* cir_buf_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;
	ae_int16x4 gain_vec;
	ae_int64 tmph, tmpl;

	gain_vec = AE_L16_I((ae_int16 *)&gain, 0);

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

			/* apply gain to in_sample */
			in_sample32 = AE_SLAI32(in_sample, 8);	/* sign extension */
			tmpl = AE_MUL32X16_L0(in_sample32, gain_vec);
			tmph = AE_MUL32X16_H0(in_sample32, gain_vec);
			tmpl = AE_SRAI64(tmpl, 8 + IPC4_MIXIN_GAIN_SHIFT);
			tmph = AE_SRAI64(tmph, 8 + IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_SEL32_LL(AE_MOVINT32X2_FROMINT64(tmph),
						AE_MOVINT32X2_FROMINT64(tmpl));

			AE_LA32X2_IP(out_sample, outu1, out);
			out--;
			out_sample = AE_ADD24S(in_sample, out_sample);
			AE_SA32X2_IP(out_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));

			in_sample32 = AE_SLAI32(in_sample, 8);	/* sign extension */
			tmpl = AE_MUL32X16_L0(in_sample32, gain_vec);
			tmpl = AE_SRAI64(tmpl, 8 + IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_MOVINT32X2_FROMINT64(tmpl);

			AE_L32_IP(out_sample, (ae_int32 *)out, 0);
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

			in_sample32 = AE_SLAI32(in_sample, 8);	/* sign extension */
			tmpl = AE_MUL32X16_L0(in_sample32, gain_vec);
			tmph = AE_MUL32X16_H0(in_sample32, gain_vec);
			tmpl = AE_SRAI64(tmpl, 8 + IPC4_MIXIN_GAIN_SHIFT);
			tmph = AE_SRAI64(tmph, 8 + IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_SEL32_LL(AE_MOVINT32X2_FROMINT64(tmph),
						AE_MOVINT32X2_FROMINT64(tmpl));

			AE_SA32X2_IP(in_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);
		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));

			in_sample32 = AE_SLAI32(in_sample, 8);	/* sign extension */
			tmpl = AE_MUL32X16_L0(in_sample32, gain_vec);
			tmpl = AE_SRAI64(tmpl, 8 + IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_MOVINT32X2_FROMINT64(tmpl);

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
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu1 = AE_ZALIGN64();
	ae_valign outu2 = AE_ZALIGN64();
	/* cir_buf_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;
	ae_int16x4 gain_vec;
	ae_int64 tmpl, tmph;

	gain_vec = AE_L16_I((ae_int16 *)&gain, 0);

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

			/* apply gain to in_sample */
			tmpl = AE_MUL32X16_L0(in_sample, gain_vec);
			tmph = AE_MUL32X16_H0(in_sample, gain_vec);
			tmpl = AE_SRAI64(tmpl, IPC4_MIXIN_GAIN_SHIFT);
			tmph = AE_SRAI64(tmph, IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_SEL32_LL(AE_MOVINT32X2_FROMINT64(tmph),
						AE_MOVINT32X2_FROMINT64(tmpl));

			AE_LA32X2_IP(out_sample, outu1, out);
			out--;
			out_sample = AE_ADD32S(in_sample, out_sample);
			AE_SA32X2_IP(out_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));

			tmpl = AE_MUL32X16_L0(in_sample, gain_vec);
			tmpl = AE_SRAI64(tmpl, IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_MOVINT32X2_FROMINT64(tmpl);

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

			tmpl = AE_MUL32X16_L0(in_sample, gain_vec);
			tmph = AE_MUL32X16_H0(in_sample, gain_vec);
			tmpl = AE_SRAI64(tmpl, IPC4_MIXIN_GAIN_SHIFT);
			tmph = AE_SRAI64(tmph, IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_SEL32_LL(AE_MOVINT32X2_FROMINT64(tmph),
						AE_MOVINT32X2_FROMINT64(tmpl));

			AE_SA32X2_IP(in_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));

			tmpl = AE_MUL32X16_L0(in_sample, gain_vec);
			tmpl = AE_SRAI64(tmpl, IPC4_MIXIN_GAIN_SHIFT);
			in_sample = AE_MOVINT32X2_FROMINT64(tmpl);

			AE_S32_L_IP(in_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}
}

#endif	/* CONFIG_FORMAT_S32LE */

const struct mix_func_map mix_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, mix_s16 },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, mix_s24 },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, mix_s32 }
#endif
};

const size_t mix_count = ARRAY_SIZE(mix_func_map);

#endif
