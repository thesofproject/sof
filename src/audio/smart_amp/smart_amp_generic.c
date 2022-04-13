// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Maxim Integrated All rights reserved.
//
// Author: Ryan Lee <ryans.lee@maximintegrated.com>

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/smart_amp/smart_amp.h>

#ifdef CONFIG_GENERIC

static int32_t smart_amp_ff_generic(int32_t x)
{
	/* Add speaker protection feed forward process here */
	return x;
}

static void smart_amp_fb_generic(int32_t x)
{
	/* Add speaker protection feedback process here */
}

#if CONFIG_FORMAT_S16LE
static void smart_amp_s16_ff_default(const struct comp_dev *dev,
				     const struct audio_stream __sparse_cache *source,
				     const struct audio_stream __sparse_cache *sink,
				     const struct audio_stream __sparse_cache *feedback,
				     uint32_t frames)
{
	int16_t *x;
	int16_t *y;
	int32_t tmp;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s16(source, idx);
			y = audio_stream_read_frag_s16(sink, idx);
			tmp = smart_amp_ff_generic(*x << 16);
			*y = sat_int16(Q_SHIFT_RND(tmp, 31, 15));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void smart_amp_s24_ff_default(const struct comp_dev *dev,
				     const struct audio_stream __sparse_cache *source,
				     const struct audio_stream __sparse_cache *sink,
				     const struct audio_stream __sparse_cache *feedback,
				     uint32_t frames)
{
	int32_t *x;
	int32_t *y;
	int32_t tmp;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_read_frag_s32(sink, idx);
			tmp = smart_amp_ff_generic(*x << 8);
			*y = sat_int24(Q_SHIFT_RND(tmp, 31, 23));
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void smart_amp_s32_ff_default(const struct comp_dev *dev,
				     const struct audio_stream __sparse_cache *source,
				     const struct audio_stream __sparse_cache *sink,
				     const struct audio_stream __sparse_cache *feedback,
				     uint32_t frames)
{
	int32_t *x;
	int32_t *y;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(source, idx);
			y = audio_stream_read_frag_s32(sink, idx);
			*y = smart_amp_ff_generic(*x);
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S16LE
static void smart_amp_s16_fb_default(const struct comp_dev *dev,
				     const struct audio_stream __sparse_cache *source,
				     const struct audio_stream __sparse_cache *sink,
				     const struct audio_stream __sparse_cache *feedback,
				     uint32_t frames)
{
	int16_t *x;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s16(feedback, idx);
			smart_amp_fb_generic(*x << 16);
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
static void smart_amp_s24_fb_default(const struct comp_dev *dev,
				     const struct audio_stream __sparse_cache *source,
				     const struct audio_stream __sparse_cache *sink,
				     const struct audio_stream __sparse_cache *feedback,
				     uint32_t frames)
{
	int32_t *x;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(feedback, idx);
			smart_amp_fb_generic(*x << 8);
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
static void smart_amp_s32_fb_default(const struct comp_dev *dev,
				     const struct audio_stream __sparse_cache *source,
				     const struct audio_stream __sparse_cache *sink,
				     const struct audio_stream __sparse_cache *feedback,
				     uint32_t frames)
{
	int32_t *x;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for (ch = 0; ch < nch; ch++) {
		idx = ch;
		for (i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s32(feedback, idx);
			smart_amp_ff_generic(*x);
			idx += nch;
		}
	}
}
#endif /* CONFIG_FORMAT_S32LE */

const struct smart_amp_func_map smart_amp_function_map[] = {
/* { SOURCE_FORMAT , PROCESSING FUNCTION } */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, smart_amp_s16_ff_default },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, smart_amp_s24_ff_default },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, smart_amp_s32_ff_default },
#endif /* CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, smart_amp_s16_fb_default },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, smart_amp_s24_fb_default },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, smart_amp_s32_fb_default },
#endif /* CONFIG_FORMAT_S32LE */
};

const size_t smart_amp_func_count = ARRAY_SIZE(smart_amp_function_map);
#endif
