// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Sound Open Firmware. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/pcm_converter.h>

#ifdef PCM_CONVERTER_ARM_SIMD

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/arm_simd.h>
#include <sof/common.h>
#include <sof/compiler_attributes.h>
#include <ipc/stream.h>
#include <rtos/string.h>

#include <stddef.h>
#include <stdint.h>

#define BYTES_TO_S16_SAMPLES	1
#define BYTES_TO_S32_SAMPLES	2

#if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE
static int pcm_convert_s16_to_s24(const struct cir_buf_source *source,
				  uint32_t src_channels, struct cir_buf_sink *sink,
				  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int16_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S16_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = (int32_t)src[i + 0] << 8;
			dst[i + 1] = (int32_t)src[i + 1] << 8;
			dst[i + 2] = (int32_t)src[i + 2] << 8;
			dst[i + 3] = (int32_t)src[i + 3] << 8;
		}
		for (; i < n; i++) {
			dst[i] = (int32_t)src[i] << 8;
		}
		src += n;
		dst += n;
	}

	return samples;
}

static int pcm_convert_s24_to_s16(const struct cir_buf_source *source,
				  uint32_t src_channels, struct cir_buf_sink *sink,
				  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int32_t *src = source->ptr;
	int16_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S16_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			int16_t s0 = arm_sat_s16(Q_SHIFT_RND(sign_extend_s24(src[i + 0]), 23, 15));
			int16_t s1 = arm_sat_s16(Q_SHIFT_RND(sign_extend_s24(src[i + 1]), 23, 15));
			int16_t s2 = arm_sat_s16(Q_SHIFT_RND(sign_extend_s24(src[i + 2]), 23, 15));
			int16_t s3 = arm_sat_s16(Q_SHIFT_RND(sign_extend_s24(src[i + 3]), 23, 15));

			*(uint32_t *)(dst + i) = arm_pkhbt(s0, s1, 16);
			*(uint32_t *)(dst + i + 2) = arm_pkhbt(s2, s3, 16);
		}
		for (; i < n; i++) {
			dst[i] = arm_sat_s16(Q_SHIFT_RND(sign_extend_s24(src[i]), 23, 15));
		}
		src += n;
		dst += n;
	}

	return samples;
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE */

#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE
static int pcm_convert_s24_to_s32(const struct cir_buf_source *source,
				  uint32_t src_channels, struct cir_buf_sink *sink,
				  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = sign_extend_s24(src[i + 0]) << 8;
			dst[i + 1] = sign_extend_s24(src[i + 1]) << 8;
			dst[i + 2] = sign_extend_s24(src[i + 2]) << 8;
			dst[i + 3] = sign_extend_s24(src[i + 3]) << 8;
		}
		for (; i < n; i++) {
			dst[i] = sign_extend_s24(src[i]) << 8;
		}
		src += n;
		dst += n;
	}

	return samples;
}

static int pcm_convert_s32_to_s24(const struct cir_buf_source *source,
				  uint32_t src_channels, struct cir_buf_sink *sink,
				  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = arm_sat_s24(Q_SHIFT_RND(src[i + 0], 31, 23));
			dst[i + 1] = arm_sat_s24(Q_SHIFT_RND(src[i + 1], 31, 23));
			dst[i + 2] = arm_sat_s24(Q_SHIFT_RND(src[i + 2], 31, 23));
			dst[i + 3] = arm_sat_s24(Q_SHIFT_RND(src[i + 3], 31, 23));
		}
		for (; i < n; i++) {
			dst[i] = arm_sat_s24(Q_SHIFT_RND(src[i], 31, 23));
		}
		src += n;
		dst += n;
	}

	return samples;
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE */

#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
static int pcm_convert_s16_to_s32(const struct cir_buf_source *source,
				  uint32_t src_channels, struct cir_buf_sink *sink,
				  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int16_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S16_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = (int32_t)src[i + 0] << 16;
			dst[i + 1] = (int32_t)src[i + 1] << 16;
			dst[i + 2] = (int32_t)src[i + 2] << 16;
			dst[i + 3] = (int32_t)src[i + 3] << 16;
		}
		for (; i < n; i++) {
			dst[i] = (int32_t)src[i] << 16;
		}
		src += n;
		dst += n;
	}

	return samples;
}

