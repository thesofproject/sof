/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * Fast-path PCM format conversion and interleaving for ffmpeg_dec.
 */
#ifndef __SOF_AUDIO_FFMPEG_DEC_CONVERT_H__
#define __SOF_AUDIO_FFMPEG_DEC_CONVERT_H__

#include <stddef.h>
#include <stdint.h>
#include <libavutil/frame.h>

/**
 * ffmpeg_dec_convert_frame - Convert and interleave an AVFrame into a SOF sink buffer.
 * @frame: Decoded or filtered libav* AVFrame.
 * @out: Destination buffer.
 * @out_size: Capacity of destination buffer in bytes.
 * @sink_fmt: SOF valid sample format (SOF_IPC_FRAME_*).
 * @channels: Output channel count.
 *
 * Dispatches to specialized HiFi/SIMD and unrolled scalar conversion routines
 * based on the source AVFrame format and the destination SOF sink format.
 *
 * Returns number of bytes written, -ENOSPC if @out is too small, or -EINVAL for
 * an empty frame or one whose channel count does not match @channels.
 */
int ffmpeg_dec_convert_frame(const AVFrame *frame, uint8_t *out, size_t out_size,
			     int sink_fmt, int channels);

/**
 * ffmpeg_dec_pcm_to_frame - Convert interleaved SOF PCM into an AVFrame.
 * @in: Source interleaved PCM buffer.
 * @frame: Destination pre-allocated AVFrame (AV_SAMPLE_FMT_FLTP or AV_SAMPLE_FMT_DBL).
 * @src_fmt: SOF valid sample format (SOF_IPC_FRAME_S16_LE / S24_4LE / S32_LE).
 * @channels: Input channel count.
 * @nb_samples: Number of audio frames (samples per channel) to convert.
 *
 * Returns 0 on success or negative errno on invalid parameters.
 */
int ffmpeg_dec_pcm_to_frame(const uint8_t *in, AVFrame *frame,
			    int src_fmt, int channels, int nb_samples);

/* HiFi SIMD helpers implemented in ffmpeg-convert-hifi.c */
int ffmpeg_hifi_fltp_to_s32(const float *const *src, int32_t *dst,
			    int channels, int nb_samples);
int ffmpeg_hifi_s32_to_fltp(const int32_t *src, float *const *dst,
			    int channels, int nb_samples);
int ffmpeg_hifi_s32p_to_s32_stereo(const int32_t *c0, const int32_t *c1,
				   int32_t *dst, int nb_samples);

#endif /* __SOF_AUDIO_FFMPEG_DEC_CONVERT_H__ */
