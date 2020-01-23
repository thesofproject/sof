// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>

/**
 * \file audio/pcm_converter/pcm_converter_generic.c
 * \brief PCM converter generic processing implementation
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#include <sof/audio/pcm_converter.h>

#ifdef PCM_CONVERTER_GENERIC

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE

static void pcm_convert_s16_to_s24(const struct audio_stream *source,
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
}

static void pcm_convert_s24_to_s16(const struct audio_stream *source,
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
}

#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE

static void pcm_convert_s16_to_s32(const struct audio_stream *source,
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
}

static void pcm_convert_s32_to_s16(const struct audio_stream *source,
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
}

#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE

static void pcm_convert_s24_to_s32(const struct audio_stream *source,
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
}

static void pcm_convert_s32_to_s24(const struct audio_stream *source,
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
}

#endif /* CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE */

const struct pcm_func_map pcm_func_map[] = {
#if CONFIG_FORMAT_S16LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, audio_stream_copy_s16 },
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, audio_stream_copy_s32 },
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, audio_stream_copy_s32 },
#endif /* CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, pcm_convert_s16_to_s24 },
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, pcm_convert_s24_to_s16 },
#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, pcm_convert_s16_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, pcm_convert_s32_to_s16 },
#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
	{ SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, pcm_convert_s24_to_s32 },
	{ SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, pcm_convert_s32_to_s24 },
#endif /* CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE */
};

const size_t pcm_func_count = ARRAY_SIZE(pcm_func_map);

#endif
