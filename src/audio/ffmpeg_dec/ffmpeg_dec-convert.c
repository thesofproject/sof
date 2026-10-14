// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Fast-path PCM format conversion and interleaving for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/string.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <libavutil/samplefmt.h>
#include "ffmpeg_dec-convert.h"

#if defined(__XTENSA__) && defined(__has_include)
#if __has_include(<xtensa/config/core-isa.h>)
#include <xtensa/config/core-isa.h>
#endif

#if defined(__has_builtin) && __has_builtin(__builtin_xtensa_trunc_sx2) && \
	defined(XCHAL_HAVE_HIFI4_VFPU) && XCHAL_HAVE_HIFI4_VFPU && \
	__has_include(<xtensahifiintrin.h>)
#include <xtensahifiintrin.h>
#define FFMPEG_DEC_CONV_VFPU 1
#endif

#if defined(XCHAL_HAVE_HIFI3) && XCHAL_HAVE_HIFI3 && __has_include(<xtensa/tie/xt_hifi3.h>)
#include <xtensa/tie/xt_hifi3.h>
#define FFMPEG_DEC_CONV_HIFI3 1
#endif
#endif

/*
 * Branch-free soft-float-free float -> Q1.31 for f in [-1.0, 1.0).
 *
 * Multiplying an IEEE-754 float by 2^31 is exact: it only adjusts the
 * exponent. We can extract the 24-bit significand and shift it directly,
 * avoiding libgcc soft-float emulation calls entirely on targets without
 * scalar hardware FP0.
 */
static inline int32_t f32_to_q31_scalar(float f)
{
	union { float f; uint32_t u; } v = { .f = f };
	uint32_t u = v.u;
	uint32_t sign = u >> 31;
	int e = (int)((u >> 23) & 0xFF);
	uint32_t mant = (u & 0x7FFFFFU) | 0x800000U;	/* 24-bit significand */
	int shift = e - 119;
	int32_t q;

	if (e >= 127)					/* |f| >= 1.0 -> saturate */
		return sign ? INT32_MIN : INT32_MAX;
	if (shift >= 0)
		q = (int32_t)(mant << shift);
	else if (shift > -32)
		q = (int32_t)(mant >> (-shift));
	else
		q = 0;
	return sign ? -q : q;
}

static inline int16_t f32_to_s16_scalar(float f)
{
	int32_t q = f32_to_q31_scalar(f);
	return (int16_t)(q >> 16);
}

/* -------------------------------------------------------------------------
 * Fast Path: FLTP (Planar Float) -> S32_LE
 * ------------------------------------------------------------------------- */
static void conv_fltp_to_s32_stereo(const AVFrame *frame, int32_t *dst, int nb_samples)
{
	const float *c0 = (const float *)frame->data[0];
	const float *c1 = (const float *)frame->data[1];
	int i = 0;

#if defined(FFMPEG_DEC_CONV_VFPU)
	for (; i + 2 <= nb_samples; i += 2) {
		ae_int32x2 f0 = AE_L32X2_I((const ae_int32x2 *)&c0[i], 0);
		ae_int32x2 f1 = AE_L32X2_I((const ae_int32x2 *)&c1[i], 0);
		ae_int32x2 q0 = XT_TRUNC_SX2((ae_xtfloatx2)f0, 31);
		ae_int32x2 q1 = XT_TRUNC_SX2((ae_xtfloatx2)f1, 31);
		int32_t tmp0[2] __attribute__((aligned(8)));
		int32_t tmp1[2] __attribute__((aligned(8)));

		AE_S32X2_I(q0, tmp0, 0);
		AE_S32X2_I(q1, tmp1, 0);

		dst[2 * i + 0] = tmp0[0];
		dst[2 * i + 1] = tmp1[0];
		dst[2 * i + 2] = tmp0[1];
		dst[2 * i + 3] = tmp1[1];
	}
#else
	for (; i + 4 <= nb_samples; i += 4) {
		dst[2 * i + 0] = f32_to_q31_scalar(c0[i + 0]);
		dst[2 * i + 1] = f32_to_q31_scalar(c1[i + 0]);
		dst[2 * i + 2] = f32_to_q31_scalar(c0[i + 1]);
		dst[2 * i + 3] = f32_to_q31_scalar(c1[i + 1]);
		dst[2 * i + 4] = f32_to_q31_scalar(c0[i + 2]);
		dst[2 * i + 5] = f32_to_q31_scalar(c1[i + 2]);
		dst[2 * i + 6] = f32_to_q31_scalar(c0[i + 3]);
		dst[2 * i + 7] = f32_to_q31_scalar(c1[i + 3]);
	}
#endif
	for (; i < nb_samples; i++) {
		dst[2 * i + 0] = f32_to_q31_scalar(c0[i]);
		dst[2 * i + 1] = f32_to_q31_scalar(c1[i]);
	}
}

