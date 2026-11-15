// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 *
 * ITU-T G.711 (A-law and mu-law) per-codec configuration for ffmpeg_dec.
 */

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include <errno.h>
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#endif
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define G711_DEFAULT_RATE	8000
#define G711_DEC_MAX_SAMPLES	1024

static uint32_t ffmpeg_dec_g711_max_frame_samples(const struct ffmpeg_dec_comp_data *cd)
{
	(void)cd;
	return G711_DEC_MAX_SAMPLES;
}

static size_t ffmpeg_dec_g711_limit_packet_size(const struct ffmpeg_dec_comp_data *cd,
						size_t pkt_len)
{
	uint32_t ch = cd->req_channels ? cd->req_channels :
		      (cd->out_channels ? cd->out_channels : 1);
	size_t max_bytes = G711_DEC_MAX_SAMPLES * (size_t)ch;
	size_t clamped = pkt_len > max_bytes ? max_bytes : pkt_len;

	return clamped - (clamped % (size_t)ch);
}

#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
static int ffmpeg_dec_g711_configure_avctx(struct processing_module *mod, void *ctx)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	AVCodecContext *avctx = ctx;
	uint32_t ch = cd->req_channels ? cd->req_channels :
		      (cd->out_channels ? cd->out_channels : 1);
	uint32_t rate = cd->req_rate ? cd->req_rate :
			(cd->out_rate ? cd->out_rate : G711_DEFAULT_RATE);

	if (ch < 1 || ch > 2) {
		comp_err(mod->dev, "unsupported G.711 channels %u", ch);
		return -EINVAL;
	}

	avctx->sample_rate = rate;
	av_channel_layout_default(&avctx->ch_layout, ch);
	return 0;
}
#endif

#if defined(CONFIG_FFMPEG_DEC_ALAW) || defined(CONFIG_COMP_FFMPEG_DEC_STUB)
const struct ffmpeg_dec_codec_ops ffmpeg_dec_alaw_ops = {
	.id = FFMPEG_DEC_CODEC_ALAW,
	.snd_codec_id = SND_AUDIOCODEC_ALAW,
	.name = "alaw",
	.decoder_name = "pcm_alaw",
	.max_frame_samples = ffmpeg_dec_g711_max_frame_samples,
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	.configure_avctx = ffmpeg_dec_g711_configure_avctx,
#endif
	.limit_packet_size = ffmpeg_dec_g711_limit_packet_size,
};
#endif

#if defined(CONFIG_FFMPEG_DEC_MULAW) || defined(CONFIG_COMP_FFMPEG_DEC_STUB)
const struct ffmpeg_dec_codec_ops ffmpeg_dec_mulaw_ops = {
	.id = FFMPEG_DEC_CODEC_MULAW,
	.snd_codec_id = SND_AUDIOCODEC_MULAW,
	.name = "mulaw",
	.decoder_name = "pcm_mulaw",
	.max_frame_samples = ffmpeg_dec_g711_max_frame_samples,
#if !defined(CONFIG_COMP_FFMPEG_DEC_STUB)
	.configure_avctx = ffmpeg_dec_g711_configure_avctx,
#endif
	.limit_packet_size = ffmpeg_dec_g711_limit_packet_size,
};
#endif
