// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <ipc4/mixin_mixout.h>
#include <sof/audio/mixer.h>
#include <sof/common.h>

LOG_MODULE_DECLARE(mixer, CONFIG_SOF_LOG_LEVEL);

#ifdef MIXIN_MIXOUT_HIFI3

#if CONFIG_FORMAT_S16LE
/* Instead of using audio_stream_get_channels(sink) and audio_stream_get_channels(source),
 * sink_channel_count and source_channel_count are supplied as parameters. This is done to reuse
 * the function to also mix an entire stream. In this case the function is called with fake stream
 * parameters: multichannel stream is treated as single channel and so the entire stream
 * contents is mixed.
 */
static void normal_mix_channel_s16(struct audio_stream __sparse_cache *sink, int32_t start_frame,
				   int32_t mixed_frames,
				   const struct audio_stream __sparse_cache *source,
				   int32_t frame_count, uint16_t gain)
{
	int frames_to_mix, frames_to_copy, left_frames;
	int n, nmax, i, m, left;
	ae_int16x4 in_sample;
	ae_int16x4 out_sample;
	ae_int16x4 *in;
	ae_int16x4 *out;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu1 = AE_ZALIGN64();
	ae_valign outu2 = AE_ZALIGN64();
	/* audio_stream_wrap() is required and is done below in a loop */
	ae_int16 *dst = (ae_int16 *)audio_stream_get_wptr(sink) + start_frame;
	ae_int16 *src = audio_stream_get_rptr(source);

	assert(mixed_frames >= start_frame);
	frames_to_mix = AE_MIN_32_signed(mixed_frames - start_frame, frame_count);
	frames_to_copy = frame_count - frames_to_mix;
	n = 0;

	for (left_frames = frames_to_mix; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src + n);
		dst = audio_stream_wrap(sink, dst + n);
		/* calculate the remaining samples*/
		nmax = audio_stream_samples_without_wrap_s16(source, src);
		n = AE_MIN_32_signed(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s16(sink, dst);
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

	for (left_frames = frames_to_copy; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src + n);
		dst = audio_stream_wrap(sink, dst + n);
		/* calculate the remaining samples*/
		nmax = audio_stream_samples_without_wrap_s16(source, src);
		n = AE_MIN_32_signed(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s16(sink, dst);
		n = AE_MIN_32_signed(n, nmax);
		in = (ae_int16x4 *)src;
		out = (ae_int16x4 *)dst;
		inu = AE_LA64_PP(in);
		m = n >> 2;
		left = n & 0x03;
		/* process 4 frames per loop */
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

static void remap_mix_channel_s16(struct audio_stream __sparse_cache *sink,
				  int32_t sink_channel_index,  int32_t sink_channel_count,
				  int32_t start_frame, int32_t mixed_frames,
				  const struct audio_stream __sparse_cache *source,
				  int32_t source_channel_index, int32_t source_channel_count,
				  int32_t frame_count, uint16_t gain)
{
	ae_int16 *dst, *src;
	int frames_to_mix, frames_to_copy, left_frames;
	int n, nmax, frames, i;
	int inoff = source_channel_count * sizeof(ae_int16);
	int outoff = sink_channel_count * sizeof(ae_int16);
	ae_int16x4 in;
	ae_int16x4 out;
	ae_int16 *pgain = (ae_int16 *)&gain;
	ae_int16x4 gain_v;
	ae_int32x2 temp, out1;

	dst = (ae_int16 *)audio_stream_get_wptr(sink) + start_frame * sink_channel_count +
		sink_channel_index;
	src = (ae_int16 *)audio_stream_get_rptr(source) + source_channel_index;
	src = audio_stream_wrap(source, src);

	assert(mixed_frames >= start_frame);
	frames_to_mix = AE_MIN_32_signed(mixed_frames - start_frame, frame_count);
	frames_to_copy = frame_count - frames_to_mix;

	/* store gain to a AE_DR register gain_v*/
	AE_L16_IP(gain_v, pgain, 0);

	/* set source as circular buffer, hifi3 only have 1 circular buffer*/
	AE_SETCBEGIN0(audio_stream_get_addr(source));
	AE_SETCEND0(audio_stream_get_end_addr(source));

	for (left_frames = frames_to_mix; left_frames > 0; left_frames -= frames) {
		/* audio_stream_wrap() is required and is done below in a loop */
		dst = audio_stream_wrap(sink, dst);

		/* calculate the remaining samples of sink*/
		nmax = audio_stream_samples_without_wrap_s16(sink, dst);
		n = AE_MIN_32_signed(left_frames * sink_channel_count, nmax);

		/* frames is the processed frame count in this loop*/
		frames = 0;
		for (i = 0; i < n; i += source_channel_count) {
			AE_L16_XC(in, src, inoff);
			AE_L16_XP(out, dst, 0);
			/* Q1.15 * Q1.15 to Q2.30*/
			temp = AE_MULF16SS_00(in, gain_v);
			temp = AE_SRAI32R(temp, IPC4_MIXIN_GAIN_SHIFT + 1);
			out1 = AE_SEXT32X2D16_10(out);
			temp = AE_ADD32S(temp, out1);
			temp = AE_SLAI32S(temp, 16);
			out = AE_ROUND16X4F32SSYM(temp, temp);
			AE_S16_0_XP(out, dst, outoff);
			frames++;
		}
	}

	for (left_frames = frames_to_copy; left_frames > 0; left_frames -= frames) {
		dst = audio_stream_wrap(sink, dst);
		nmax = audio_stream_samples_without_wrap_s16(sink, dst);
		n = AE_MIN_32_signed(left_frames * sink_channel_count, nmax);
		frames = 0;

		for (i = 0; i < n; i += source_channel_count) {
			AE_L16_XC(in, src, inoff);
			AE_L16_XP(out, dst, 0);
			temp = AE_MULF16SS_00(in, gain_v);
			temp = AE_SRAI32R(temp, IPC4_MIXIN_GAIN_SHIFT + 1);
			temp = AE_SLAI32S(temp, 16);
			out = AE_ROUND16X4F32SSYM(temp, temp);
			AE_S16_0_XP(out, dst, outoff);
			frames++;
		}
	}
}

static void mute_channel_s16(struct audio_stream __sparse_cache *stream, int32_t channel_index,
			     int32_t start_frame, int32_t mixed_frames, int32_t frame_count)
{
	int skip_mixed_frames, left_frames;
	int off = audio_stream_get_channels(stream) * sizeof(ae_int16);
	ae_int16 *ptr;
	ae_int16x4 zero = AE_ZERO16();

	assert(mixed_frames >= start_frame);
	skip_mixed_frames = mixed_frames - start_frame;

	if (frame_count <= skip_mixed_frames)
		return;
	frame_count -= skip_mixed_frames;

	AE_SETCBEGIN0(audio_stream_get_addr(stream));
	AE_SETCEND0(audio_stream_get_end_addr(stream));

	/* audio_stream_wrap() is needed here and it is just below in a loop */
	ptr = (ae_int16 *)audio_stream_get_wptr(stream) +
		mixed_frames * audio_stream_get_channels(stream) +
		channel_index;
	ptr = audio_stream_wrap(stream, ptr);

	for (left_frames = frame_count ; left_frames; left_frames--)
		AE_S16_0_XC(zero, ptr, off);
}
#endif	/* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/* Instead of using audio_stream_get_channels(sink) and audio_stream_get_channels(source),
 * sink_channel_count and source_channel_count are supplied as parameters. This is done to reuse
 * the function to also mix an entire stream. In this case the function is called with fake stream
 * parameters: multichannel stream is treated as single channel and so the entire stream
 * contents is mixed.
 */
static void normal_mix_channel_s24(struct audio_stream __sparse_cache *sink, int32_t start_frame,
				   int32_t mixed_frames,
				   const struct audio_stream __sparse_cache *source,
				   int32_t frame_count, uint16_t gain)
{
	int frames_to_mix, frames_to_copy, left_frames;
	int n, nmax, i, m, left;
	ae_int32x2 in_sample;
	ae_int32x2 out_sample;
	ae_int32x2 *in;
	ae_int32x2 *out;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu1 = AE_ZALIGN64();
	ae_valign outu2 = AE_ZALIGN64();
	/* audio_stream_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)audio_stream_get_wptr(sink) + start_frame;
	int32_t *src = audio_stream_get_rptr(source);

	assert(mixed_frames >= start_frame);
	frames_to_mix = AE_MIN_32_signed(mixed_frames - start_frame, frame_count);
	frames_to_copy = frame_count - frames_to_mix;
	n = 0;

	for (left_frames = frames_to_mix; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src + n);
		dst = audio_stream_wrap(sink, dst + n);
		/* calculate the remaining samples*/
		nmax = audio_stream_samples_without_wrap_s24(source, src);
		n = AE_MIN_32_signed(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s24(sink, dst);
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
			out_sample = AE_ADD24S(in_sample, out_sample);
			AE_SA32X2_IP(out_sample, outu2, out);
		}
		AE_SA64POS_FP(outu2, out);

		/* process the left sample to avoid memory access overrun */
		if (left) {
			AE_L32_IP(in_sample, (ae_int32 *)in, sizeof(ae_int32));
			AE_L32_IP(out_sample, (ae_int32 *)out, 0);
			out_sample = AE_ADD24S(in_sample, out_sample);
			AE_S32_L_IP(out_sample, (ae_int32 *)out, sizeof(ae_int32));
		}
	}

	for (left_frames = frames_to_copy; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src + n);
		dst = audio_stream_wrap(sink, dst + n);
		nmax = audio_stream_samples_without_wrap_s24(source, src);
		n = AE_MIN_32_signed(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s24(sink, dst);
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

static void remap_mix_channel_s24(struct audio_stream __sparse_cache *sink,
				  int32_t sink_channel_index,  int32_t sink_channel_count,
				  int32_t start_frame, int32_t mixed_frames,
				  const struct audio_stream __sparse_cache *source,
				  int32_t source_channel_index, int32_t source_channel_count,
				  int32_t frame_count, uint16_t gain)
{
	int frames_to_mix, frames_to_copy, left_frames;
	int n, nmax, i, frames;
	int inoff = source_channel_count * sizeof(ae_int32);
	int outoff = sink_channel_count * sizeof(ae_int32);
	ae_int32x2 in;
	ae_int32x2 out;
	ae_int64 tmp;
	ae_int16 *pgain = (ae_int16 *)&gain;
	ae_int16x4 gain_v;
	ae_int32 *dst, *src;

	dst = (ae_int32 *)audio_stream_get_wptr(sink) + start_frame * sink_channel_count +
		sink_channel_index;
	src = (ae_int32 *)audio_stream_get_rptr(source) + source_channel_index;
	src = audio_stream_wrap(source, src);
	assert(mixed_frames >= start_frame);
	frames_to_mix = AE_MIN_32_signed(mixed_frames - start_frame, frame_count);
	frames_to_copy = frame_count - frames_to_mix;

	/* store gain to a AE_DR register gain_v*/
	AE_L16_IP(gain_v, pgain, 0);
	/* set source as circular buffer, hifi3 only have 1 circular buffer*/
	AE_SETCBEGIN0(audio_stream_get_addr(source));
	AE_SETCEND0(audio_stream_get_end_addr(source));

	for (left_frames = frames_to_mix; left_frames > 0; left_frames -= frames) {
		dst = audio_stream_wrap(sink, dst);
		/* calculate the remaining samples*/
		nmax = audio_stream_samples_without_wrap_s24(sink, dst);
		n = AE_MIN_32_signed(left_frames * sink_channel_count, nmax);
		/* frames is the processed frame count in this loop*/
		frames = 0;

		for (i = 0; i < n; i += source_channel_count) {
			AE_L32_XC(in, src, inoff);
			/*shift the significant 8 bits to the left*/
			in = AE_SLAI32(in, 8);
			AE_L32_XP(out, dst, 0);
			out = AE_SLAI32(out, 8);
			out = AE_SRAI32(out, 8);
			tmp = AE_MUL32X16_H0(in, gain_v);

			/* shift should be IPC4_MIXIN_GAIN_SHIFT + 8(shift right for in)
			 * - 16(to keep the valid LSB bits of ae_int64)
			 */
			in = AE_ROUND32F48SSYM(AE_SRAI64(tmp, IPC4_MIXIN_GAIN_SHIFT - 8));
			out = AE_ADD32S(out, in);
			out = AE_SLAI32S(out, 8);
			out = AE_SRAI32(out, 8);
			AE_S32_L_XP(out, dst, outoff);
			frames++;
		}
	}

	for (left_frames = frames_to_copy; left_frames > 0; left_frames -= frames) {
		dst = audio_stream_wrap(sink, dst);
		nmax = audio_stream_samples_without_wrap_s24(sink, dst);
		n = AE_MIN_32_signed(left_frames * sink_channel_count, nmax);
		frames = 0;

		for (i = 0; i < n; i += source_channel_count) {
			AE_L32_XC(in, src, inoff);
			/*shift the significant bit to the left*/
			in = AE_SLAI32(in, 8);
			tmp = AE_MUL32X16_H0(in, gain_v);
			out = AE_ROUND32F48SSYM(AE_SRAI64(tmp, IPC4_MIXIN_GAIN_SHIFT - 8));
			out = AE_SLAI32S(out, 8);
			out = AE_SRAI32(out, 8);
			AE_S32_L_XP(out, dst, outoff);
			frames++;
		}
	}
}

#endif	/* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/* Instead of using audio_stream_get_channels(sink) and audio_stream_get_channels(source),
 * sink_channel_count and source_channel_count are supplied as parameters. This is done to reuse
 * the function to also mix an entire stream. In this case the function is called with fake stream
 * parameters: multichannel stream is treated as single channel and so the entire stream
 * contents is mixed.
 */
static void normal_mix_channel_s32(struct audio_stream __sparse_cache *sink, int32_t start_frame,
				   int32_t mixed_frames,
				   const struct audio_stream __sparse_cache *source,
				   int32_t frame_count, uint16_t gain)
{
	int frames_to_mix, frames_to_copy, left_frames;
	int n, nmax, i, m, left;
	ae_int32x2 in_sample;
	ae_int32x2 out_sample;
	ae_int32x2 *in;
	ae_int32x2 *out;
	ae_valign inu = AE_ZALIGN64();
	ae_valign outu1 = AE_ZALIGN64();
	ae_valign outu2 = AE_ZALIGN64();
	/* audio_stream_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)audio_stream_get_wptr(sink) + start_frame;
	int32_t *src = audio_stream_get_rptr(source);

	assert(mixed_frames >= start_frame);
	frames_to_mix = AE_MIN_32_signed(mixed_frames - start_frame, frame_count);
	frames_to_copy = frame_count - frames_to_mix;
	n = 0;

	for (left_frames = frames_to_mix; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src + n);
		dst = audio_stream_wrap(sink, dst + n);
		/* calculate the remaining samples*/
		nmax = audio_stream_samples_without_wrap_s32(source, src);
		n = AE_MIN_32_signed(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, dst);
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

	for (left_frames = frames_to_copy; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src + n);
		dst = audio_stream_wrap(sink, dst + n);
		/* calculate the remaining samples*/
		nmax = audio_stream_samples_without_wrap_s32(source, src);
		n = AE_MIN_32_signed(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, dst);
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

static void remap_mix_channel_s32(struct audio_stream __sparse_cache *sink,
				  int32_t sink_channel_index,  int32_t sink_channel_count,
				  int32_t start_frame, int32_t mixed_frames,
				  const struct audio_stream __sparse_cache *source,
				  int32_t source_channel_index, int32_t source_channel_count,
				  int32_t frame_count, uint16_t gain)
{
	int inoff = source_channel_count * sizeof(ae_int32);
	int outoff = sink_channel_count * sizeof(ae_int32);
	ae_int32x2 in;
	ae_int32x2 out;
	ae_int64 tmp, tmp1;
	ae_int16 *pgain = (ae_int16 *)&gain;
	ae_int16x4 gain_v;
	int32_t frames_to_mix, frames_to_copy, left_frames;
	int32_t n, nmax, frames, i;
	ae_int32 *dst, *src;

	/* audio_stream_wrap() is required and is done below in a loop */
	dst = (ae_int32 *)audio_stream_get_wptr(sink) + start_frame * sink_channel_count +
		sink_channel_index;
	src = (ae_int32 *)audio_stream_get_rptr(source) + source_channel_index;

	assert(mixed_frames >= start_frame);
	frames_to_mix = AE_MIN_32_signed(mixed_frames - start_frame, frame_count);
	frames_to_copy = frame_count - frames_to_mix;
	/* store gain to a AE_DR register gain_v*/
	AE_L16_IP(gain_v, pgain, 0);
	/* set source as circular buffer, hifi3 only have 1 circular buffer*/
	AE_SETCBEGIN0(audio_stream_get_addr(source));
	AE_SETCEND0(audio_stream_get_end_addr(source));
	src = audio_stream_wrap(source, src);

	for (left_frames = frames_to_mix; left_frames > 0; left_frames -= frames) {
		dst = audio_stream_wrap(sink, dst);
		nmax = audio_stream_samples_without_wrap_s32(sink, dst);
		n = AE_MIN_32_signed(left_frames * sink_channel_count, nmax);
		/* frames is the processed frame count in this loop*/
		frames = 0;

		for (i = 0; i < n; i += source_channel_count) {
			AE_L32_XC(in, src, inoff);
			AE_L32_XP(out, dst, 0);
			tmp = AE_MUL32X16_H0(in, gain_v);

			/* shift should be -(IPC4_MIXIN_GAIN_SHIFT
			 * - 16(to keep the valid LSB bits of ae_int64))
			 */
			tmp = AE_SLAI64S(tmp, 16 - IPC4_MIXIN_GAIN_SHIFT);
			tmp1 = AE_CVT48A32(out);
			tmp = AE_ADD64S(tmp, tmp1);
			out = AE_ROUND32F48SSYM(tmp);
			AE_S32_L_XP(out, dst, outoff);
			frames++;
		}
	}

	for (left_frames = frames_to_copy; left_frames > 0; left_frames -= frames) {
		dst = audio_stream_wrap(sink, dst);
		nmax = audio_stream_samples_without_wrap_s32(sink, dst);
		n = AE_MIN_32_signed(left_frames * sink_channel_count, nmax);
		/* frames is the processed frame count in this loop*/
		frames = 0;

		for (i = 0; i < n; i += source_channel_count) {
			AE_L32_XC(in, src, inoff);
			tmp = AE_MUL32X16_H0(in, gain_v);
			tmp = AE_SLAI64S(tmp, 16 - IPC4_MIXIN_GAIN_SHIFT);
			out = AE_ROUND32F48SSYM(tmp);
			AE_S32_L_XP(out, dst, outoff);
			frames++;
		}
	}
}

#endif	/* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S32LE || CONFIG_FORMAT_S24LE
static void mute_channel_s32(struct audio_stream __sparse_cache *stream, int32_t channel_index,
			     int32_t start_frame, int32_t mixed_frames, int32_t frame_count)
{
	int skip_mixed_frames, left_frames;
	ae_int32 *ptr;
	int off = audio_stream_get_channels(stream) * sizeof(ae_int32);
	ae_int32x2 zero = AE_ZERO32();

	assert(mixed_frames >= start_frame);
	skip_mixed_frames = mixed_frames - start_frame;

	if (frame_count <= skip_mixed_frames)
		return;
	frame_count -= skip_mixed_frames;

	AE_SETCBEGIN0(audio_stream_get_addr(stream));
	AE_SETCEND0(audio_stream_get_end_addr(stream));

	/* audio_stream_wrap() is needed here and it is just below in a loop */
	ptr = (ae_int32 *)audio_stream_get_wptr(stream) +
		mixed_frames * audio_stream_get_channels(stream) +
		channel_index;
	ptr = audio_stream_wrap(stream, ptr);

	for (left_frames = frame_count ; left_frames > 0; left_frames--)
		AE_S32_L_XC(zero, ptr, off);
}

#endif

const struct mix_func_map mix_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, normal_mix_channel_s16, remap_mix_channel_s16, mute_channel_s16},
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, normal_mix_channel_s24, remap_mix_channel_s24, mute_channel_s32},
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, normal_mix_channel_s32, remap_mix_channel_s32, mute_channel_s32}
#endif
};

const size_t mix_count = ARRAY_SIZE(mix_func_map);

#if __XCC__ && (XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4)

#include <xtensa/tie/xt_hifi3.h>

#if CONFIG_FORMAT_S16LE
/* Mix n 16 bit PCM source streams to one sink stream */
static void mix_n_s16(struct comp_dev *dev, struct audio_stream __sparse_cache *sink,
		      const struct audio_stream __sparse_cache **sources, uint32_t num_sources,
		      uint32_t frames)
{
	ae_int16x4 * in[PLATFORM_MAX_CHANNELS];
	ae_int16x4 *out = audio_stream_get_wptr(sink);
	ae_int16x4 sample = AE_ZERO16();
	ae_int16x4 res = AE_ZERO16();
	ae_int32x2 val1;
	ae_int32x2 val2;
	ae_int32x2 sample_1;
	ae_int32x2 sample_2;
	unsigned int n, m, nmax, i, j, left_samples;
	unsigned int samples = frames * audio_stream_get_channels(sink);

	for (j = 0; j < num_sources; j++)
		in[j] = audio_stream_get_rptr(sources[j]);

	for (left_samples = samples; left_samples; left_samples -= n) {
		out = audio_stream_wrap(sink,  out);
		nmax = audio_stream_samples_without_wrap_s16(sink, out);
		n = MIN(left_samples, nmax);
		for (j = 0; j < num_sources; j++) {
			in[j] = audio_stream_wrap(sources[j], in[j]);
			nmax = audio_stream_samples_without_wrap_s16(sources[j], in[j]);
			n = MIN(n, nmax);
		}
		m = n >> 2;

		for (i = 0; i < m; i++) {
			val1 = AE_ZERO32();
			val2 = AE_ZERO32();
			for (j = 0; j < num_sources; j++) {
				/* load four 16 bit samples, 8 is sizeof(ae_int16x4) */
				AE_L16X4_IP(sample, in[j], 8);
				sample_1 = AE_SEXT32X2D16_32(sample);
				sample_2 = AE_SEXT32X2D16_10(sample);
				val1 = AE_ADD32S(val1, sample_1);
				val2 = AE_ADD32S(val2, sample_2);
			}
			/*Saturate to 16 bits */
			val1 = AE_SRAA32S(AE_SLAA32S(val1, 16), 16);
			val2 = AE_SRAA32S(AE_SLAA32S(val2, 16), 16);

			/* truncate the LSB 16bit of four 32-bit signed elements*/
			res = AE_CVT16X4(val1, val2);

			/* store four 16 bit samples, 8 is sizeof(ae_int16x4) */
			AE_S16X4_IP(res, out, 8);
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/* Mix n 24 bit PCM source streams to one sink stream */
static void mix_n_s24(struct comp_dev *dev, struct audio_stream __sparse_cache *sink,
		      const struct audio_stream __sparse_cache **sources, uint32_t num_sources,
		      uint32_t frames)
{
	ae_int32x2 *in[PLATFORM_MAX_CHANNELS];
	ae_int32x2 *out = audio_stream_get_wptr(sink);
	ae_int32x2 val;
	ae_int32x2 sample = AE_ZERO32();
	unsigned int n, m, nmax, i, j, left_samples;
	unsigned int samples = frames * audio_stream_get_channels(sink);

	for (j = 0; j < num_sources; j++)
		in[j] = audio_stream_get_rptr(sources[j]);

	for (left_samples = samples; left_samples; left_samples -= n) {
		out = audio_stream_wrap(sink,  out);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(left_samples, nmax);
		for (j = 0; j < num_sources; j++) {
			in[j] = audio_stream_wrap(sources[j], in[j]);
			nmax = audio_stream_samples_without_wrap_s32(sources[j], in[j]);
			n = MIN(n, nmax);
		}
		m = n >> 1;
		for (i = 0; i < m; i++) {
			val = AE_ZERO32();
			for (j = 0; j < num_sources; j++) {
				/* load two 32 bit samples, 8 is sizeof(ae_int32x2) */
				AE_L32X2_IP(sample, in[j], 8);
				/* Sign extend */
				sample = AE_SRAA32RS(AE_SLAI32(sample, 8), 8);
				val = AE_ADD32S(val, sample);
			}
			/*Saturate to 24 bits */
			val = AE_SRAA32S(AE_SLAA32S(val, 8), 8);

			/* store two 32 bit samples, 8 is sizeof(ae_int32x2) */
			AE_S32X2_IP(val, out, 8);
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/* Mix n 32 bit PCM source streams to one sink stream */
static void mix_n_s32(struct comp_dev *dev, struct audio_stream __sparse_cache *sink,
		      const struct audio_stream __sparse_cache **sources, uint32_t num_sources,
		      uint32_t frames)
{
	ae_q32s * in[PLATFORM_MAX_CHANNELS];
	ae_int32 *out = audio_stream_get_wptr(sink);
	ae_int64 sample;
	ae_int64 val;
	ae_int32x2 res;
	unsigned int n, nmax, i, j, left_samples;
	unsigned int m = 0;
	unsigned int samples = frames * audio_stream_get_channels(sink);

	for (j = 0; j < num_sources; j++)
		in[j] = audio_stream_get_rptr(sources[j]);

	for (left_samples = samples; left_samples; left_samples -= n) {
		out = audio_stream_wrap(sink,  out);
		nmax = audio_stream_samples_without_wrap_s32(sink, out);
		n = MIN(left_samples, nmax);
		for (j = 0; j < num_sources; j++) {
			in[j] = audio_stream_wrap(sources[j], in[j] + m);
			nmax = audio_stream_samples_without_wrap_s32(sources[j], in[j]);
			n = MIN(n, nmax);
		}
		/*record the processed samples for next address iteration */
		m = n;
		for (i = 0; i < m; i++) {
			val =  AE_ZERO64();
			for (j = 0; j < num_sources; j++) {
				/* load one 32 bit sample */
				sample = AE_L32M_X(in[j],  i * sizeof(ae_q32s));
				val = AE_ADD64S(val, sample);
			}
			/*Saturate to 32 bits */
			res =  AE_ROUND32X2F48SSYM(val, val);

			/* store one 32 bit samples */
			AE_S32_L_IP(res, out, sizeof(ae_int32));
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

const struct mixer_func_map mixer_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, mix_n_s16 },
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, mix_n_s24 },
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, mix_n_s32 },
#endif
};

const size_t mixer_func_count = ARRAY_SIZE(mixer_func_map);

#endif

#endif
