// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Bluetooth aptX per-codec packet alignment and frame sizing for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include "ffmpeg_dec.h"

#define APTX_DEC_MAX_SAMPLES	1024
#define APTX_BLOCK_ALIGN	4

static uint32_t ffmpeg_dec_aptx_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	(void)cd;
	return APTX_DEC_MAX_SAMPLES;
}

/*
 * Each 4-byte aptX block (two 16-bit codewords, L+R) decodes to 4 stereo PCM
 * samples. Clamp each packet to 1024 bytes and align down to a multiple of 4
 * bytes so aptx_decode_frame never sees a partial codeword.
 */
static size_t ffmpeg_dec_aptx_limit_packet_size(const struct ffmpeg_dec_comp_data *cd,
						size_t pkt_len)
{
	size_t clamped = pkt_len > APTX_DEC_MAX_SAMPLES ? APTX_DEC_MAX_SAMPLES : pkt_len;

	(void)cd;
	return clamped & ~(size_t)(APTX_BLOCK_ALIGN - 1);
}

const struct ffmpeg_dec_codec_ops ffmpeg_dec_aptx_ops = {
	.id = FFMPEG_DEC_CODEC_APTX,
	.snd_codec_id = SND_AUDIOCODEC_APTX,
	.name = "aptx",
	.decoder_name = "aptx",
	.max_frame_samples = ffmpeg_dec_aptx_max_frame_samples,
	.limit_packet_size = ffmpeg_dec_aptx_limit_packet_size,
};