static int pcm_convert_s32_to_s16(const struct cir_buf_source *source,
				  uint32_t src_channels, struct cir_buf_sink *sink,
				  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int32_t *src = source->ptr;
	int16_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S16_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			int16_t s0 = arm_sat_s16(Q_SHIFT_RND(src[i + 0], 31, 15));
			int16_t s1 = arm_sat_s16(Q_SHIFT_RND(src[i + 1], 31, 15));
			int16_t s2 = arm_sat_s16(Q_SHIFT_RND(src[i + 2], 31, 15));
			int16_t s3 = arm_sat_s16(Q_SHIFT_RND(src[i + 3], 31, 15));

			*(uint32_t *)(dst + i) = arm_pkhbt(s0, s1, 16);
			*(uint32_t *)(dst + i + 2) = arm_pkhbt(s2, s3, 16);
		}
		for (; i < n; i++) {
			dst[i] = arm_sat_s16(Q_SHIFT_RND(src[i], 31, 15));
		}
		src += n;
		dst += n;
	}

	return samples;
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE */

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE
static int pcm_convert_s16_to_f(const struct cir_buf_source *source,
				uint32_t src_channels, struct cir_buf_sink *sink,
				uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int16_t *src = source->ptr;
	float *dst = sink->ptr;
	size_t processed, nmax, n, i;
	const float scale = 1.0f / 32768.0f;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S16_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		for (i = 0; i < n; i++) {
			dst[i] = (float)src[i] * scale;
		}
		src += n;
		dst += n;
	}

	return samples;
}

static int pcm_convert_f_to_s16(const struct cir_buf_source *source,
				uint32_t src_channels, struct cir_buf_sink *sink,
				uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const float *src = source->ptr;
	int16_t *dst = sink->ptr;
	size_t processed, nmax, n, i;
	const float scale = 32768.0f;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S16_SAMPLES;
		n = MIN(n, nmax);

		for (i = 0; i < n; i++) {
			dst[i] = arm_sat_s16((int32_t)(src[i] * scale));
		}
		src += n;
		dst += n;
	}

	return samples;
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE */

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE
static int pcm_convert_s24_to_f(const struct cir_buf_source *source,
				uint32_t src_channels, struct cir_buf_sink *sink,
				uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int32_t *src = source->ptr;
	float *dst = sink->ptr;
	size_t processed, nmax, n, i;
	const float scale = 1.0f / 8388608.0f;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		for (i = 0; i < n; i++) {
			dst[i] = (float)sign_extend_s24(src[i]) * scale;
		}
		src += n;
		dst += n;
	}

	return samples;
}

static int pcm_convert_f_to_s24(const struct cir_buf_source *source,
				uint32_t src_channels, struct cir_buf_sink *sink,
				uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const float *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t processed, nmax, n, i;
	const float scale = 8388608.0f;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		for (i = 0; i < n; i++) {
			dst[i] = arm_sat_s24((int32_t)(src[i] * scale));
		}
		src += n;
		dst += n;
	}

	return samples;
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE */

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE
static int pcm_convert_s32_to_f(const struct cir_buf_source *source,
				uint32_t src_channels, struct cir_buf_sink *sink,
				uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int32_t *src = source->ptr;
	float *dst = sink->ptr;
	size_t processed, nmax, n, i;
	const float scale = 1.0f / 2147483648.0f;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		for (i = 0; i < n; i++) {
			dst[i] = (float)src[i] * scale;
		}
		src += n;
		dst += n;
	}

	return samples;
}

static int pcm_convert_f_to_s32(const struct cir_buf_source *source,
				uint32_t src_channels, struct cir_buf_sink *sink,
				uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const float *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t processed, nmax, n, i;
	const float scale = 2147483648.0f;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		for (i = 0; i < n; i++) {
			dst[i] = arm_sat_q31((int64_t)(src[i] * scale));
		}
		src += n;
		dst += n;
	}

	return samples;
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE */

#if CONFIG_PCM_CONVERTER_FORMAT_S16_C16_AND_S16_C32
static int pcm_convert_s16_c16_to_s16_c32(const struct cir_buf_source *source,
					  uint32_t src_channels, struct cir_buf_sink *sink,
					  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int16_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S16_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = (int32_t)src[i + 0];
			dst[i + 1] = (int32_t)src[i + 1];
			dst[i + 2] = (int32_t)src[i + 2];
			dst[i + 3] = (int32_t)src[i + 3];
		}
		for (; i < n; i++) {
			dst[i] = (int32_t)src[i];
		}
		src += n;
		dst += n;
	}

	return samples;
}

