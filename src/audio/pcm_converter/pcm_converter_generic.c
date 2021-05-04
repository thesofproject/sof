// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file audio/pcm_converter/pcm_converter_generic.c
 * \brief PCM converter generic processing implementation
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 * \authors Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#include <sof/audio/pcm_converter.h>

#ifdef PCM_CONVERTER_GENERIC

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/bit.h>
#include <sof/common.h>
#include <ipc/stream.h>

#include <stddef.h>
#include <stdint.h>

#if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE

static int pcm_convert_s16_to_s24(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	uint32_t buff_frag = 0;
	int16_t *src;
	int32_t *dst;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		src = audio_stream_read_frag_s16(source, buff_frag + ioffset);
		dst = audio_stream_write_frag_s32(sink, buff_frag + ooffset);
		*dst = *src << 8;
		buff_frag++;
	}

	return samples;
}

static int pcm_convert_s24_to_s16(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	uint32_t buff_frag = 0;
	int32_t *src;
	int16_t *dst;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		src = audio_stream_read_frag_s32(source, buff_frag + ioffset);
		dst = audio_stream_write_frag_s16(sink, buff_frag + ooffset);
		*dst = sat_int16(Q_SHIFT_RND(sign_extend_s24(*src), 23, 15));
		buff_frag++;
	}

	return samples;
}

#endif /* CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE */

#if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE

static int pcm_convert_s16_to_s32(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	uint32_t buff_frag = 0;
	int16_t *src;
	int32_t *dst;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		src = audio_stream_read_frag_s16(source, buff_frag + ioffset);
		dst = audio_stream_write_frag_s32(sink, buff_frag + ooffset);
		*dst = *src << 16;
		buff_frag++;
	}

	return samples;
}

static int pcm_convert_s32_to_s16(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	uint32_t buff_frag = 0;
	int32_t *src;
	int16_t *dst;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		src = audio_stream_read_frag_s32(source, buff_frag + ioffset);
		dst = audio_stream_write_frag_s16(sink, buff_frag + ooffset);
		*dst = sat_int16(Q_SHIFT_RND(*src, 31, 15));
		buff_frag++;
	}

	return samples;
}

#endif /* CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE */

#if CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE

static int pcm_convert_s24_to_s32(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	uint32_t buff_frag = 0;
	int32_t *src;
	int32_t *dst;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		src = audio_stream_read_frag_s32(source, buff_frag + ioffset);
		dst = audio_stream_write_frag_s32(sink, buff_frag + ooffset);
		*dst = *src << 8;
		buff_frag++;
	}

	return samples;
}

static int pcm_convert_s32_to_s24(const struct audio_stream *source,
				  uint32_t ioffset, struct audio_stream *sink,
				  uint32_t ooffset, uint32_t samples)
{
	uint32_t buff_frag = 0;
	int32_t *src;
	int32_t *dst;
	uint32_t i;

	for (i = 0; i < samples; i++) {
		src = audio_stream_read_frag_s32(source, buff_frag + ioffset);
		dst = audio_stream_write_frag_s32(sink, buff_frag + ooffset);
		*dst = sat_int24(Q_SHIFT_RND(*src, 31, 23));
		buff_frag++;
	}

	return samples;
}

#endif /* CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE */

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && (CONFIG_PCM_CONVERTER_FORMAT_S16LE || CONFIG_PCM_CONVERTER_FORMAT_S24LE || CONFIG_PCM_CONVERTER_FORMAT_S32LE)
/*
 * IEEE 754 binary32 float format:
 *
 *   S|EEEEEEEE|MMMMMMMMMMMMMMMMMMMMMMM|
 *  31|30    23|22                    0|
 *
 * S - sign bit
 * E - exponent number, base 2
 * M - mantissa, unsigned Q1.22 value where integer portion is always set
*/

/**
 * shift d value left (for positive a) or right (for negative a)
 * and take care about overflows
 */
static inline int32_t _pcm_shift(int32_t d, int32_t a)
{
	int64_t dd = d;

	if (a > 32)
		a = 32;
	else if (a < -32)
		a = -32;

	dd = a >= 0 ? dd << a : dd >> -a;
	if (dd > INT32_MAX)
		dd = INT32_MAX;

	return (int32_t)dd;
}

/**
 * Calculate absolute value of s32 number without using code branching.
 * XOR number with sign bit (stretched in 32 bits) (+1 for for negative numbers)
 */
#define PCM_ABS32(x) (((x) ^ ((int32_t)(x) >> 31)) + ((uint32_t)(x) >> 31))

/**
 * \brief convert float number to fixed point
 *
 * Do not relay on compiler built-in float<=>int conversion in generic
 * implementation, because "floating types float, double, and long double whose
 * radix is not specified by the C standard but is usually two"
 * ~https://gcc.gnu.org/onlinedocs/gcc/Decimal-Float.html
 *
 * \param src integer number to convert, it is int32_t to omit software float
 *            operations library inclusion by compiler, when in whole topology
 *            only external component needs float input.
 * \param pow additional exponent component,
 *            number of fractional bits in fixed point value.
 *            Use '0' for normal conversion to integers
 * \return (int32_t)src * 2**pow
 */
static int32_t _pcm_convert_f_to_i(int32_t src, int32_t pow)
{
	int32_t exponent, mantissa, dst;

	exponent = (src >> 23);
	exponent = (exponent & 0xFF) + pow - 127; /* exponential */
	mantissa = BIT(23) | (MASK(22, 0) & src); /* mantisa + 1.0 [Q9.22] */
	/* calculate power */
	dst = _pcm_shift(mantissa, exponent - 23);
	/* add 0.5 to round correctly but assert it doesn't lead to overflow */
	if (exponent - 22 < 9 || (src & BIT(31)) == BIT(31))
		dst += _pcm_shift(mantissa, exponent - 22) & 1;
	/* copy sign to dst */
	dst = (dst ^ (src >> 31)) + (int)((unsigned int)src >> 31);

	return dst;
}

