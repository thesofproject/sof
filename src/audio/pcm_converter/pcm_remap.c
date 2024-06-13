// SPDX-License-Identifier: BSD-3-Clause
/*
 * (c) 2024 Intel Corporation. All rights reserved.
 */

#include <sof/audio/pcm_converter.h>
#include <sof/audio/audio_stream.h>

static inline void mute_channel_c16(struct audio_stream *stream, int channel, int frames)
{
	int16_t *ptr = audio_stream_get_wptr(stream);
	int num_channels = audio_stream_get_channels(stream);

	while (frames) {
		int i, n;

		assert(audio_stream_bytes_without_wrap(stream, ptr) %
				audio_stream_frame_bytes(stream) == 0);

		n = MIN(frames, audio_stream_frames_without_wrap(stream, ptr));

		for (i = 0; i < n; i++) {
			*(ptr + channel) = 0;
			ptr += num_channels;
		}

		ptr = audio_stream_wrap(stream, ptr);
		frames -= n;
	}
}

static inline void mute_channel_c32(struct audio_stream *stream, int channel, int frames)
{
	int32_t *ptr = audio_stream_get_wptr(stream);
	int num_channels = audio_stream_get_channels(stream);

	while (frames) {
		int i, n;

		assert(audio_stream_bytes_without_wrap(stream, ptr) %
				audio_stream_frame_bytes(stream) == 0);

		n = MIN(frames, audio_stream_frames_without_wrap(stream, ptr));

		for (i = 0; i < n; i++) {
			*(ptr + channel) = 0;
			ptr += num_channels;
		}

		ptr = audio_stream_wrap(stream, ptr);
		frames -= n;
	}
}

static int remap_c16(const struct audio_stream *source, uint32_t dummy1,
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

		src = audio_stream_get_rptr(source);
		dst = audio_stream_get_wptr(sink);

		frames_left = frames;

		while (frames_left) {
			int i, n;

			assert(audio_stream_bytes_without_wrap(source, src) %
					audio_stream_frame_bytes(source) == 0);
			assert(audio_stream_bytes_without_wrap(sink, dst) %
					audio_stream_frame_bytes(sink) == 0);

			n = MIN(frames_left, audio_stream_frames_without_wrap(source, src));
			n = MIN(n, audio_stream_frames_without_wrap(sink, dst));

			for (i = 0; i < n; i++) {
				*(dst + sink_channel) = *(src + src_channel);
				src += num_src_channels;
				dst += num_sink_channels;
			}

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			frames_left -= n;
		}
	}

	return source_samples;
}

static int remap_c32(const struct audio_stream *source, uint32_t dummy1,
		     struct audio_stream *sink, uint32_t dummy2,
		     uint32_t source_samples, uint32_t chmap)
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

		src = audio_stream_get_rptr(source);
		dst = audio_stream_get_wptr(sink);

		frames_left = frames;

		while (frames_left) {
			int i, n;

			assert(audio_stream_bytes_without_wrap(source, src) %
					audio_stream_frame_bytes(source) == 0);
			assert(audio_stream_bytes_without_wrap(sink, dst) %
					audio_stream_frame_bytes(sink) == 0);

			n = MIN(frames_left, audio_stream_frames_without_wrap(source, src));
			n = MIN(n, audio_stream_frames_without_wrap(sink, dst));

			for (i = 0; i < n; i++) {
				*(dst + sink_channel) = *(src + src_channel);
				src += num_src_channels;
				dst += num_sink_channels;
			}

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			frames_left -= n;
		}
	}

	return source_samples;
}

static int remap_and_convert_32_to_16(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
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

		src = audio_stream_get_rptr(source);
		dst = audio_stream_get_wptr(sink);

		frames_left = frames;

		while (frames_left) {
			int i, n;

			assert(audio_stream_bytes_without_wrap(source, src) %
					audio_stream_frame_bytes(source) == 0);
			assert(audio_stream_bytes_without_wrap(sink, dst) %
					audio_stream_frame_bytes(sink) == 0);

			n = MIN(frames_left, audio_stream_frames_without_wrap(source, src));
			n = MIN(n, audio_stream_frames_without_wrap(sink, dst));

			for (i = 0; i < n; i++) {
				*(dst + sink_channel) = *(src + src_channel) >> 16;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			frames_left -= n;
		}
	}

	return source_samples;
}

static int remap_and_convert_16_to_32(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
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

		src = audio_stream_get_rptr(source);
		dst = audio_stream_get_wptr(sink);

		frames_left = frames;

		while (frames_left) {
			int i, n;

			assert(audio_stream_bytes_without_wrap(source, src) %
					audio_stream_frame_bytes(source) == 0);
			assert(audio_stream_bytes_without_wrap(sink, dst) %
					audio_stream_frame_bytes(sink) == 0);

			n = MIN(frames_left, audio_stream_frames_without_wrap(source, src));
			n = MIN(n, audio_stream_frames_without_wrap(sink, dst));

			for (i = 0; i < n; i++) {
				*(dst + sink_channel) = (int32_t)*(src + src_channel) << 16;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			frames_left -= n;
		}
	}

	return source_samples;
}

