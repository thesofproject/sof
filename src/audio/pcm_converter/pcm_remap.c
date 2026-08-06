// SPDX-License-Identifier: BSD-3-Clause
/*
 * (c) 2024 Intel Corporation. All rights reserved.
 */

#include <sof/audio/pcm_converter.h>
#include <sof/audio/audio_stream.h>

static void mute_channel_c16(struct cir_buf_sink *sink, uint32_t num_channels, uint32_t channel,
			     size_t frames)
{
	int16_t *ptr = (int16_t *)sink->ptr + channel;

	while (frames) {
		size_t samples_wo_wrap, n, i;

		ptr = cir_buf_wrap(ptr, sink->buf_start, sink->buf_end);

		samples_wo_wrap = cir_buf_samples_without_wrap_s16(ptr, sink->buf_end);
		n = SOF_DIV_ROUND_UP(samples_wo_wrap, num_channels);
		n = MIN(n, frames);

		for (i = 0; i < n; i++) {
			*ptr = 0;
			ptr += num_channels;
		}

		frames -= n;
	}
}

static void mute_channel_c32(struct cir_buf_sink *sink, uint32_t num_channels, uint32_t channel,
			     size_t frames)
{
	int32_t *ptr = (int32_t *)sink->ptr + channel;

	while (frames) {
		size_t samples_wo_wrap, n, i;

		ptr = cir_buf_wrap(ptr, sink->buf_start, sink->buf_end);

		samples_wo_wrap = cir_buf_samples_without_wrap_s32(ptr, sink->buf_end);
		n = SOF_DIV_ROUND_UP(samples_wo_wrap, num_channels);
		n = MIN(n, frames);

		for (i = 0; i < n; i++) {
			*ptr = 0;
			ptr += num_channels;
		}

		frames -= n;
	}
}

static int remap_c16(const struct cir_buf_source *source, uint32_t src_channels,
		     struct cir_buf_sink *sink, uint32_t sink_channels,
		     size_t source_samples, uint32_t chmap)
{
	uint32_t src_channel, sink_channel;
	size_t frames = source_samples / src_channels;

	for (sink_channel = 0; sink_channel < sink_channels; sink_channel++) {
		const int16_t *src;
		int16_t *dst;
		size_t frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		/* 0xf means "mute"; also mute any out-of-range source channel so
		 * a crafted chmap nibble cannot index past the source frame.
		 */
		if (src_channel == 0xf || src_channel >= src_channels) {
			mute_channel_c16(sink, sink_channels, sink_channel, frames);
			continue;
		}

		src = (const int16_t *)source->ptr + src_channel;
		dst = (int16_t *)sink->ptr + sink_channel;

		frames_left = frames;

		while (frames_left) {
			size_t samples_wo_wrap, n, i;

			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);

			samples_wo_wrap = cir_buf_samples_without_wrap_s16(src, source->buf_end);
			n = SOF_DIV_ROUND_UP(samples_wo_wrap, src_channels);

			samples_wo_wrap = cir_buf_samples_without_wrap_s16(dst, sink->buf_end);
			n = MIN(n, SOF_DIV_ROUND_UP(samples_wo_wrap, sink_channels));

			n = MIN(n, frames_left);

			for (i = 0; i < n; i++) {
				*dst = *src;
				src += src_channels;
				dst += sink_channels;
			}

			frames_left -= n;
		}
	}

	return source_samples;
}

