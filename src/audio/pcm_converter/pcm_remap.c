// SPDX-License-Identifier: BSD-3-Clause
/*
 * (c) 2024 Intel Corporation. All rights reserved.
 */

#include <sof/audio/pcm_converter.h>
#include <sof/audio/audio_stream.h>

static inline void mute_channel_c16(struct audio_stream *stream, int channel, int frames)
{
	int num_channels = audio_stream_get_channels(stream);
	int16_t *ptr = (int16_t *)audio_stream_get_wptr(stream) + channel;

	while (frames) {
		int samples_wo_wrap, n, i;

		ptr = audio_stream_wrap(stream, ptr);

		samples_wo_wrap = audio_stream_samples_without_wrap_s16(stream, ptr);
		n = samples_wo_wrap / num_channels;
		if (samples_wo_wrap % num_channels)
			n++;

		n = MIN(n, frames);

		for (i = 0; i < n; i++) {
			*ptr = 0;
			ptr += num_channels;
		}

		frames -= n;
	}
}

static inline void mute_channel_c32(struct audio_stream *stream, int channel, int frames)
{
	int num_channels = audio_stream_get_channels(stream);
	int32_t *ptr = (int32_t *)audio_stream_get_wptr(stream) + channel;

	while (frames) {
		int samples_wo_wrap, n, i;

		ptr = audio_stream_wrap(stream, ptr);

		samples_wo_wrap = audio_stream_samples_without_wrap_s32(stream, ptr);
		n = samples_wo_wrap / num_channels;
		if (samples_wo_wrap % num_channels)
			n++;

		n = MIN(n, frames);

		for (i = 0; i < n; i++) {
			*ptr = 0;
			ptr += num_channels;
		}

		frames -= n;
	}
}

static inline int remap_c16(const struct audio_stream *source, uint32_t dummy1,
		     struct audio_stream *sink, uint32_t dummy2,
		     uint32_t source_samples, uint32_t chmap)
{
	int src_channel, sink_channel;
	int num_src_channels = audio_stream_get_channels(source);
	int num_sink_channels = audio_stream_get_channels(sink);
	int frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		int16_t *src, *dst;
		int frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		if (src_channel == 0xf) {
			mute_channel_c16(sink, sink_channel, frames);
			continue;
		}

		assert(src_channel < num_src_channels);

		src = (int16_t *)audio_stream_get_rptr(source) + src_channel;
		dst = (int16_t *)audio_stream_get_wptr(sink) + sink_channel;

		frames_left = frames;

		while (frames_left) {
			int src_samples_wo_wrap, dst_samples_wo_wrap;
			int src_loops, dst_loops, n, i;

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			src_samples_wo_wrap = audio_stream_samples_without_wrap_s16(source, src);
			src_loops = src_samples_wo_wrap / num_src_channels;
			if (src_samples_wo_wrap % num_src_channels)
				src_loops++;

			dst_samples_wo_wrap = audio_stream_samples_without_wrap_s16(sink, dst);
			dst_loops = dst_samples_wo_wrap / num_sink_channels;
			if (dst_samples_wo_wrap % num_sink_channels)
				dst_loops++;

			n = MIN(src_loops, dst_loops);
			n = MIN(n, frames_left);

			for (i = 0; i < n; i++) {
				*dst = *src;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			frames_left -= n;
		}
	}

	return source_samples;
}

static inline int remap_c32_with_left_shift(const struct audio_stream *source,
		     struct audio_stream *sink,
		     uint32_t source_samples, uint32_t chmap,
			 int shift)
{
	int src_channel, sink_channel;
	int num_src_channels = audio_stream_get_channels(source);
	int num_sink_channels = audio_stream_get_channels(sink);
	int frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		int32_t *src, *dst;
		int frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		if (src_channel == 0xf) {
			mute_channel_c32(sink, sink_channel, frames);
			continue;
		}

		assert(src_channel < num_src_channels);

		src = (int32_t *)audio_stream_get_rptr(source) + src_channel;
		dst = (int32_t *)audio_stream_get_wptr(sink) + sink_channel;

		frames_left = frames;

		while (frames_left) {
			int src_samples_wo_wrap, dst_samples_wo_wrap;
			int src_loops, dst_loops, n, i;

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			src_samples_wo_wrap = audio_stream_samples_without_wrap_s32(source, src);
			src_loops = src_samples_wo_wrap / num_src_channels;
			if (src_samples_wo_wrap % num_src_channels)
				src_loops++;

			dst_samples_wo_wrap = audio_stream_samples_without_wrap_s32(sink, dst);
			dst_loops = dst_samples_wo_wrap / num_sink_channels;
			if (dst_samples_wo_wrap % num_sink_channels)
				dst_loops++;

			n = MIN(src_loops, dst_loops);
			n = MIN(n, frames_left);

			for (i = 0; i < n; i++) {
				*dst = *src << shift;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			frames_left -= n;
		}
	}

	return source_samples;
}

