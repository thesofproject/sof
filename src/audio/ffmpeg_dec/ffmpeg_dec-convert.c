// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Fast-path PCM format conversion and interleaving for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/format.h>
#include <rtos/string.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <libavutil/samplefmt.h>
#include "ffmpeg_dec-convert.h"

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

	return (int16_t)Q_SHIFT(q, 31, 15);
}

static inline int32_t f64_to_q31_scalar(double d)
{
	if (d >= 1.0)
		return INT32_MAX;
	if (d <= -1.0)
		return INT32_MIN;
	return sat_int32((int64_t)(d * (double)((int64_t)1 << 31)));
}

/* -------------------------------------------------------------------------
 * Fast Path: FLTP (Planar Float) -> S32_LE / S24_4LE
 * ------------------------------------------------------------------------- */
static void conv_fltp_to_s32(const AVFrame *frame, int32_t *dst,
			     int channels, int nb_samples)
{
	const float *const *src = (const float *const *)frame->data;
	int i = ffmpeg_hifi_fltp_to_s32(src, dst, channels, nb_samples);

	if (channels == 1) {
		const float *c0 = src[0];

		for (; i < nb_samples; i++)
			dst[i] = f32_to_q31_scalar(c0[i]);
		return;
	}

	if (channels == 2) {
		const float *c0 = src[0];
		const float *c1 = src[1];

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
		for (; i < nb_samples; i++) {
			dst[2 * i + 0] = f32_to_q31_scalar(c0[i]);
			dst[2 * i + 1] = f32_to_q31_scalar(c1[i]);
		}
		return;
	}

	for (; i < nb_samples; i++) {
		for (int c = 0; c < channels; c++)
			dst[i * channels + c] = f32_to_q31_scalar(src[c][i]);
	}
}

static void conv_fltp_to_s24(const AVFrame *frame, int32_t *dst,
			     int channels, int nb_samples)
{
	const float *const *src = (const float *const *)frame->data;

	for (int i = 0; i < nb_samples; i++) {
		for (int c = 0; c < channels; c++) {
			int32_t q = f32_to_q31_scalar(src[c][i]);

			dst[i * channels + c] = Q_SHIFT(q, 31, 23);
		}
	}
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

	for (int i = 0; i < nb_samples; i++)
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

	for (; i + 4 <= nb_samples; i += 4) {
		dst[2 * i + 0] = Q_SHIFT_LEFT((int32_t)c0[i + 0], 15, 31);
		dst[2 * i + 1] = Q_SHIFT_LEFT((int32_t)c1[i + 0], 15, 31);
		dst[2 * i + 2] = Q_SHIFT_LEFT((int32_t)c0[i + 1], 15, 31);
		dst[2 * i + 3] = Q_SHIFT_LEFT((int32_t)c1[i + 1], 15, 31);
		dst[2 * i + 4] = Q_SHIFT_LEFT((int32_t)c0[i + 2], 15, 31);
		dst[2 * i + 5] = Q_SHIFT_LEFT((int32_t)c1[i + 2], 15, 31);
		dst[2 * i + 6] = Q_SHIFT_LEFT((int32_t)c0[i + 3], 15, 31);
		dst[2 * i + 7] = Q_SHIFT_LEFT((int32_t)c1[i + 3], 15, 31);
	}
	for (; i < nb_samples; i++) {
		dst[2 * i + 0] = Q_SHIFT_LEFT((int32_t)c0[i], 15, 31);
		dst[2 * i + 1] = Q_SHIFT_LEFT((int32_t)c1[i], 15, 31);
	}
}

/* -------------------------------------------------------------------------
 * Fast Path: S16 (Interleaved S16) -> S32_LE (Interleaved S32, shift << 16)
 * ------------------------------------------------------------------------- */
