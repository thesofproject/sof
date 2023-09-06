// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Andrula Song <xiaoyuan.song@intel.com>

#include <ipc4/mixin_mixout.h>
#include <sof/common.h>

#ifdef MIXIN_MIXOUT_HIFI3

#if CONFIG_FORMAT_S16LE
/* Instead of using audio_stream_get_channels(sink) and audio_stream_get_channels(source),
 * sink_channel_count and source_channel_count are supplied as parameters. This is done to reuse
 * the function to also mix an entire stream. In this case the function is called with fake stream
 * parameters: multichannel stream is treated as single channel and so the entire stream
 * contents is mixed.
 */
static void normal_mix_channel_s16(struct audio_stream *sink, int32_t start_frame,
				   int32_t mixed_frames,
				   const struct audio_stream *source,
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

static void mute_channel_s16(struct audio_stream *stream, int32_t channel_index,
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
static void normal_mix_channel_s24(struct audio_stream *sink, int32_t start_frame,
				   int32_t mixed_frames,
				   const struct audio_stream *source,
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

#endif	/* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/* Instead of using audio_stream_get_channels(sink) and audio_stream_get_channels(source),
 * sink_channel_count and source_channel_count are supplied as parameters. This is done to reuse
 * the function to also mix an entire stream. In this case the function is called with fake stream
 * parameters: multichannel stream is treated as single channel and so the entire stream
 * contents is mixed.
 */
static void normal_mix_channel_s32(struct audio_stream *sink, int32_t start_frame,
				   int32_t mixed_frames,
				   const struct audio_stream *source,
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

#endif	/* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S32LE || CONFIG_FORMAT_S24LE
static void mute_channel_s32(struct audio_stream *stream, int32_t channel_index,
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
	{ SOF_IPC_FRAME_S16_LE, normal_mix_channel_s16, mute_channel_s16},
#endif
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, normal_mix_channel_s24, mute_channel_s32},
#endif
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, normal_mix_channel_s32, mute_channel_s32}
#endif
};

const size_t mix_count = ARRAY_SIZE(mix_func_map);

#endif
