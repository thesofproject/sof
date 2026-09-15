// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <sof/common.h>
#include <rtos/string.h>

#include "mixin_mixout.h"

#if SOF_USE_HIFI(NONE, MIXIN_MIXOUT)

#if CONFIG_FORMAT_S16LE
static void mix_s16(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
		    const struct cir_buf_ptr *source,
		    int32_t sample_count, uint16_t gain)
{
	int32_t samples_to_mix, samples_to_copy, left_samples;
	int32_t n, nmax, i;

	/* cir_buf_wrap() is required and is done below in a loop */
	int16_t *dst = (int16_t *)sink->ptr + start_sample;
	int16_t *src = source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = mixed_samples - start_sample;
	samples_to_mix = MIN(samples_to_mix, sample_count);
	samples_to_copy = sample_count - samples_to_mix;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int16_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int16_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = sat_int16((int32_t)dst[i + 0] + (int32_t)src[i + 0]);
			dst[i + 1] = sat_int16((int32_t)dst[i + 1] + (int32_t)src[i + 1]);
			dst[i + 2] = sat_int16((int32_t)dst[i + 2] + (int32_t)src[i + 2]);
			dst[i + 3] = sat_int16((int32_t)dst[i + 3] + (int32_t)src[i + 3]);
		}
		for (; i < n; i++) {
			dst[i] = sat_int16((int32_t)dst[i] + (int32_t)src[i]);
		}
		dst += n;
		src += n;
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		nmax = (int16_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int16_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		memcpy_s(dst, n * sizeof(int16_t), src, n * sizeof(int16_t));
		dst += n;
		src += n;
	}
}