static inline int remap_c32_left_shift(const struct cir_buf_source *source,
				       uint32_t num_src_channels,
				       struct cir_buf_sink *sink,
				       uint32_t num_sink_channels,
				       size_t source_samples, uint32_t chmap,
				       int shift)
{
	uint32_t src_channel, sink_channel;
	size_t frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		const int32_t *src;
		int32_t *dst;
		size_t frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		/* 0xf means "mute"; also mute any out-of-range source channel so
		 * a crafted chmap nibble cannot index past the source frame.
		 */
		if (src_channel == 0xf || src_channel >= num_src_channels) {
			mute_channel_c32(sink, num_sink_channels, sink_channel, frames);
			continue;
		}

		src = (const int32_t *)source->ptr + src_channel;
		dst = (int32_t *)sink->ptr + sink_channel;

		frames_left = frames;

		while (frames_left) {
			size_t samples_wo_wrap, n, i;

			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);

			samples_wo_wrap = cir_buf_samples_without_wrap_s32(src, source->buf_end);
			n = SOF_DIV_ROUND_UP(samples_wo_wrap, num_src_channels);

			samples_wo_wrap = cir_buf_samples_without_wrap_s32(dst, sink->buf_end);
			n = MIN(n, SOF_DIV_ROUND_UP(samples_wo_wrap, num_sink_channels));

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

static inline int remap_c32_right_shift(const struct cir_buf_source *source,
					uint32_t num_src_channels,
					struct cir_buf_sink *sink,
					uint32_t num_sink_channels,
					size_t source_samples, uint32_t chmap,
					int shift)
{
	uint32_t src_channel, sink_channel;
	size_t frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		const int32_t *src;
		int32_t *dst;
		size_t frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		/* 0xf means "mute"; also mute any out-of-range source channel so
		 * a crafted chmap nibble cannot index past the source frame.
		 */
		if (src_channel == 0xf || src_channel >= num_src_channels) {
			mute_channel_c32(sink, num_sink_channels, sink_channel, frames);
			continue;
		}

		src = (const int32_t *)source->ptr + src_channel;
		dst = (int32_t *)sink->ptr + sink_channel;

		frames_left = frames;

		while (frames_left) {
			size_t samples_wo_wrap, n, i;

			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);

			samples_wo_wrap = cir_buf_samples_without_wrap_s32(src, source->buf_end);
			n = SOF_DIV_ROUND_UP(samples_wo_wrap, num_src_channels);

			samples_wo_wrap = cir_buf_samples_without_wrap_s32(dst, sink->buf_end);
			n = MIN(n, SOF_DIV_ROUND_UP(samples_wo_wrap, num_sink_channels));

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

static inline int remap_c16_to_c32(const struct cir_buf_source *source,
				   uint32_t num_src_channels,
				   struct cir_buf_sink *sink,
				   uint32_t num_sink_channels,
				   size_t source_samples, uint32_t chmap,
				   int shift)
{
	uint32_t src_channel, sink_channel;
	size_t frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		const int16_t *src;
		int32_t *dst;
		size_t frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		/* 0xf means "mute"; also mute any out-of-range source channel so
		 * a crafted chmap nibble cannot index past the source frame.
		 */
		if (src_channel == 0xf || src_channel >= num_src_channels) {
			mute_channel_c32(sink, num_sink_channels, sink_channel, frames);
			continue;
		}

		src = (const int16_t *)source->ptr + src_channel;
		dst = (int32_t *)sink->ptr + sink_channel;

		frames_left = frames;

		while (frames_left) {
			size_t samples_wo_wrap, n, i;

			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);

			samples_wo_wrap = cir_buf_samples_without_wrap_s16(src, source->buf_end);
			n = SOF_DIV_ROUND_UP(samples_wo_wrap, num_src_channels);

			samples_wo_wrap = cir_buf_samples_without_wrap_s32(dst, sink->buf_end);
			n = MIN(n, SOF_DIV_ROUND_UP(samples_wo_wrap, num_sink_channels));

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

static inline int remap_c32_to_c16(const struct cir_buf_source *source,
				   uint32_t num_src_channels,
				   struct cir_buf_sink *sink,
				   uint32_t num_sink_channels,
				   size_t source_samples, uint32_t chmap,
				   int shift)
{
	uint32_t src_channel, sink_channel;
	size_t frames = source_samples / num_src_channels;

	for (sink_channel = 0; sink_channel < num_sink_channels; sink_channel++) {
		const int32_t *src;
		int16_t *dst;
		size_t frames_left;

		src_channel = chmap & 0xf;
		chmap >>= 4;

		/* 0xf means "mute"; also mute any out-of-range source channel so
		 * a crafted chmap nibble cannot index past the source frame.
		 */
		if (src_channel == 0xf || src_channel >= num_src_channels) {
			mute_channel_c16(sink, num_sink_channels, sink_channel, frames);
			continue;
		}

		src = (const int32_t *)source->ptr + src_channel;
		dst = (int16_t *)sink->ptr + sink_channel;

		frames_left = frames;

		while (frames_left) {
			size_t samples_wo_wrap, n, i;

			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);

			samples_wo_wrap = cir_buf_samples_without_wrap_s32(src, source->buf_end);
			n = SOF_DIV_ROUND_UP(samples_wo_wrap, num_src_channels);

			samples_wo_wrap = cir_buf_samples_without_wrap_s16(dst, sink->buf_end);
			n = MIN(n, SOF_DIV_ROUND_UP(samples_wo_wrap, num_sink_channels));

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

static int remap_c32(const struct cir_buf_source *source, uint32_t src_channels,
		     struct cir_buf_sink *sink, uint32_t sink_channels,
		     size_t source_samples, uint32_t chmap)
{
	return remap_c32_left_shift(source, src_channels, sink, sink_channels,
				    source_samples, chmap, 0);
}

static int remap_c32_to_c16_right_shift_16(const struct cir_buf_source *source,
					   uint32_t src_channels, struct cir_buf_sink *sink,
					   uint32_t sink_channels,
					   size_t source_samples, uint32_t chmap)
{
	return remap_c32_to_c16(source, src_channels, sink, sink_channels,
				source_samples, chmap, 16);
}

static int remap_c16_to_c32_left_shift_16(const struct cir_buf_source *source,
					  uint32_t src_channels, struct cir_buf_sink *sink,
					  uint32_t sink_channels,
					  size_t source_samples, uint32_t chmap)
{
	return remap_c16_to_c32(source, src_channels, sink, sink_channels,
				source_samples, chmap, 16);
}

static int remap_c32_to_c16_right_shift_8(const struct cir_buf_source *source,
					  uint32_t src_channels, struct cir_buf_sink *sink,
					  uint32_t sink_channels,
					  size_t source_samples, uint32_t chmap)
{
	return remap_c32_to_c16(source, src_channels, sink, sink_channels,
				source_samples, chmap, 8);
}

static int remap_c16_to_c32_left_shift_8(const struct cir_buf_source *source,
					 uint32_t src_channels, struct cir_buf_sink *sink,
					 uint32_t sink_channels,
					 size_t source_samples, uint32_t chmap)
{
	return remap_c16_to_c32(source, src_channels, sink, sink_channels,
				source_samples, chmap, 8);
}

static int remap_c32_right_shift_8(const struct cir_buf_source *source, uint32_t src_channels,
				   struct cir_buf_sink *sink, uint32_t sink_channels,
				   size_t source_samples, uint32_t chmap)
{
	return remap_c32_right_shift(source, src_channels, sink, sink_channels,
				     source_samples, chmap, 8);
}

static int remap_c32_left_shift_8(const struct cir_buf_source *source, uint32_t src_channels,
				  struct cir_buf_sink *sink, uint32_t sink_channels,
				  size_t source_samples, uint32_t chmap)
{
	return remap_c32_left_shift(source, src_channels, sink, sink_channels,
				    source_samples, chmap, 8);
}

static int remap_c32_right_shift_16(const struct cir_buf_source *source, uint32_t src_channels,
				    struct cir_buf_sink *sink, uint32_t sink_channels,
				    size_t source_samples, uint32_t chmap)
{
	return remap_c32_right_shift(source, src_channels, sink, sink_channels,
				     source_samples, chmap, 16);
}

static int remap_c32_left_shift_16(const struct cir_buf_source *source, uint32_t src_channels,
				   struct cir_buf_sink *sink, uint32_t sink_channels,
				   size_t source_samples, uint32_t chmap)
{
	return remap_c32_left_shift(source, src_channels, sink, sink_channels,
				    source_samples, chmap, 16);
}

static int remap_c32_to_c16_no_shift(const struct cir_buf_source *source, uint32_t src_channels,
				     struct cir_buf_sink *sink, uint32_t sink_channels,
				     size_t source_samples, uint32_t chmap)
{
	return remap_c32_to_c16(source, src_channels, sink, sink_channels,
				source_samples, chmap, 0);
}

static int remap_c16_to_c32_no_shift(const struct cir_buf_source *source, uint32_t src_channels,
				     struct cir_buf_sink *sink, uint32_t sink_channels,
				     size_t source_samples, uint32_t chmap)
{
	return remap_c16_to_c32(source, src_channels, sink, sink_channels,
				source_samples, chmap, 0);
}

/* Unfortunately, all these nice "if"s were commented out to suppress
 * CI "defined but not used" warnings.
 */
const struct pcm_func_map pcm_remap_func_map[] = {
/* #if CONFIG_PCM_CONVERTER_FORMAT_S16LE */
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, remap_c16},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S24_4LE */
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, remap_c16_to_c32_left_shift_8},
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, remap_c32_to_c16_right_shift_8},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S24_4LE_MSB */
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE_MSB, remap_c16_to_c32_left_shift_16},
	{ SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S16_LE, remap_c32_to_c16_right_shift_16},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE */
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, remap_c16_to_c32_left_shift_16},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, remap_c32_to_c16_right_shift_16},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S16LE && CONFIG_PCM_CONVERTER_FORMAT_S16_4LE */
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_4LE, remap_c16_to_c32_no_shift},
	{ SOF_IPC_FRAME_S16_4LE, SOF_IPC_FRAME_S16_LE, remap_c32_to_c16_no_shift},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S24_4LE */
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, remap_c32},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S24_4LE && CONFIG_PCM_CONVERTER_FORMAT_S24_4LE_MSB */
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE_MSB, remap_c32_left_shift_8},
	{ SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S24_4LE, remap_c32_right_shift_8},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S24_4LE && CONFIG_PCM_CONVERTER_FORMAT_S32LE */
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, remap_c32_left_shift_8},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, remap_c32_right_shift_8},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S24_4LE && CONFIG_PCM_CONVERTER_FORMAT_S16_4LE */
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_4LE, remap_c32_right_shift_8},
	{ SOF_IPC_FRAME_S16_4LE, SOF_IPC_FRAME_S24_4LE, remap_c32_left_shift_8},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S24_4LE_MSB && CONFIG_PCM_CONVERTER_FORMAT_S32LE */
	{ SOF_IPC_FRAME_S24_4LE_MSB, SOF_IPC_FRAME_S32_LE, remap_c32},
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE_MSB, remap_c32},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S32LE */
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, remap_c32},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S32LE && CONFIG_PCM_CONVERTER_FORMAT_S16_4LE */
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_4LE, remap_c32_right_shift_16},
	{ SOF_IPC_FRAME_S16_4LE, SOF_IPC_FRAME_S32_LE, remap_c32_left_shift_16},
/* #endif */
/* #if CONFIG_PCM_CONVERTER_FORMAT_S16_4LE */
	{ SOF_IPC_FRAME_S16_4LE, SOF_IPC_FRAME_S16_4LE, remap_c32},
/* #endif */
};

const size_t pcm_remap_func_count = ARRAY_SIZE(pcm_remap_func_map);