static void conv_fltp_to_s32_mono(const AVFrame *frame, int32_t *dst, int nb_samples)
{
	const float *c0 = (const float *)frame->data[0];
	int i = 0;

#if defined(FFMPEG_DEC_CONV_VFPU)
	for (; i + 2 <= nb_samples; i += 2) {
		ae_int32x2 f = AE_L32X2_I((const ae_int32x2 *)&c0[i], 0);
		ae_int32x2 q = XT_TRUNC_SX2((ae_xtfloatx2)f, 31);
		AE_S32X2_I(q, (ae_int32x2 *)&dst[i], 0);
	}
#endif
	for (; i < nb_samples; i++)
		dst[i] = f32_to_q31_scalar(c0[i]);
}

/* -------------------------------------------------------------------------
 * Fast Path: FLTP (Planar Float) -> S16_LE
 * ------------------------------------------------------------------------- */
static void conv_fltp_to_s16_stereo(const AVFrame *frame, int16_t *dst, int nb_samples)
{
	const float *c0 = (const float *)frame->data[0];
	const float *c1 = (const float *)frame->data[1];
	int i = 0;

	for (; i + 4 <= nb_samples; i += 4) {
		dst[2 * i + 0] = f32_to_s16_scalar(c0[i + 0]);
		dst[2 * i + 1] = f32_to_s16_scalar(c1[i + 0]);
		dst[2 * i + 2] = f32_to_s16_scalar(c0[i + 1]);
		dst[2 * i + 3] = f32_to_s16_scalar(c1[i + 1]);
		dst[2 * i + 4] = f32_to_s16_scalar(c0[i + 2]);
		dst[2 * i + 5] = f32_to_s16_scalar(c1[i + 2]);
		dst[2 * i + 6] = f32_to_s16_scalar(c0[i + 3]);
		dst[2 * i + 7] = f32_to_s16_scalar(c1[i + 3]);
	}
	for (; i < nb_samples; i++) {
		dst[2 * i + 0] = f32_to_s16_scalar(c0[i]);
		dst[2 * i + 1] = f32_to_s16_scalar(c1[i]);
	}
}

static void conv_fltp_to_s16_mono(const AVFrame *frame, int16_t *dst, int nb_samples)
{
	const float *c0 = (const float *)frame->data[0];
	int i = 0;

	for (; i < nb_samples; i++)
		dst[i] = f32_to_s16_scalar(c0[i]);
}

/* -------------------------------------------------------------------------
 * Fast Path: S16P (Planar S16) -> S16_LE (Interleaved S16)
 * ------------------------------------------------------------------------- */
static void conv_s16p_to_s16_stereo(const AVFrame *frame, int16_t *dst, int nb_samples)
{
	const int16_t *c0 = (const int16_t *)frame->data[0];
	const int16_t *c1 = (const int16_t *)frame->data[1];
	int i = 0;

	for (; i + 4 <= nb_samples; i += 4) {
		dst[2 * i + 0] = c0[i + 0];
		dst[2 * i + 1] = c1[i + 0];
		dst[2 * i + 2] = c0[i + 1];
		dst[2 * i + 3] = c1[i + 1];
		dst[2 * i + 4] = c0[i + 2];
		dst[2 * i + 5] = c1[i + 2];
		dst[2 * i + 6] = c0[i + 3];
		dst[2 * i + 7] = c1[i + 3];
	}
	for (; i < nb_samples; i++) {
		dst[2 * i + 0] = c0[i];
		dst[2 * i + 1] = c1[i];
	}
}

/* -------------------------------------------------------------------------
 * Fast Path: S16P (Planar S16) -> S32_LE (Interleaved S32, shift << 16)
 * ------------------------------------------------------------------------- */
static void conv_s16p_to_s32_stereo(const AVFrame *frame, int32_t *dst, int nb_samples)
{
	const int16_t *c0 = (const int16_t *)frame->data[0];
	const int16_t *c1 = (const int16_t *)frame->data[1];
	int i = 0;

#if defined(FFMPEG_DEC_CONV_HIFI3)
	for (; i + 2 <= nb_samples; i += 2) {
		dst[2 * i + 0] = (int32_t)c0[i + 0] << 16;
		dst[2 * i + 1] = (int32_t)c1[i + 0] << 16;
		dst[2 * i + 2] = (int32_t)c0[i + 1] << 16;
		dst[2 * i + 3] = (int32_t)c1[i + 1] << 16;
	}
#else
	for (; i + 4 <= nb_samples; i += 4) {
		dst[2 * i + 0] = (int32_t)c0[i + 0] << 16;
		dst[2 * i + 1] = (int32_t)c1[i + 0] << 16;
		dst[2 * i + 2] = (int32_t)c0[i + 1] << 16;
		dst[2 * i + 3] = (int32_t)c1[i + 1] << 16;
		dst[2 * i + 4] = (int32_t)c0[i + 2] << 16;
		dst[2 * i + 5] = (int32_t)c1[i + 2] << 16;
		dst[2 * i + 6] = (int32_t)c0[i + 3] << 16;
		dst[2 * i + 7] = (int32_t)c1[i + 3] << 16;
	}
#endif
	for (; i < nb_samples; i++) {
		dst[2 * i + 0] = (int32_t)c0[i] << 16;
		dst[2 * i + 1] = (int32_t)c1[i] << 16;
	}
}