static void conv_s16_to_s32_interleaved(const int16_t *src, int32_t *dst, int total_samples)
{
	int i = 0;

	for (; i + 4 <= total_samples; i += 4) {
		dst[i + 0] = Q_SHIFT_LEFT((int32_t)src[i + 0], 15, 31);
		dst[i + 1] = Q_SHIFT_LEFT((int32_t)src[i + 1], 15, 31);
		dst[i + 2] = Q_SHIFT_LEFT((int32_t)src[i + 2], 15, 31);
		dst[i + 3] = Q_SHIFT_LEFT((int32_t)src[i + 3], 15, 31);
	}
	for (; i < total_samples; i++)
		dst[i] = Q_SHIFT_LEFT((int32_t)src[i], 15, 31);
}

/* -------------------------------------------------------------------------
 * Fast Path: S32P (Planar S32) -> S32_LE (Interleaved S32)
 * ------------------------------------------------------------------------- */
static void conv_s32p_to_s32_stereo(const AVFrame *frame, int32_t *dst, int nb_samples)
{
	const int32_t *c0 = (const int32_t *)frame->data[0];
	const int32_t *c1 = (const int32_t *)frame->data[1];
	int i = ffmpeg_hifi_s32p_to_s32_stereo(c0, c1, dst, nb_samples);

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
 * Fallback: Generic Sample-by-Sample Conversion (AVFrame -> SOF PCM)
 * ------------------------------------------------------------------------- */
static inline int32_t src_sample_to_q31(const uint8_t *p, int fmt)
{
	switch (fmt) {
	case AV_SAMPLE_FMT_U8:
	case AV_SAMPLE_FMT_U8P:
		return Q_SHIFT_LEFT((int32_t)*p - 0x80, 7, 31);
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		return Q_SHIFT_LEFT((int32_t)*(const int16_t *)p, 15, 31);
	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_S32P:
		return *(const int32_t *)p;
	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_FLTP:
		return f32_to_q31_scalar(*(const float *)p);
	case AV_SAMPLE_FMT_DBL:
	case AV_SAMPLE_FMT_DBLP:
		return f64_to_q31_scalar(*(const double *)p);
	default:
		return 0;
	}
}

static inline void q31_to_sink_sample(uint8_t *p, int32_t v, int fmt)
{
	switch (fmt) {
	case SOF_IPC_FRAME_S16_LE:
		*(int16_t *)p = (int16_t)Q_SHIFT(v, 31, 15);
		break;
	case SOF_IPC_FRAME_S24_4LE:
		*(int32_t *)p = Q_SHIFT(v, 31, 23);
		break;
	case SOF_IPC_FRAME_FLOAT:
		*(float *)p = Q_CONVERT_QTOF(v, 31);
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
	int src_ch = frame->ch_layout.nb_channels;
	int i, ch;

	for (i = 0; i < frame->nb_samples; i++) {
		for (ch = 0; ch < channels; ch++) {
			int c = (src_ch == 1) ? 0 : ch;
			const uint8_t *src = planar ?
				frame->data[c] + (size_t)i * src_bps :
				frame->data[0] + ((size_t)i * src_ch + c) * src_bps;
			int32_t q = src_sample_to_q31(src, frame->format);

			q31_to_sink_sample(out + ((size_t)i * channels + ch) * sink_bps,
					   q, sink_fmt);
		}
	}
}

/* -------------------------------------------------------------------------
 * Input Conversion: SOF Interleaved PCM -> AVFrame (FLTP / DBL)
 * ------------------------------------------------------------------------- */
static void conv_s16_to_fltp(const int16_t *src, float *const *dst,
			     int channels, int nb_samples)
{
	int i = 0;

	if (channels == 2) {
		float *dst0 = dst[0];
		float *dst1 = dst[1];

		for (; i + 4 <= nb_samples; i += 4) {
			dst0[i + 0] = Q_CONVERT_QTOF(src[(i + 0) * 2 + 0], 15);
			dst1[i + 0] = Q_CONVERT_QTOF(src[(i + 0) * 2 + 1], 15);
			dst0[i + 1] = Q_CONVERT_QTOF(src[(i + 1) * 2 + 0], 15);
			dst1[i + 1] = Q_CONVERT_QTOF(src[(i + 1) * 2 + 1], 15);
			dst0[i + 2] = Q_CONVERT_QTOF(src[(i + 2) * 2 + 0], 15);
			dst1[i + 2] = Q_CONVERT_QTOF(src[(i + 2) * 2 + 1], 15);
			dst0[i + 3] = Q_CONVERT_QTOF(src[(i + 3) * 2 + 0], 15);
			dst1[i + 3] = Q_CONVERT_QTOF(src[(i + 3) * 2 + 1], 15);
		}
		for (; i < nb_samples; i++) {
			dst0[i] = Q_CONVERT_QTOF(src[i * 2 + 0], 15);
			dst1[i] = Q_CONVERT_QTOF(src[i * 2 + 1], 15);
		}
		return;
	}

	for (; i < nb_samples; i++) {
		for (int c = 0; c < channels; c++)
			dst[c][i] = Q_CONVERT_QTOF(src[i * channels + c], 15);
	}
}

static void conv_s24_to_fltp(const int32_t *src, float *const *dst,
			     int channels, int nb_samples)
{
	for (int i = 0; i < nb_samples; i++) {
		for (int c = 0; c < channels; c++) {
			int32_t s = sign_extend_s24(src[i * channels + c]);

			dst[c][i] = Q_CONVERT_QTOF(s, 23);
		}
	}
}

static void conv_s32_to_fltp(const int32_t *src, float *const *dst,
			     int channels, int nb_samples)
{
	int i = ffmpeg_hifi_s32_to_fltp(src, dst, channels, nb_samples);

	if (channels == 2) {
		float *dst0 = dst[0];
		float *dst1 = dst[1];

		for (; i + 4 <= nb_samples; i += 4) {
			dst0[i + 0] = Q_CONVERT_QTOF(src[(i + 0) * 2 + 0], 31);
			dst1[i + 0] = Q_CONVERT_QTOF(src[(i + 0) * 2 + 1], 31);
			dst0[i + 1] = Q_CONVERT_QTOF(src[(i + 1) * 2 + 0], 31);
			dst1[i + 1] = Q_CONVERT_QTOF(src[(i + 1) * 2 + 1], 31);
			dst0[i + 2] = Q_CONVERT_QTOF(src[(i + 2) * 2 + 0], 31);
			dst1[i + 2] = Q_CONVERT_QTOF(src[(i + 2) * 2 + 1], 31);
			dst0[i + 3] = Q_CONVERT_QTOF(src[(i + 3) * 2 + 0], 31);
			dst1[i + 3] = Q_CONVERT_QTOF(src[(i + 3) * 2 + 1], 31);
		}
		for (; i < nb_samples; i++) {
			dst0[i] = Q_CONVERT_QTOF(src[i * 2 + 0], 31);
			dst1[i] = Q_CONVERT_QTOF(src[i * 2 + 1], 31);
		}
		return;
	}

	for (; i < nb_samples; i++) {
		for (int c = 0; c < channels; c++)
			dst[c][i] = Q_CONVERT_QTOF(src[i * channels + c], 31);
	}
}

static inline int16_t pcm_frame_to_s16_mono(const uint8_t *in, int idx,
					    int src_fmt, int src_ch)
{
	int32_t acc = 0;
	int c;

	if (src_fmt == SOF_IPC_FRAME_S16_LE) {
		const int16_t *s16 = (const int16_t *)in + (size_t)idx * src_ch;

		if (src_ch == 1)
			return s16[0];
		for (c = 0; c < src_ch; c++)
			acc += s16[c];
		return (int16_t)(acc / src_ch);
	}

	if (src_fmt == SOF_IPC_FRAME_S24_4LE) {
		const int32_t *s24 = (const int32_t *)in + (size_t)idx * src_ch;

		for (c = 0; c < src_ch; c++)
			acc += Q_SHIFT(sign_extend_s24(s24[c]), 23, 15);
		return (int16_t)(acc / src_ch);
	}

	{
		const int32_t *s32 = (const int32_t *)in + (size_t)idx * src_ch;

		for (c = 0; c < src_ch; c++)
			acc += Q_SHIFT(s32[c], 31, 15);
		return (int16_t)(acc / src_ch);
	}
}

void ffmpeg_dec_pcm_decimate_3to1_s16_mono(const uint8_t *in, int16_t *dst,
					   int src_fmt, int src_ch,
					   int nb_out_samples,
					   int16_t hist[FFMPEG_DECIM_3TO1_HIST_LEN])
{
	int i, k;

	for (i = 0; i < nb_out_samples; i++) {
		int16_t x0 = pcm_frame_to_s16_mono(in, 3 * i + 0, src_fmt, src_ch);
		int16_t x1 = pcm_frame_to_s16_mono(in, 3 * i + 1, src_fmt, src_ch);
		int16_t x2 = pcm_frame_to_s16_mono(in, 3 * i + 2, src_fmt, src_ch);
		/* 13-sample window: hist[0..11] followed by x0, x1, x2 (shifted by 3) */
		int32_t s0  = hist[2];
		int32_t s2  = hist[4];
		int32_t s3  = hist[5];
		int32_t s4  = hist[6];
		int32_t s5  = hist[7];
		int32_t s6  = hist[8];
		int32_t s7  = hist[9];
		int32_t s8  = hist[10];
		int32_t s9  = hist[11];
		int32_t s10 = x0;
		int32_t s12 = x2;
		int32_t acc;

		/*
		 * 13-tap symmetric linear-phase FIR low-pass filter
		 * h = [-208, 0, 1096, 2272, 4344, 5840, 6080, 5840, 4344, 2272, 1096, 0, -208]
		 * Unity DC gain (32768), exact notch at 12 kHz, -30 dB at 16 kHz.
		 */
		acc = -208 * (s0 + s12) +
		      1096 * (s2 + s10) +
		      2272 * (s3 + s9) +
		      4344 * (s4 + s8) +
		      5840 * (s5 + s7) +
		      6080 * s6;
		acc = (acc + 16384) >> 15;
		dst[i] = sat_int16(acc);

		for (k = 0; k < 9; k++)
			hist[k] = hist[k + 3];
		hist[9]  = x0;
		hist[10] = x1;
		hist[11] = x2;
	}
}

int ffmpeg_dec_pcm_to_frame(const uint8_t *in, AVFrame *frame,
			    int src_fmt, int channels, int nb_samples)
{
	int dst_ch;

	if (!in || !frame || channels <= 0 || nb_samples <= 0)
		return -EINVAL;

	dst_ch = frame->ch_layout.nb_channels;
	if (dst_ch != channels && !(dst_ch == 1 && frame->format == AV_SAMPLE_FMT_S16))
		return -EINVAL;

	if (frame->format == AV_SAMPLE_FMT_S16) {
		int16_t *dst = (int16_t *)frame->data[0];

		if (dst_ch == 1 && channels > 1) {
			for (int i = 0; i < nb_samples; i++)
				dst[i] = pcm_frame_to_s16_mono(in, i, src_fmt, channels);
			return 0;
		}

		size_t total = (size_t)nb_samples * dst_ch;

		if (src_fmt == SOF_IPC_FRAME_S16_LE) {
			memcpy_s(dst, total * sizeof(int16_t),
				 in, total * sizeof(int16_t));
			return 0;
		}
		if (src_fmt == SOF_IPC_FRAME_S24_4LE) {
			const int32_t *src24 = (const int32_t *)in;

			for (size_t i = 0; i < total; i++)
				dst[i] = (int16_t)Q_SHIFT(sign_extend_s24(src24[i]), 23, 15);
			return 0;
		}
		if (src_fmt == SOF_IPC_FRAME_S32_LE) {
			const int32_t *src32 = (const int32_t *)in;

			for (size_t i = 0; i < total; i++)
				dst[i] = (int16_t)Q_SHIFT(src32[i], 31, 15);
			return 0;
		}
		return -EINVAL;
	}

	if (frame->format == AV_SAMPLE_FMT_S16P) {
		if (src_fmt == SOF_IPC_FRAME_S16_LE) {
			const int16_t *src16 = (const int16_t *)in;

			for (int c = 0; c < dst_ch; c++) {
				int16_t *out = (int16_t *)frame->data[c];

				for (int i = 0; i < nb_samples; i++)
					out[i] = src16[i * dst_ch + c];
			}
			return 0;
		}
		if (src_fmt == SOF_IPC_FRAME_S24_4LE) {
			const int32_t *src24 = (const int32_t *)in;

			for (int c = 0; c < dst_ch; c++) {
				int16_t *out = (int16_t *)frame->data[c];

				for (int i = 0; i < nb_samples; i++) {
					int32_t v = sign_extend_s24(src24[i * dst_ch + c]);

					out[i] = (int16_t)Q_SHIFT(v, 23, 15);
				}
			}
			return 0;
		}
		if (src_fmt == SOF_IPC_FRAME_S32_LE) {
			const int32_t *src32 = (const int32_t *)in;

			for (int c = 0; c < dst_ch; c++) {
				int16_t *out = (int16_t *)frame->data[c];

				for (int i = 0; i < nb_samples; i++)
					out[i] = (int16_t)Q_SHIFT(src32[i * dst_ch + c], 31, 15);
			}
			return 0;
		}
		return -EINVAL;
	}

	if (frame->format == AV_SAMPLE_FMT_S32) {
		int32_t *dst = (int32_t *)frame->data[0];
		size_t total = (size_t)nb_samples * dst_ch;

		if (src_fmt == SOF_IPC_FRAME_S16_LE) {
			const int16_t *src16 = (const int16_t *)in;

			for (size_t i = 0; i < total; i++)
				dst[i] = Q_SHIFT_LEFT((int32_t)src16[i], 15, 31);
			return 0;
		}
		if (src_fmt == SOF_IPC_FRAME_S24_4LE) {
			const int32_t *src24 = (const int32_t *)in;

			for (size_t i = 0; i < total; i++)
				dst[i] = Q_SHIFT_LEFT(sign_extend_s24(src24[i]), 23, 31);
			return 0;
		}
		if (src_fmt == SOF_IPC_FRAME_S32_LE) {
			memcpy_s(dst, total * sizeof(int32_t),
				 in, total * sizeof(int32_t));
			return 0;
		}
		return -EINVAL;
	}

	if (frame->format == AV_SAMPLE_FMT_S32P) {
		if (src_fmt == SOF_IPC_FRAME_S16_LE) {
			const int16_t *src16 = (const int16_t *)in;

			for (int c = 0; c < dst_ch; c++) {
				int32_t *out = (int32_t *)frame->data[c];

				for (int i = 0; i < nb_samples; i++) {
					int32_t v = src16[i * dst_ch + c];

					out[i] = Q_SHIFT_LEFT(v, 15, 31);
				}
			}
			return 0;
		}
		if (src_fmt == SOF_IPC_FRAME_S24_4LE) {
			const int32_t *src24 = (const int32_t *)in;

			for (int c = 0; c < dst_ch; c++) {
				int32_t *out = (int32_t *)frame->data[c];

				for (int i = 0; i < nb_samples; i++) {
					int32_t v = sign_extend_s24(src24[i * dst_ch + c]);

					out[i] = Q_SHIFT_LEFT(v, 23, 31);
				}
			}
			return 0;
		}
		if (src_fmt == SOF_IPC_FRAME_S32_LE) {
			const int32_t *src32 = (const int32_t *)in;

			for (int c = 0; c < dst_ch; c++) {
				int32_t *out = (int32_t *)frame->data[c];

				for (int i = 0; i < nb_samples; i++)
					out[i] = src32[i * dst_ch + c];
			}
			return 0;
		}
		return -EINVAL;
	}

	if (frame->format == AV_SAMPLE_FMT_DBL) {
		double *dst = (double *)frame->data[0];
		int total = nb_samples * channels;

		switch (src_fmt) {
		case SOF_IPC_FRAME_S16_LE: {
			const int16_t *src16 = (const int16_t *)in;

			for (int i = 0; i < total; i++)
				dst[i] = Q_CONVERT_QTOD(src16[i], 15);
			return 0;
		}
		case SOF_IPC_FRAME_S24_4LE: {
			const int32_t *src24 = (const int32_t *)in;

			for (int i = 0; i < total; i++)
				dst[i] = Q_CONVERT_QTOD(sign_extend_s24(src24[i]), 23);
			return 0;
		}
		case SOF_IPC_FRAME_S32_LE: {
			const int32_t *src32 = (const int32_t *)in;

			for (int i = 0; i < total; i++)
				dst[i] = Q_CONVERT_QTOD(src32[i], 31);
			return 0;
		}
		default:
			return -EINVAL;
		}
	}

	if (frame->format == AV_SAMPLE_FMT_FLTP) {
		float *const *dst = (float *const *)frame->data;

		switch (src_fmt) {
		case SOF_IPC_FRAME_S16_LE:
			conv_s16_to_fltp((const int16_t *)in, dst, channels, nb_samples);
			return 0;
		case SOF_IPC_FRAME_S24_4LE:
			conv_s24_to_fltp((const int32_t *)in, dst, channels, nb_samples);
			return 0;
		case SOF_IPC_FRAME_S32_LE:
			conv_s32_to_fltp((const int32_t *)in, dst, channels, nb_samples);
			return 0;
		default:
			return -EINVAL;
		}
	}

	return -EINVAL;
}

/* -------------------------------------------------------------------------
 * Top-Level Output Dispatcher: AVFrame -> SOF Interleaved PCM
 * ------------------------------------------------------------------------- */
int ffmpeg_dec_convert_frame(const AVFrame *frame, uint8_t *out, size_t out_size,
			     int sink_fmt, int channels)
{
	int sink_bps = (sink_fmt == SOF_IPC_FRAME_S16_LE) ? 2 : 4;
	size_t need = (size_t)frame->nb_samples * channels * sink_bps;

	if (channels <= 0 || frame->nb_samples <= 0)
		return -EINVAL;

	/*
	 * The frame's channel count comes from the bitstream, @channels from the
	 * sink. Allow mono (nb_channels == 1) to upmix to @channels via conv_generic.
	 */
	if (frame->ch_layout.nb_channels != channels &&
	    frame->ch_layout.nb_channels != 1)
		return -EINVAL;
	if (need > out_size)
		return -ENOSPC;

	if (frame->ch_layout.nb_channels != channels) {
		conv_generic(frame, out, sink_fmt, channels);
		return (int)need;
	}

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

	/* 2. Planar Float (FLTP) -> S32_LE / S24_4LE for any channel count */
	if (frame->format == AV_SAMPLE_FMT_FLTP) {
		if (sink_fmt == SOF_IPC_FRAME_S32_LE) {
			conv_fltp_to_s32(frame, (int32_t *)out, channels, frame->nb_samples);
			return (int)need;
		}
		if (sink_fmt == SOF_IPC_FRAME_S24_4LE) {
			conv_fltp_to_s24(frame, (int32_t *)out, channels, frame->nb_samples);
			return (int)need;
		}
	}

	/* 3. Stereo Fast Paths (the dominant audio decoding configuration) */
	if (channels == 2) {
		/* AAC / Opus / afftdn (FLTP) -> S16_LE */
		if (frame->format == AV_SAMPLE_FMT_FLTP &&
		    sink_fmt == SOF_IPC_FRAME_S16_LE) {
			conv_fltp_to_s16_stereo(frame, (int16_t *)out, frame->nb_samples);
			return (int)need;
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

	/* 4. Mono Fast Paths */
	if (channels == 1) {
		if (frame->format == AV_SAMPLE_FMT_FLTP || frame->format == AV_SAMPLE_FMT_FLT) {
			if (sink_fmt == SOF_IPC_FRAME_S32_LE) {
				conv_fltp_to_s32(frame, (int32_t *)out, 1, frame->nb_samples);
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

	/* 5. General Fallback */
	conv_generic(frame, out, sink_fmt, channels);
	return (int)need;
}
