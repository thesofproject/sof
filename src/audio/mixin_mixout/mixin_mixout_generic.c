// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <ipc4/mixin_mixout.h>
#include <sof/common.h>
#include <rtos/string.h>

#ifdef MIXIN_MIXOUT_GENERIC

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
		for (i = 0; i < n; i++) {
			*dst = sat_int16(*dst + *src++);
			dst++;
		}
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
		for (i = 0; i < n; i++) {
			*dst = sat_int24(sign_extend_s24(*dst) + sign_extend_s24(*src++));
			dst++;
		}
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
		for (i = 0; i < n; i++) {
			*dst = sat_int32((int64_t)*dst + (int64_t)*src++);
			dst++;
		}
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