/* -------------------------------------------------------------------------
 * Fast Path: S16 (Interleaved S16) -> S32_LE (Interleaved S32, shift << 16)
 * ------------------------------------------------------------------------- */
static void conv_s16_to_s32_interleaved(const int16_t *src, int32_t *dst, int total_samples)
{
	int i = 0;

	for (; i + 4 <= total_samples; i += 4) {
		dst[i + 0] = (int32_t)src[i + 0] << 16;
		dst[i + 1] = (int32_t)src[i + 1] << 16;
		dst[i + 2] = (int32_t)src[i + 2] << 16;
		dst[i + 3] = (int32_t)src[i + 3] << 16;
	}
	for (; i < total_samples; i++)
		dst[i] = (int32_t)src[i] << 16;
}

/* -------------------------------------------------------------------------
 * Fast Path: S32P (Planar S32) -> S32_LE (Interleaved S32)
 * ------------------------------------------------------------------------- */
static void conv_s32p_to_s32_stereo(const AVFrame *frame, int32_t *dst, int nb_samples)
{
	const int32_t *c0 = (const int32_t *)frame->data[0];
	const int32_t *c1 = (const int32_t *)frame->data[1];
	int i = 0;

#if defined(FFMPEG_DEC_CONV_HIFI3)
	for (; i + 2 <= nb_samples; i += 2) {
		ae_int32x2 v0 = AE_L32X2_I((const ae_int32x2 *)&c0[i], 0);
		ae_int32x2 v1 = AE_L32X2_I((const ae_int32x2 *)&c1[i], 0);
		ae_int32x2 pair0 = AE_SEL32_LL(v0, v1);
		ae_int32x2 pair1 = AE_SEL32_HH(v0, v1);

		AE_S32X2_I(pair0, (ae_int32x2 *)&dst[2 * i + 0], 0);
		AE_S32X2_I(pair1, (ae_int32x2 *)&dst[2 * i + 2], 0);
	}
#else
	for (; i + 4 <= nb_samples; i += 4) {
		dst[2 * i + 0] = c0[i + 0];
		dst[2 * i + 1] = c1[i + 0];
		dst[2 * i + 2] = c0[i + 1];
		dst[2 * i + 3] = c1[i + 1];
		dst[2 * i + 4] = c0[i + 2];
		dst[2 * i + 5] = c1[i + 2];
		dst[2 * i + 6] = c0[i + 3];
		dst[2 * i + 7] = c1[i + 3];
	}
#endif
	for (; i < nb_samples; i++) {
		dst[2 * i + 0] = c0[i];
		dst[2 * i + 1] = c1[i];
	}
}

/* -------------------------------------------------------------------------
 * Fallback: Generic Sample-by-Sample Conversion
 * ------------------------------------------------------------------------- */
static inline int32_t src_sample_to_q31(const uint8_t *p, int fmt)
{
	switch (fmt) {
	case AV_SAMPLE_FMT_U8:
	case AV_SAMPLE_FMT_U8P:
		return ((int32_t)*p - 0x80) << 24;
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		return (int32_t)*(const int16_t *)p << 16;
	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_S32P:
		return *(const int32_t *)p;
	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_FLTP:
		return f32_to_q31_scalar(*(const float *)p);
	default:
		return 0;
	}
}

static inline void q31_to_sink_sample(uint8_t *p, int32_t v, int fmt)
{
	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		*(int16_t *)p = (int16_t)(v >> 16);
		break;
	case SOF_IPC_FRAME_S24_4LE:
		*(int32_t *)p = v >> 8;
		break;
	case SOF_IPC_FRAME_FLOAT:
		*(float *)p = (float)v / 2147483648.0f;
		break;
	case SOF_IPC_FRAME_S32_LE:
	default:
		*(int32_t *)p = v;
		break;
	}
}