static inline int remap_c32_with_right_shift(const struct audio_stream *source,
		     struct audio_stream *sink,
		     uint32_t source_samples, uint32_t chmap,
			 int shift)
{
	int src_channel, sink_channel;
	int num_src_channels = audio_stream_get_channels(source);
	int num_sink_channels = audio_stream_get_channels(sink);
	int frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		int32_t *src, *dst;
		int frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		if (src_channel == 0xf) {
			mute_channel_c32(sink, sink_channel, frames);
			continue;
		}

		assert(src_channel < num_src_channels);

		src = (int32_t *)audio_stream_get_rptr(source) + src_channel;
		dst = (int32_t *)audio_stream_get_wptr(sink) + sink_channel;

		frames_left = frames;

		while (frames_left) {
			int src_samples_wo_wrap, dst_samples_wo_wrap;
			int src_loops, dst_loops, n, i;

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			src_samples_wo_wrap = audio_stream_samples_without_wrap_s32(source, src);
			src_loops = src_samples_wo_wrap / num_src_channels;
			if (src_samples_wo_wrap % num_src_channels)
				src_loops++;

			dst_samples_wo_wrap = audio_stream_samples_without_wrap_s32(sink, dst);
			dst_loops = dst_samples_wo_wrap / num_sink_channels;
			if (dst_samples_wo_wrap % num_sink_channels)
				dst_loops++;

			n = MIN(src_loops, dst_loops);
			n = MIN(n, frames_left);

			for (i = 0; i < n; i++) {
				*dst = *src >> shift;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			frames_left -= n;
		}
	}

	return source_samples;
}

static inline int remap_c16_to_c32(const struct audio_stream *source,
		     struct audio_stream *sink,
		     uint32_t source_samples, uint32_t chmap,
			 int shift)
{
	int src_channel, sink_channel;
	int num_src_channels = audio_stream_get_channels(source);
	int num_sink_channels = audio_stream_get_channels(sink);
	int frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		int16_t *src;
		int32_t *dst;
		int frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		if (src_channel == 0xf) {
			mute_channel_c32(sink, sink_channel, frames);
			continue;
		}

		assert(src_channel < num_src_channels);

		src = (int16_t *)audio_stream_get_rptr(source) + src_channel;
		dst = (int32_t *)audio_stream_get_wptr(sink) + sink_channel;

		frames_left = frames;

		while (frames_left) {
			int src_samples_wo_wrap, dst_samples_wo_wrap;
			int src_loops, dst_loops, n, i;

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			src_samples_wo_wrap = audio_stream_samples_without_wrap_s16(source, src);
			src_loops = src_samples_wo_wrap / num_src_channels;
			if (src_samples_wo_wrap % num_src_channels)
				src_loops++;

			dst_samples_wo_wrap = audio_stream_samples_without_wrap_s32(sink, dst);
			dst_loops = dst_samples_wo_wrap / num_sink_channels;
			if (dst_samples_wo_wrap % num_sink_channels)
				dst_loops++;

			n = MIN(src_loops, dst_loops);
			n = MIN(n, frames_left);

			for (i = 0; i < n; i++) {
				*dst = (int32_t)*src << shift;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			frames_left -= n;
		}
	}

	return source_samples;
}

static inline int remap_c32_to_c16(const struct audio_stream *source,
		     struct audio_stream *sink,
		     uint32_t source_samples, uint32_t chmap,
			 int shift)
{
	int src_channel, sink_channel;
	int num_src_channels = audio_stream_get_channels(source);
	int num_sink_channels = audio_stream_get_channels(sink);
	int frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		int32_t *src;
		int16_t *dst;
		int frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		if (src_channel == 0xf) {
			mute_channel_c16(sink, sink_channel, frames);
			continue;
		}

		assert(src_channel < num_src_channels);

		src = (int32_t *)audio_stream_get_rptr(source) + src_channel;
		dst = (int16_t *)audio_stream_get_wptr(sink) + sink_channel;

		frames_left = frames;

		while (frames_left) {
			int src_samples_wo_wrap, dst_samples_wo_wrap;
			int src_loops, dst_loops, n, i;

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			src_samples_wo_wrap = audio_stream_samples_without_wrap_s32(source, src);
			src_loops = src_samples_wo_wrap / num_src_channels;
			if (src_samples_wo_wrap % num_src_channels)
				src_loops++;

			dst_samples_wo_wrap = audio_stream_samples_without_wrap_s16(sink, dst);
			dst_loops = dst_samples_wo_wrap / num_sink_channels;
			if (dst_samples_wo_wrap % num_sink_channels)
				dst_loops++;

			n = MIN(src_loops, dst_loops);
			n = MIN(n, frames_left);

			for (i = 0; i < n; i++) {
				*dst = *src >> shift;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			frames_left -= n;
		}
	}

	return source_samples;
}

