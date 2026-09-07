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
 * @frame: Decoded libavcodec AVFrame.
 * @out: Destination buffer.
 * @out_size: Capacity of destination buffer in bytes.
 * @sink_fmt: SOF IPC frame format (SOF_IPC_FRAME_*).
 * @channels: Output channel count.
 *
 * Dispatches to specialized HiFi/SIMD and unrolled scalar conversion routines
 * based on the source AVFrame format and the destination SOF sink format.
 *
 * Returns number of bytes written, or a negative errno on error.
 */
int ffmpeg_dec_convert_frame(const AVFrame *frame, uint8_t *out, size_t out_size,
			     int sink_fmt, int channels);

#endif /* __SOF_AUDIO_FFMPEG_DEC_CONVERT_H__ */
