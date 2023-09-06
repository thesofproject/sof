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
	int32_t frames_to_mix, frames_to_copy, left_frames;
	int32_t n, nmax, i;

	/* audio_stream_wrap() is required and is done below in a loop */
	int16_t *dst = (int16_t *)audio_stream_get_wptr(sink) + start_frame;
	int16_t *src = audio_stream_get_rptr(source);

	assert(mixed_frames >= start_frame);
	frames_to_mix = mixed_frames - start_frame;
	frames_to_mix = MIN(frames_to_mix, frame_count);
	frames_to_copy = frame_count - frames_to_mix;

	for (left_frames = frames_to_mix; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);
		/* calculate the remaining samples*/
		nmax = audio_stream_samples_without_wrap_s16(source, src);
		n = MIN(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s16(sink, dst);
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*dst = sat_int16(*dst + *src++);
			dst++;
		}
	}

	for (left_frames = frames_to_copy; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);
		nmax = audio_stream_samples_without_wrap_s16(source, src);
		n = MIN(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s16(sink, dst);
		n = MIN(n, nmax);
		memcpy_s(dst, n * sizeof(int16_t), src, n * sizeof(int16_t));
		dst += n;
		src += n;
	}
}

static void mute_channel_s16(struct audio_stream *stream, int32_t channel_index,
			     int32_t start_frame, int32_t mixed_frames, int32_t frame_count)
{
	int32_t skip_mixed_frames, n, left_frames, i, channel_count, frames, samples;
	int16_t *ptr;

	assert(mixed_frames >= start_frame);
	skip_mixed_frames = mixed_frames - start_frame;

	if (frame_count <= skip_mixed_frames)
		return;
	frame_count -= skip_mixed_frames;
	channel_count = audio_stream_get_channels(stream);
	/* audio_stream_wrap() is needed here and it is just below in a loop */
	ptr = (int16_t *)audio_stream_get_wptr(stream) +
		mixed_frames * audio_stream_get_channels(stream) +
		channel_index;

	for (left_frames = frame_count; left_frames; left_frames -= frames) {
		ptr = audio_stream_wrap(stream, ptr);
		n = audio_stream_samples_without_wrap_s16(stream, ptr);
		samples = left_frames * channel_count;
		n = MIN(samples, n);
		frames = 0;
		for (i = 0; i < n; i += channel_count) {
			*ptr = 0;
			ptr += channel_count;
			frames++;
		}
	}
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
	int32_t frames_to_mix, frames_to_copy, left_frames;
	int32_t n, nmax, i;
	/* audio_stream_wrap() is required and is done below in a loop */
	int32_t *dst = (int32_t *)audio_stream_get_wptr(sink) + start_frame;
	int32_t *src = audio_stream_get_rptr(source);

	assert(mixed_frames >= start_frame);
	frames_to_mix = mixed_frames - start_frame;
	frames_to_mix = MIN(frames_to_mix, frame_count);
	frames_to_copy = frame_count - frames_to_mix;

	for (left_frames = frames_to_mix; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);
		/* calculate the remaining samples*/
		nmax = audio_stream_samples_without_wrap_s24(source, src);
		n = MIN(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s24(sink, dst);
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*dst = sat_int24(sign_extend_s24(*dst) + sign_extend_s24(*src++));
			dst++;
		}
	}

	for (left_frames = frames_to_copy; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);
		nmax = audio_stream_samples_without_wrap_s24(source, src);
		n = MIN(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s24(sink, dst);
		n = MIN(n, nmax);
		memcpy_s(dst, n * sizeof(int32_t), src, n * sizeof(int32_t));
		dst += n;
		src += n;
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
	int32_t frames_to_mix, frames_to_copy, left_frames;
	int32_t n, nmax, i;
	int32_t *dst = (int32_t *)audio_stream_get_wptr(sink) + start_frame;
	int32_t *src = audio_stream_get_rptr(source);

	assert(mixed_frames >= start_frame);
	frames_to_mix = mixed_frames - start_frame;
	frames_to_mix = MIN(frames_to_mix, frame_count);
	frames_to_copy = frame_count - frames_to_mix;

	for (left_frames = frames_to_mix; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);
		/* calculate the remaining samples*/
		nmax = audio_stream_samples_without_wrap_s32(source, src);
		n = MIN(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, dst);
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*dst = sat_int32((int64_t)*dst + (int64_t)*src++);
			dst++;
		}
	}

	for (left_frames = frames_to_copy; left_frames > 0; left_frames -= n) {
		src = audio_stream_wrap(source, src);
		dst = audio_stream_wrap(sink, dst);
		nmax = audio_stream_samples_without_wrap_s32(source, src);
		n = MIN(left_frames, nmax);
		nmax = audio_stream_samples_without_wrap_s32(sink, dst);
		n = MIN(n, nmax);
		memcpy_s(dst, n * sizeof(int32_t), src, n * sizeof(int32_t));
		dst += n;
		src += n;
	}
}

#endif	/* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S32LE || CONFIG_FORMAT_S24LE
static void mute_channel_s32(struct audio_stream *stream, int32_t channel_index,
			     int32_t start_frame, int32_t mixed_frames, int32_t frame_count)
{
	int32_t skip_mixed_frames, left_frames, n, channel_count, i, frames, samples;
	int32_t *ptr;

	assert(mixed_frames >= start_frame);
	skip_mixed_frames = mixed_frames - start_frame;

	if (frame_count <= skip_mixed_frames)
		return;
	frame_count -= skip_mixed_frames;
	channel_count = audio_stream_get_channels(stream);

	ptr = (int32_t *)audio_stream_get_wptr(stream) +
		mixed_frames * audio_stream_get_channels(stream) +
		channel_index;

	for (left_frames = frame_count; left_frames > 0; left_frames -= frames) {
		ptr = audio_stream_wrap(stream, ptr);
		n = audio_stream_samples_without_wrap_s32(stream, ptr);
		samples = left_frames * channel_count;
		n =  MIN(samples, n);
		frames = 0;
		for (i = 0; i < n; i += channel_count) {
			*ptr = 0;
			ptr += channel_count;
			frames++;
		}
	}
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