static void conv_generic(const AVFrame *frame, uint8_t *out, int sink_fmt, int channels)
{
	int src_bps = av_get_bytes_per_sample(frame->format);
	int planar = av_sample_fmt_is_planar(frame->format);
	int sink_bps = (sink_fmt == SOF_IPC_FRAME_S16_LE) ? 2 : 4;
	int i, ch;

	for (i = 0; i < frame->nb_samples; i++) {
		for (ch = 0; ch < channels; ch++) {
			const uint8_t *src = planar ?
				frame->data[ch] + (size_t)i * src_bps :
				frame->data[0] + ((size_t)i * channels + ch) * src_bps;
			int32_t q = src_sample_to_q31(src, frame->format);

			q31_to_sink_sample(out + ((size_t)i * channels + ch) * sink_bps,
					   q, sink_fmt);
		}
	}
}

/* -------------------------------------------------------------------------
 * Top-Level Dispatcher
 * ------------------------------------------------------------------------- */
int ffmpeg_dec_convert_frame(const AVFrame *frame, uint8_t *out, size_t out_size,
			     int sink_fmt, int channels)
{
	int sink_bps = (sink_fmt == SOF_IPC_FRAME_S16_LE) ? 2 : 4;
	size_t need = (size_t)frame->nb_samples * channels * sink_bps;

	if (channels <= 0 || frame->nb_samples <= 0)
		return -EINVAL;
	if (need > out_size)
		return -ENOSPC;

	/* 1. Direct copy if format, layout and bit-depth match identically */
	if (channels == 1 || !av_sample_fmt_is_planar(frame->format)) {
		if (frame->format == AV_SAMPLE_FMT_S16 && sink_fmt == SOF_IPC_FRAME_S16_LE) {
			memcpy_s(out, out_size, frame->data[0], need);
			return (int)need;
		}
		if (frame->format == AV_SAMPLE_FMT_S32 && sink_fmt == SOF_IPC_FRAME_S32_LE) {
			memcpy_s(out, out_size, frame->data[0], need);
			return (int)need;
		}
	}

	/* 2. Stereo Fast Paths (the dominant audio decoding configuration) */
	if (channels == 2) {
		/* AAC / Opus (FLTP) */
		if (frame->format == AV_SAMPLE_FMT_FLTP) {
			if (sink_fmt == SOF_IPC_FRAME_S32_LE) {
				conv_fltp_to_s32_stereo(frame, (int32_t *)out, frame->nb_samples);
				return (int)need;
			}
			if (sink_fmt == SOF_IPC_FRAME_S16_LE) {
				conv_fltp_to_s16_stereo(frame, (int16_t *)out, frame->nb_samples);
				return (int)need;
			}
		}

		/* MP3 / FLAC (S16P) */
		if (frame->format == AV_SAMPLE_FMT_S16P) {
			if (sink_fmt == SOF_IPC_FRAME_S16_LE) {
				conv_s16p_to_s16_stereo(frame, (int16_t *)out, frame->nb_samples);
				return (int)need;
			}
			if (sink_fmt == SOF_IPC_FRAME_S32_LE) {
				conv_s16p_to_s32_stereo(frame, (int32_t *)out, frame->nb_samples);
				return (int)need;
			}
		}

		/* MP3 Interleaved S16 -> S32_LE */
		if (frame->format == AV_SAMPLE_FMT_S16 && sink_fmt == SOF_IPC_FRAME_S32_LE) {
			conv_s16_to_s32_interleaved((const int16_t *)frame->data[0],
						    (int32_t *)out,
						    frame->nb_samples * 2);
			return (int)need;
		}

		/* 24-bit/32-bit FLAC (S32P) -> S32_LE */
		if (frame->format == AV_SAMPLE_FMT_S32P && sink_fmt == SOF_IPC_FRAME_S32_LE) {
			conv_s32p_to_s32_stereo(frame, (int32_t *)out, frame->nb_samples);
			return (int)need;
		}
	}

	/* 3. Mono Fast Paths */
	if (channels == 1) {
		if (frame->format == AV_SAMPLE_FMT_FLTP || frame->format == AV_SAMPLE_FMT_FLT) {
			if (sink_fmt == SOF_IPC_FRAME_S32_LE) {
				conv_fltp_to_s32_mono(frame, (int32_t *)out, frame->nb_samples);
				return (int)need;
			}
			if (sink_fmt == SOF_IPC_FRAME_S16_LE) {
				conv_fltp_to_s16_mono(frame, (int16_t *)out, frame->nb_samples);
				return (int)need;
			}
		}
		if ((frame->format == AV_SAMPLE_FMT_S16 || frame->format == AV_SAMPLE_FMT_S16P) &&
		    sink_fmt == SOF_IPC_FRAME_S32_LE) {
			conv_s16_to_s32_interleaved((const int16_t *)frame->data[0],
						    (int32_t *)out,
						    frame->nb_samples);
			return (int)need;
		}
	}

	/* 4. General Fallback */
	conv_generic(frame, out, sink_fmt, channels);
	return (int)need;
}