static int pcm_convert_s16_c32_to_s16_c16(const struct cir_buf_source *source,
					  uint32_t src_channels, struct cir_buf_sink *sink,
					  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int32_t *src = source->ptr;
	int16_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S16_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			int16_t s0 = arm_sat_s16(src[i + 0]);
			int16_t s1 = arm_sat_s16(src[i + 1]);
			int16_t s2 = arm_sat_s16(src[i + 2]);
			int16_t s3 = arm_sat_s16(src[i + 3]);

			*(uint32_t *)(dst + i) = arm_pkhbt(s0, s1, 16);
			*(uint32_t *)(dst + i + 2) = arm_pkhbt(s2, s3, 16);
		}
		for (; i < n; i++) {
			dst[i] = arm_sat_s16(src[i]);
		}
		src += n;
		dst += n;
	}

	return samples;
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S16_C16_AND_S16_C32 */

#if CONFIG_PCM_CONVERTER_FORMAT_S16_C32_AND_S32_C32
static int pcm_convert_s16_c32_to_s32_c32(const struct cir_buf_source *source,
					  uint32_t src_channels, struct cir_buf_sink *sink,
					  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = src[i + 0] << 16;
			dst[i + 1] = src[i + 1] << 16;
			dst[i + 2] = src[i + 2] << 16;
			dst[i + 3] = src[i + 3] << 16;
		}
		for (; i < n; i++) {
			dst[i] = src[i] << 16;
		}
		src += n;
		dst += n;
	}

	return samples;
}

static int pcm_convert_s32_c32_to_s16_c32(const struct cir_buf_source *source,
					  uint32_t src_channels, struct cir_buf_sink *sink,
					  uint32_t sink_channels, size_t samples, uint32_t chmap)
{
	const int32_t *src = source->ptr;
	int32_t *dst = sink->ptr;
	size_t processed, nmax, n, i;

	for (processed = 0; processed < samples; processed += n) {
		src = cir_buf_wrap(src, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
		n = samples - processed;
		nmax = cir_buf_bytes_without_wrap(src, source->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);
		nmax = cir_buf_bytes_without_wrap(dst, sink->buf_end) >> BYTES_TO_S32_SAMPLES;
		n = MIN(n, nmax);

		size_t n4 = n & ~3;
		for (i = 0; i < n4; i += 4) {
			dst[i + 0] = arm_sat_s16(Q_SHIFT_RND(src[i + 0], 31, 15));
			dst[i + 1] = arm_sat_s16(Q_SHIFT_RND(src[i + 1], 31, 15));
			dst[i + 2] = arm_sat_s16(Q_SHIFT_RND(src[i + 2], 31, 15));
			dst[i + 3] = arm_sat_s16(Q_SHIFT_RND(src[i + 3], 31, 15));
		}
		for (; i < n; i++) {
			dst[i] = arm_sat_s16(Q_SHIFT_RND(src[i], 31, 15));
		}
		src += n;
		dst += n;
	}

	return samples;
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S16_C32_AND_S32_C32 */

const struct pcm_func_map pcm_func_map[] = {
#if CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, just_copy_2b },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, just_copy_4b },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, pcm_convert_s16_to_s24 },
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, pcm_convert_s24_to_s16 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, just_copy_4b },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, pcm_convert_s16_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, pcm_convert_s32_to_s16 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, pcm_convert_s24_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, pcm_convert_s32_to_s24 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_FLOAT, just_copy_4b },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_FLOAT, pcm_convert_s16_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S16_LE, pcm_convert_f_to_s16 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_FLOAT, pcm_convert_s24_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S24_4LE, pcm_convert_f_to_s24 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_FLOAT, pcm_convert_s32_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE, pcm_convert_f_to_s32 },
#endif
};

const size_t pcm_func_count = ARRAY_SIZE(pcm_func_map);

const struct pcm_func_vc_map pcm_func_vc_map[] = {
#if CONFIG_PCM_CONVERTER_FORMAT_S16_C16_AND_S16_C32
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,
		pcm_convert_s16_c16_to_s16_c32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,
		pcm_convert_s16_c32_to_s16_c16 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S16_C32_AND_S32_C32
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE,
		pcm_convert_s16_c32_to_s32_c32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,
		pcm_convert_s32_c32_to_s16_c32 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		just_copy_4b },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE,
		pcm_convert_s24_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		pcm_convert_s32_to_s24 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		pcm_convert_s16_to_s24 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,
		pcm_convert_s24_to_s16 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S16_C32_AND_S16_C32
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE,
		just_copy_4b },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_FLOAT,
		SOF_IPC_FRAME_FLOAT, pcm_convert_s24_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S24_4LE, pcm_convert_f_to_s24 },
#endif
};

const size_t pcm_func_vc_count = ARRAY_SIZE(pcm_func_vc_map);

#endif /* PCM_CONVERTER_ARM_SIMD */