static int remap_c32(const struct audio_stream *source, uint32_t dummy1,
		     struct audio_stream *sink, uint32_t dummy2,
		     uint32_t source_samples, uint32_t chmap)
{
	return remap_c32_with_left_shift(source, sink, source_samples, chmap, 0);
}

static int remap_and_convert_32_to_16(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c32_to_c16(source, sink, source_samples, chmap, 16);
}

static int remap_and_convert_16_to_32(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c16_to_c32(source, sink, source_samples, chmap, 16);
}

static int remap_and_convert_24_to_16(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c32_to_c16(source, sink, source_samples, chmap, 8);
}

static int remap_and_convert_16_to_24(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c16_to_c32(source, sink, source_samples, chmap, 8);
}

static int remap_and_convert_32_to_24(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c32_with_right_shift(source, sink, source_samples, chmap, 8);
}

static int remap_and_convert_24_to_32(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c32_with_left_shift(source, sink, source_samples, chmap, 8);
}

static int remap_and_convert_32_to_16in32(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c32_with_right_shift(source, sink, source_samples, chmap, 16);
}

static int remap_and_convert_16in32_to_32(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c32_with_left_shift(source, sink, source_samples, chmap, 16);
}

static int remap_and_convert_16in32_to_16(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c32_to_c16(source, sink, source_samples, chmap, 0);
}

static int remap_and_convert_16_to_16in32(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	return remap_c16_to_c32(source, sink, source_samples, chmap, 0);
}
///!!! TODO: ADD ifdef for formats !!!
const struct pcm_func_map pcm_remap_func_map[] = {
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, remap_c16},
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, remap_c32},
	{ SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S24_4LE, remap_and_convert_32_to_24},
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE_MSB, remap_and_convert_24_to_32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, remap_c32},
	{ SOF_IPC_FRAME_S16_4LE, SOF_IPC_FRAME_S16_4LE, remap_c32},

	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, remap_and_convert_32_to_16},
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, remap_and_convert_16_to_32},

	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE_MSB, remap_and_convert_16_to_32},
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, remap_and_convert_16_to_24},
	{ SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S16_LE, remap_and_convert_32_to_16},
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, remap_and_convert_24_to_16},

	{ SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S32_LE, remap_c32},
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, remap_and_convert_24_to_32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE_MSB, remap_c32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, remap_and_convert_32_to_24},

	{ SOF_IPC_FRAME_S16_4LE, SOF_IPC_FRAME_S16_LE, remap_and_convert_16in32_to_16},
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_4LE, remap_and_convert_16_to_16in32},

	{ SOF_IPC_FRAME_S16_4LE, SOF_IPC_FRAME_S24_4LE, remap_and_convert_24_to_32},
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_4LE, remap_and_convert_32_to_24},

	{ SOF_IPC_FRAME_S16_4LE, SOF_IPC_FRAME_S32_LE, remap_and_convert_16in32_to_32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_4LE, remap_and_convert_32_to_16in32},

/*
#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		audio_stream_copy},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE,
		pcm_convert_s24_to_s32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		pcm_convert_s32_to_s24 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S24LE && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		pcm_convert_s16_to_s24 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE,
		pcm_convert_s24_to_s16 },
#endif
#if CONFIG_PCM_CONVERTER_FORMAT_S24_C24_AND_S24_C32
	{ SOF_IPC_FRAME_S24_3LE, SOF_IPC_FRAME_S24_3LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE,
		pcm_convert_s24_c24_to_s24_c32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_3LE, SOF_IPC_FRAME_S24_4LE,
		pcm_convert_s24_c32_to_s24_c24 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_3LE, SOF_IPC_FRAME_S24_3LE,
		pcm_convert_s24_c32_to_s24_c24_link_gtw },
#endif

#if CONFIG_PCM_CONVERTER_FORMAT_S24_4LE_MSB && CONFIG_PCM_CONVERTER_FORMAT_S24LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S24_4LE_MSB, pcm_convert_s24_to_s32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S24_4LE, pcm_convert_s32_to_s24},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S24_4LE_MSB, audio_stream_copy},
#endif

#if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S24_4LE_MSB
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S24_4LE_MSB, pcm_convert_s32_to_s24_be},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S32_LE, audio_stream_copy},
#endif

#if CONFIG_PCM_CONVERTER_FORMAT_S24_4LE_MSB && CONFIG_PCM_CONVERTER_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE,
		SOF_IPC_FRAME_S24_4LE_MSB, pcm_convert_s16_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S16_LE,
		SOF_IPC_FRAME_S16_LE, pcm_convert_s32_to_s16 },
#endif
*/
};

const size_t pcm_remap_func_count = ARRAY_SIZE(pcm_remap_func_map);