/**
 * \brief convert fixed number to float
 *
 * Do not relay on compiler built-in float<=>int conversion in generic
 * implementation, because "floating types float, double, and long double whose
 * radix is not specified by the C standard but is usually two"
 * ~https://gcc.gnu.org/onlinedocs/gcc/Decimal-Float.html
 *
 * \param src integer number to convert
 * \param pow additional exponent component
 *            number of fractional bits in fixed point value.
 *            Use '0' for normal conversion to float
 * \return (float)(src * 2**pow), return type is int32_t to omit software float
 *          operations library inclusion by compiler, when in whole topology
 *          only external component needs float input
 */
static int32_t _pcm_convert_i_to_f(int32_t src, int32_t pow)
{
	int sign, mantissa, exponent, dst, abs_clz;

	if (src == 0)
		return 0;

	sign = src & BIT(31);
	abs_clz = clz(PCM_ABS32(src));
	exponent = (127 + 31 - abs_clz - pow) & 0xFF;
	mantissa = PCM_ABS32(src);
	mantissa = _pcm_shift(mantissa, 23 - 31 + abs_clz) & MASK(22, 0);
	dst = sign | (exponent << 23) | mantissa;

	return dst;
}

#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && (CONFIG_PCM_CONVERTER_FORMAT_S16LE || CONFIG_PCM_CONVERTER_FORMAT_S24LE || CONFIG_PCM_CONVERTER_FORMAT_S32LE) */

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE
static void pcm_convert_s16_to_f_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const int16_t *src = psrc;
	int32_t *dst = pdst; /* float */
	int i;

	/* s16 is in format Q1.15 so during */
	/* conversion subtract 15 from exponent */
	for (i = 0; i < samples; i++)
		dst[i] = _pcm_convert_i_to_f(src[i], 15);
}

static void pcm_convert_f_to_s16_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const int32_t *src = psrc; /* float */
	int16_t *dst = pdst;
	int i;

	/* s16 is in format Q1.15 so during */
	/* conversion add 15 from exponent */
	for (i = 0; i < samples; i++)
		dst[i] = sat_int16(_pcm_convert_f_to_i(src[i], 15));
}

static int pcm_convert_s16_to_f(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_s16_to_f_lin);
}

static int pcm_convert_f_to_s16(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_f_to_s16_lin);
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE */

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE
static void pcm_convert_s24_to_f_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const int32_t *src = psrc;
	int32_t *dst = pdst; /* float */
	int i;

	/* s24 is in format Q1.23 so during */
	/* conversion subtract 23 to exponent */
	for (i = 0; i < samples; i++)
		dst[i] = _pcm_convert_i_to_f(sign_extend_s24(src[i]), 23);
}

static void pcm_convert_f_to_s24_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const int32_t *src = psrc; /* float */
	int32_t *dst = pdst;
	int i;

	/* s24 is in format Q1.23 so during */
	/* conversion add 23 to exponent */
	for (i = 0; i < samples; i++)
		dst[i] = sat_int24(_pcm_convert_f_to_i(src[i], 23));
}

static int pcm_convert_s24_to_f(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_s24_to_f_lin);
}

static int pcm_convert_f_to_s24(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_f_to_s24_lin);
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE */

#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE
static void pcm_convert_s32_to_f_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const int32_t *src = psrc;
	int32_t *dst = pdst; /* float */
	int i;

	/* s32 is in format Q1.31 so during */
	/* conversion subtract 31 to exponent */
	for (i = 0; i < samples; i++)
		dst[i] = _pcm_convert_i_to_f(src[i], 31);
}

static void pcm_convert_f_to_s32_lin(const void *psrc, void *pdst,
				     uint32_t samples)
{
	const int32_t *src = psrc; /* float */
	int32_t *dst = pdst;
	int i;

	/* s32 is in format Q1.31 so during */
	/* conversion add 31 to exponent */
	for (i = 0; i < samples; i++)
		dst[i] = _pcm_convert_f_to_i(src[i], 31);
}

static int pcm_convert_s32_to_f(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_s32_to_f_lin);
}

static int pcm_convert_f_to_s32(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t samples)
{
	return pcm_convert_as_linear(source, ioffset, sink, ooffset, samples,
				     pcm_convert_f_to_s32_lin);
}
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE */

const struct pcm_func_map pcm_func_map[] = {
#if CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, audio_stream_copy },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S16LE */
#if CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, audio_stream_copy },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S24LE */
#if  CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, pcm_convert_s16_to_s24 },
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, pcm_convert_s24_to_s16 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE */
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, audio_stream_copy },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S32LE */
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, pcm_convert_s16_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, pcm_convert_s32_to_s16 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE */
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, pcm_convert_s24_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, pcm_convert_s32_to_s24 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE */
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_FLOAT, audio_stream_copy },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT */
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_FLOAT, pcm_convert_s16_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S16_LE, pcm_convert_f_to_s16 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S16LE */
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_FLOAT, pcm_convert_s24_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S24_4LE, pcm_convert_f_to_s24 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S24LE */
#if CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_FLOAT, pcm_convert_s32_to_f },
	{ SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE, pcm_convert_f_to_s32 },
#endif /* CONFIG_PCM_CONVERTER_FORMAT_FLOAT && CONFIG_PCM_CONVERTER_FORMAT_S32LE */
};

const size_t pcm_func_count = ARRAY_SIZE(pcm_func_map);

#endif
