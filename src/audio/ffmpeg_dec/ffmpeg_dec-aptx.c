// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
/* Bluetooth aptX / aptX HD per-codec packet alignment and frame sizing for ffmpeg_dec. */

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#endif
#include "ffmpeg_dec.h"

#define APTX_DEC_MAX_SAMPLES	1024
#define APTX_BLOCK_ALIGN	4
#define APTX_HD_BLOCK_ALIGN	6
#define APTX_DEFAULT_CHANNELS	2
#define APTX_DEFAULT_RATE	48000

static uint32_t ffmpeg_dec_aptx_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	(void)cd;
	return APTX_DEC_MAX_SAMPLES;
}

/*
 * Each aptX block (4 bytes) or aptX HD block (6 bytes) decodes to 4 stereo PCM
 * samples. Clamp each packet to 1024 bytes and align down to a whole block
 * (1024 bytes for aptX, 1020 bytes for aptX HD) so the decoder never sees a
 * partial codeword.
 */
static size_t ffmpeg_dec_aptx_limit_packet_size(const struct ffmpeg_dec_comp_data *cd,
						size_t pkt_len)
{
	size_t align = (cd->codec == FFMPEG_DEC_CODEC_APTX_HD) ?
		       APTX_HD_BLOCK_ALIGN : APTX_BLOCK_ALIGN;
	size_t clamped = pkt_len > APTX_DEC_MAX_SAMPLES ? APTX_DEC_MAX_SAMPLES : pkt_len;

	return clamped - (clamped % align);
}

#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
static int ffmpeg_dec_aptx_configure_avctx(struct processing_module *mod, void *ctx)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	AVCodecContext *avctx = ctx;

	if (avctx->ch_layout.nb_channels <= 0)
		av_channel_layout_default(&avctx->ch_layout, APTX_DEFAULT_CHANNELS);
	if (avctx->sample_rate <= 0)
		avctx->sample_rate = cd->out_rate ? cd->out_rate : APTX_DEFAULT_RATE;

	return 0;
}
#endif

#if defined(CONFIG_FFMPEG_DEC_APTX) || defined(CONFIG_COMP_FFMPEG_DEC_STUB)
const struct ffmpeg_dec_codec_ops ffmpeg_dec_aptx_ops = {
	.id = FFMPEG_DEC_CODEC_APTX,
	.snd_codec_id = SND_AUDIOCODEC_APTX,
	.name = "aptx",
	.decoder_name = "aptx",
	.max_frame_samples = ffmpeg_dec_aptx_max_frame_samples,
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	.configure_avctx = ffmpeg_dec_aptx_configure_avctx,
#endif
	.limit_packet_size = ffmpeg_dec_aptx_limit_packet_size,
};
#endif

#if defined(CONFIG_FFMPEG_DEC_APTX_HD) || defined(CONFIG_COMP_FFMPEG_DEC_STUB)
const struct ffmpeg_dec_codec_ops ffmpeg_dec_aptx_hd_ops = {
	.id = FFMPEG_DEC_CODEC_APTX_HD,
	.snd_codec_id = SND_AUDIOCODEC_APTX_HD,
	.name = "aptx_hd",
	.decoder_name = "aptx_hd",
	.max_frame_samples = ffmpeg_dec_aptx_max_frame_samples,
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	.configure_avctx = ffmpeg_dec_aptx_configure_avctx,
#endif
	.limit_packet_size = ffmpeg_dec_aptx_limit_packet_size,
};
#endif