static int remap_and_convert_24_to_16(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
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

		src = audio_stream_get_rptr(source);
		dst = audio_stream_get_wptr(sink);

		frames_left = frames;

		while (frames_left) {
			int i, n;

			assert(audio_stream_bytes_without_wrap(source, src) %
					audio_stream_frame_bytes(source) == 0);
			assert(audio_stream_bytes_without_wrap(sink, dst) %
					audio_stream_frame_bytes(sink) == 0);

			n = MIN(frames_left, audio_stream_frames_without_wrap(source, src));
			n = MIN(n, audio_stream_frames_without_wrap(sink, dst));

			for (i = 0; i < n; i++) {
				*(dst + sink_channel) = sign_extend_s24(*(src + src_channel)) >> 8;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			frames_left -= n;
		}
	}

	return source_samples;
}

static int remap_and_convert_16_to_24(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
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

		src = audio_stream_get_rptr(source);
		dst = audio_stream_get_wptr(sink);

		frames_left = frames;

		while (frames_left) {
			int i, n;

			assert(audio_stream_bytes_without_wrap(source, src) %
					audio_stream_frame_bytes(source) == 0);
			assert(audio_stream_bytes_without_wrap(sink, dst) %
					audio_stream_frame_bytes(sink) == 0);

			n = MIN(frames_left, audio_stream_frames_without_wrap(source, src));
			n = MIN(n, audio_stream_frames_without_wrap(sink, dst));

			for (i = 0; i < n; i++) {
				*(dst + sink_channel) = (int32_t)*(src + src_channel) << 8;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			frames_left -= n;
		}
	}

	return source_samples;
}

static int remap_and_convert_32_to_24(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	int src_channel, sink_channel;
	int num_src_channels = audio_stream_get_channels(source);
	int num_sink_channels = audio_stream_get_channels(sink);
	int frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		int32_t *src;
		int32_t *dst;
		int frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		if (src_channel == 0xf) {
			mute_channel_c32(sink, sink_channel, frames);
			continue;
		}

		assert(src_channel < num_src_channels);

		src = audio_stream_get_rptr(source);
		dst = audio_stream_get_wptr(sink);

		frames_left = frames;

		while (frames_left) {
			int i, n;

			assert(audio_stream_bytes_without_wrap(source, src) %
					audio_stream_frame_bytes(source) == 0);
			assert(audio_stream_bytes_without_wrap(sink, dst) %
					audio_stream_frame_bytes(sink) == 0);

			n = MIN(frames_left, audio_stream_frames_without_wrap(source, src));
			n = MIN(n, audio_stream_frames_without_wrap(sink, dst));

			for (i = 0; i < n; i++) {
				*(dst + sink_channel) = *(src + src_channel) >> 8;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			frames_left -= n;
		}
	}

	return source_samples;
}

static int remap_and_convert_24_to_32(const struct audio_stream *source, uint32_t dummy1,
				      struct audio_stream *sink, uint32_t dummy2,
				      uint32_t source_samples, uint32_t chmap)
{
	int src_channel, sink_channel;
	int num_src_channels = audio_stream_get_channels(source);
	int num_sink_channels = audio_stream_get_channels(sink);
	int frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		int32_t *src;
		int32_t *dst;
		int frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		if (src_channel == 0xf) {
			mute_channel_c32(sink, sink_channel, frames);
			continue;
		}

		assert(src_channel < num_src_channels);

		src = audio_stream_get_rptr(source);
		dst = audio_stream_get_wptr(sink);

		frames_left = frames;

		while (frames_left) {
			int i, n;

			assert(audio_stream_bytes_without_wrap(source, src) %
					audio_stream_frame_bytes(source) == 0);
			assert(audio_stream_bytes_without_wrap(sink, dst) %
					audio_stream_frame_bytes(sink) == 0);

			n = MIN(frames_left, audio_stream_frames_without_wrap(source, src));
			n = MIN(n, audio_stream_frames_without_wrap(sink, dst));

			for (i = 0; i < n; i++) {
				*(dst + sink_channel) = *(src + src_channel) << 8;
				src += num_src_channels;
				dst += num_sink_channels;
			}

			src = audio_stream_wrap(source, src);
			dst = audio_stream_wrap(sink, dst);

			frames_left -= n;
		}
	}

	return source_samples;
}

///!!! TODO: ADD ifdef for formats !!!
const struct pcm_func_map pcm_remap_func_map[] = {
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, remap_c16},
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, remap_c32},
	{ SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S24_4LE, remap_and_convert_32_to_24},
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE_MSB, remap_and_convert_24_to_32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, remap_c32},

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