static void mix_s16_gain(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
			 const struct cir_buf_ptr *source,
			 int32_t sample_count, uint16_t gain)
{
	int32_t samples_to_mix, samples_to_copy, left_samples;
	int32_t n, nmax, i;

	/* cir_buf_wrap() is required and is done below in a loop */
	int16_t *dst = (int16_t *)sink->ptr + start_sample;
	int16_t *src = source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = mixed_samples - start_sample;
	samples_to_mix = MIN(samples_to_mix, sample_count);
	samples_to_copy = sample_count - samples_to_mix;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int16_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int16_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = sat_int16((int32_t)dst[i + 0] +
				q_mults_16x16(src[i + 0], gain, IPC4_MIXIN_GAIN_SHIFT));
			dst[i + 1] = sat_int16((int32_t)dst[i + 1] +
				q_mults_16x16(src[i + 1], gain, IPC4_MIXIN_GAIN_SHIFT));
			dst[i + 2] = sat_int16((int32_t)dst[i + 2] +
				q_mults_16x16(src[i + 2], gain, IPC4_MIXIN_GAIN_SHIFT));
			dst[i + 3] = sat_int16((int32_t)dst[i + 3] +
				q_mults_16x16(src[i + 3], gain, IPC4_MIXIN_GAIN_SHIFT));
		}
		for (; i < n; i++) {
			dst[i] = sat_int16((int32_t)dst[i] +
				q_mults_16x16(src[i], gain, IPC4_MIXIN_GAIN_SHIFT));
		}
		dst += n;
		src += n;
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		nmax = (int16_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int16_t *)sink->buf_end - dst;
		n = MIN(n, nmax);

		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = q_mults_16x16(src[i + 0], gain, IPC4_MIXIN_GAIN_SHIFT);
			dst[i + 1] = q_mults_16x16(src[i + 1], gain, IPC4_MIXIN_GAIN_SHIFT);
			dst[i + 2] = q_mults_16x16(src[i + 2], gain, IPC4_MIXIN_GAIN_SHIFT);
			dst[i + 3] = q_mults_16x16(src[i + 3], gain, IPC4_MIXIN_GAIN_SHIFT);
		}
		for (; i < n; i++) {
			dst[i] = q_mults_16x16(src[i], gain, IPC4_MIXIN_GAIN_SHIFT);
		}
		dst += n;
		src += n;
	}
}
#endif	/* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void mix_s24(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
		    const struct cir_buf_ptr *source,
		    int32_t sample_count, uint16_t gain)
{
	int32_t samples_to_mix, samples_to_copy, left_samples;
	int32_t n, nmax, i;
	/* cir_buf_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = mixed_samples - start_sample;
	samples_to_mix = MIN(samples_to_mix, sample_count);
	samples_to_copy = sample_count - samples_to_mix;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = sat_int24(sign_extend_s24(dst[i + 0]) + sign_extend_s24(src[i + 0]));
			dst[i + 1] = sat_int24(sign_extend_s24(dst[i + 1]) + sign_extend_s24(src[i + 1]));
			dst[i + 2] = sat_int24(sign_extend_s24(dst[i + 2]) + sign_extend_s24(src[i + 2]));
			dst[i + 3] = sat_int24(sign_extend_s24(dst[i + 3]) + sign_extend_s24(src[i + 3]));
		}
		for (; i < n; i++) {
			dst[i] = sat_int24(sign_extend_s24(dst[i]) + sign_extend_s24(src[i]));
		}
		dst += n;
		src += n;
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		nmax = (int32_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		memcpy_s(dst, n * sizeof(int32_t), src, n * sizeof(int32_t));
		dst += n;
		src += n;
	}
}

static void mix_s24_gain(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
			 const struct cir_buf_ptr *source,
			 int32_t sample_count, uint16_t gain)
{
	int32_t samples_to_mix, samples_to_copy, left_samples;
	int32_t n, nmax, i;
	/* cir_buf_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = mixed_samples - start_sample;
	samples_to_mix = MIN(samples_to_mix, sample_count);
	samples_to_copy = sample_count - samples_to_mix;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = sat_int24(sign_extend_s24(dst[i + 0]) +
				(int32_t)q_mults_32x32(sign_extend_s24(src[i + 0]),
						       gain, IPC4_MIXIN_GAIN_SHIFT));
			dst[i + 1] = sat_int24(sign_extend_s24(dst[i + 1]) +
				(int32_t)q_mults_32x32(sign_extend_s24(src[i + 1]),
						       gain, IPC4_MIXIN_GAIN_SHIFT));
			dst[i + 2] = sat_int24(sign_extend_s24(dst[i + 2]) +
				(int32_t)q_mults_32x32(sign_extend_s24(src[i + 2]),
						       gain, IPC4_MIXIN_GAIN_SHIFT));
			dst[i + 3] = sat_int24(sign_extend_s24(dst[i + 3]) +
				(int32_t)q_mults_32x32(sign_extend_s24(src[i + 3]),
						       gain, IPC4_MIXIN_GAIN_SHIFT));
		}
		for (; i < n; i++) {
			dst[i] = sat_int24(sign_extend_s24(dst[i]) +
				(int32_t)q_mults_32x32(sign_extend_s24(src[i]),
						       gain, IPC4_MIXIN_GAIN_SHIFT));
		}
		dst += n;
		src += n;
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		nmax = (int32_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = q_mults_32x32(sign_extend_s24(src[i + 0]), gain, IPC4_MIXIN_GAIN_SHIFT);
			dst[i + 1] = q_mults_32x32(sign_extend_s24(src[i + 1]), gain, IPC4_MIXIN_GAIN_SHIFT);
			dst[i + 2] = q_mults_32x32(sign_extend_s24(src[i + 2]), gain, IPC4_MIXIN_GAIN_SHIFT);
			dst[i + 3] = q_mults_32x32(sign_extend_s24(src[i + 3]), gain, IPC4_MIXIN_GAIN_SHIFT);
		}
		for (; i < n; i++) {
			dst[i] = q_mults_32x32(sign_extend_s24(src[i]), gain, IPC4_MIXIN_GAIN_SHIFT);
		}
		dst += n;
		src += n;
	}
}
#endif	/* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void mix_s32(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
		    const struct cir_buf_ptr *source,
		    int32_t sample_count, uint16_t gain)
{
	int32_t samples_to_mix, samples_to_copy, left_samples;
	int32_t n, nmax, i;
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = mixed_samples - start_sample;
	samples_to_mix = MIN(samples_to_mix, sample_count);
	samples_to_copy = sample_count - samples_to_mix;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = sat_int32((int64_t)dst[i + 0] + (int64_t)src[i + 0]);
			dst[i + 1] = sat_int32((int64_t)dst[i + 1] + (int64_t)src[i + 1]);
			dst[i + 2] = sat_int32((int64_t)dst[i + 2] + (int64_t)src[i + 2]);
			dst[i + 3] = sat_int32((int64_t)dst[i + 3] + (int64_t)src[i + 3]);
		}
		for (; i < n; i++) {
			dst[i] = sat_int32((int64_t)dst[i] + (int64_t)src[i]);
		}
		dst += n;
		src += n;
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		nmax = (int32_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		memcpy_s(dst, n * sizeof(int32_t), src, n * sizeof(int32_t));
		dst += n;
		src += n;
	}
}

static void mix_s32_gain(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
			 const struct cir_buf_ptr *source,
			 int32_t sample_count, uint16_t gain)
{
	int32_t samples_to_mix, samples_to_copy, left_samples;
	int32_t n, nmax, i;
	int32_t *dst = (int32_t *)sink->ptr + start_sample;
	int32_t *src = source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = mixed_samples - start_sample;
	samples_to_mix = MIN(samples_to_mix, sample_count);
	samples_to_copy = sample_count - samples_to_mix;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		/* calculate the remaining samples*/
		nmax = (int32_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = sat_int32((int64_t)dst[i + 0] +
				q_mults_32x32(src[i + 0], gain, IPC4_MIXIN_GAIN_SHIFT));
			dst[i + 1] = sat_int32((int64_t)dst[i + 1] +
				q_mults_32x32(src[i + 1], gain, IPC4_MIXIN_GAIN_SHIFT));
			dst[i + 2] = sat_int32((int64_t)dst[i + 2] +
				q_mults_32x32(src[i + 2], gain, IPC4_MIXIN_GAIN_SHIFT));
			dst[i + 3] = sat_int32((int64_t)dst[i + 3] +
				q_mults_32x32(src[i + 3], gain, IPC4_MIXIN_GAIN_SHIFT));
		}
		for (; i < n; i++) {
			dst[i] = sat_int32((int64_t)dst[i] +
				q_mults_32x32(src[i], gain, IPC4_MIXIN_GAIN_SHIFT));
		}
		dst += n;
		src += n;
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		nmax = (int32_t *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (int32_t *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = q_mults_32x32(src[i + 0], gain, IPC4_MIXIN_GAIN_SHIFT);
			dst[i + 1] = q_mults_32x32(src[i + 1], gain, IPC4_MIXIN_GAIN_SHIFT);
			dst[i + 2] = q_mults_32x32(src[i + 2], gain, IPC4_MIXIN_GAIN_SHIFT);
			dst[i + 3] = q_mults_32x32(src[i + 3], gain, IPC4_MIXIN_GAIN_SHIFT);
		}
		for (; i < n; i++) {
			dst[i] = q_mults_32x32(src[i], gain, IPC4_MIXIN_GAIN_SHIFT);
		}
		dst += n;
		src += n;
	}
}
#endif	/* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_FLOAT
static void mix_float(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
		      const struct cir_buf_ptr *source,
		      int32_t sample_count, uint16_t gain)
{
	int32_t samples_to_mix, samples_to_copy, left_samples;
	int32_t n, nmax, i;
	float *dst = (float *)sink->ptr + start_sample;
	const float *src = (const float *)source->ptr;

	assert(mixed_samples >= start_sample);
	samples_to_mix = mixed_samples - start_sample;
	samples_to_mix = MIN(samples_to_mix, sample_count);
	samples_to_copy = sample_count - samples_to_mix;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap((void *)src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap((void *)dst, sink->buf_start, sink->buf_end);
		nmax = (const float *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (float *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] += src[i + 0];
			dst[i + 1] += src[i + 1];
			dst[i + 2] += src[i + 2];
			dst[i + 3] += src[i + 3];
		}
		for (; i < n; i++) {
			dst[i] += src[i];
		}
		dst += n;
		src += n;
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap((void *)src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap((void *)dst, sink->buf_start, sink->buf_end);
		nmax = (const float *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (float *)sink->buf_end - dst;
		n = MIN(n, nmax);
		memcpy_s(dst, n * sizeof(float), src, n * sizeof(float));
		dst += n;
		src += n;
	}
}

static void mix_float_gain(struct cir_buf_ptr *sink, int32_t start_sample, int32_t mixed_samples,
			   const struct cir_buf_ptr *source,
			   int32_t sample_count, uint16_t gain)
{
	int32_t samples_to_mix, samples_to_copy, left_samples;
	int32_t n, nmax, i;
	float *dst = (float *)sink->ptr + start_sample;
	const float *src = (const float *)source->ptr;
	const float gain_f = (float)gain * (1.0f / (float)IPC4_MIXIN_UNITY_GAIN);

	assert(mixed_samples >= start_sample);
	samples_to_mix = mixed_samples - start_sample;
	samples_to_mix = MIN(samples_to_mix, sample_count);
	samples_to_copy = sample_count - samples_to_mix;

	for (left_samples = samples_to_mix; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap((void *)src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap((void *)dst, sink->buf_start, sink->buf_end);
		nmax = (const float *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (float *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] += src[i + 0] * gain_f;
			dst[i + 1] += src[i + 1] * gain_f;
			dst[i + 2] += src[i + 2] * gain_f;
			dst[i + 3] += src[i + 3] * gain_f;
		}
		for (; i < n; i++) {
			dst[i] += src[i] * gain_f;
		}
		dst += n;
		src += n;
	}

	for (left_samples = samples_to_copy; left_samples > 0; left_samples -= n) {
		src = cir_buf_wrap((void *)src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap((void *)dst, sink->buf_start, sink->buf_end);
		nmax = (const float *)source->buf_end - src;
		n = MIN(left_samples, nmax);
		nmax = (float *)sink->buf_end - dst;
		n = MIN(n, nmax);
		int32_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = src[i + 0] * gain_f;
			dst[i + 1] = src[i + 1] * gain_f;
			dst[i + 2] = src[i + 2] * gain_f;
			dst[i + 3] = src[i + 3] * gain_f;
		}
		for (; i < n; i++) {
			dst[i] = src[i] * gain_f;
		}
		dst += n;
		src += n;
	}
}
#endif /* CONFIG_FORMAT_FLOAT */

__cold_rodata const struct mix_func_map mix_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, mix_s16, mix_s16_gain },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, mix_s24, mix_s24_gain },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, mix_s32, mix_s32_gain },
#endif
#if CONFIG_FORMAT_FLOAT
	{ SOF_IPC_FRAME_FLOAT, mix_float, mix_float_gain },
#endif
};

const size_t mix_count = ARRAY_SIZE(mix_func_map);

#endif
