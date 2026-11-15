// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 *
 * ITU-T G.711 (A-law and mu-law) encoder implementation for ffmpeg_dec.
 */

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include <rtos/string.h>
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include "ffmpeg_dec.h"
#include "ffmpeg_dec-convert.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define G711_ENC_FRAME_SAMPLES	160

struct ffmpeg_enc_g711_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
	uint32_t enc_rate;
	uint32_t enc_ch;
	int decimate;
};

static int ffmpeg_enc_g711_validate_params(struct processing_module *mod,
					   const struct snd_codec *codec_params)
{
	struct comp_dev *dev = mod->dev;

	if (codec_params->format != SND_AUDIOSTREAMFORMAT_UNDEFINED &&
	    codec_params->format != SND_AUDIOSTREAMFORMAT_RAW &&
	    codec_params->format != 2 &&
	    codec_params->format != 10) {
		comp_err(dev, "unsupported G.711 stream format 0x%x",
			 codec_params->format);
		return -ENOTSUP;
	}

	return 0;
}

static int ffmpeg_enc_g711_init(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_g711_data *g711;
	enum AVCodecID codec_id;

	g711 = mod_zalloc(mod, sizeof(*g711));
	if (!g711) {
		return -ENOMEM;
	}

	codec_id = (e->codec_ops->id == FFMPEG_DEC_CODEC_ALAW) ?
		   AV_CODEC_ID_PCM_ALAW : AV_CODEC_ID_PCM_MULAW;
	g711->codec = avcodec_find_encoder(codec_id);
	if (!g711->codec) {
		comp_err(dev, "libavcodec %s encoder not found", e->codec_ops->name);
		mod_free(mod, g711);
		return -ENODEV;
	}

	g711->avctx = avcodec_alloc_context3(g711->codec);
	g711->pkt = av_packet_alloc();
	g711->frame = av_frame_alloc();
	if (!g711->avctx || !g711->pkt || !g711->frame) {
		if (g711->frame) {
			av_frame_free(&g711->frame);
		}
		if (g711->pkt) {
			av_packet_free(&g711->pkt);
		}
		if (g711->avctx) {
			avcodec_free_context(&g711->avctx);
		}
		mod_free(mod, g711);
		return -ENOMEM;
	}

	e->codec_priv = g711;
	return 0;
}

static int ffmpeg_enc_g711_configure(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_g711_data *g711 = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	uint32_t enc_rate = e->req_rate ? e->req_rate : cd->out_rate;
	uint32_t enc_ch = e->req_channels ? e->req_channels : cd->out_channels;
	int ret;

	if (enc_ch < 1 || enc_ch > 2 ||
	    (enc_ch != 1 && enc_ch != cd->out_channels)) {
		comp_err(dev, "G.711 cannot convert %u ch source to %u ch",
			 cd->out_channels, enc_ch);
		return -EINVAL;
	}

	if (enc_rate == cd->out_rate) {
		g711->decimate = 1;
	} else if (cd->out_rate == 48000 && enc_rate == 16000) {
		g711->decimate = 3;
	} else if (cd->out_rate == 48000 && enc_rate == 8000) {
		g711->decimate = 6;
	} else {
		comp_err(dev, "G.711 cannot convert %u Hz source to %u Hz",
			 cd->out_rate, enc_rate);
		return -EINVAL;
	}

	if (g711->decimate > 1 && enc_ch != 1) {
		comp_err(dev, "G.711 decimation %d:1 only supports mono (got %u ch)",
			 g711->decimate, enc_ch);
		return -EINVAL;
	}

	if (avcodec_is_open(g711->avctx)) {
		avcodec_free_context(&g711->avctx);
		g711->avctx = avcodec_alloc_context3(g711->codec);
		if (!g711->avctx) {
			return -ENOMEM;
		}
	}

	g711->enc_rate = enc_rate;
	g711->enc_ch = enc_ch;
	g711->avctx->sample_rate = enc_rate;
	av_channel_layout_default(&g711->avctx->ch_layout, enc_ch);
	g711->avctx->sample_fmt = AV_SAMPLE_FMT_S16;
	g711->avctx->thread_count = 1;

	ret = avcodec_open2(g711->avctx, g711->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 (%s encoder) failed %d",
			 e->codec_ops->name, ret);
		return -EIO;
	}

	e->nb = G711_ENC_FRAME_SAMPLES * g711->decimate;
	return 0;
}

static int ffmpeg_enc_g711_encode_frame(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_g711_data *g711 = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	g711->frame->nb_samples = G711_ENC_FRAME_SAMPLES;
	g711->frame->format = g711->avctx->sample_fmt;
	g711->frame->sample_rate = g711->enc_rate;
	av_channel_layout_copy(&g711->frame->ch_layout, &g711->avctx->ch_layout);
	if (av_frame_get_buffer(g711->frame, 0) < 0) {
		return -ENOMEM;
	}

	if (g711->decimate == 6) {
		ffmpeg_dec_pcm_decimate_6to1_s16_mono(e->pcm_in,
						      (int16_t *)g711->frame->data[0],
						      e->src_fmt, cd->out_channels,
						      G711_ENC_FRAME_SAMPLES,
						      e->decim_hist);
	} else if (g711->decimate == 3) {
		ffmpeg_dec_pcm_decimate_3to1_s16_mono(e->pcm_in,
						      (int16_t *)g711->frame->data[0],
						      e->src_fmt, cd->out_channels,
						      G711_ENC_FRAME_SAMPLES,
						      e->decim_hist);
	} else {
		ret = ffmpeg_dec_pcm_to_frame(e->pcm_in, g711->frame,
					      e->src_fmt, cd->out_channels,
					      G711_ENC_FRAME_SAMPLES);
		if (ret < 0) {
			av_frame_unref(g711->frame);
			return ret;
		}
	}

	ret = avcodec_send_frame(g711->avctx, g711->frame);
	av_frame_unref(g711->frame);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_frame (%s) failed %d",
			 e->codec_ops->name, ret);
		return -EIO;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(g711->avctx, g711->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;
		}
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_packet (%s) failed %d",
				 e->codec_ops->name, ret);
			return -EIO;
		}
		if (g711->pkt->size > 0) {
			ffmpeg_enc_stage_packet(mod, e, g711->pkt->data,
						(size_t)g711->pkt->size);
		}
		av_packet_unref(g711->pkt);
		ret = 0;
	}

	return 0;
}

static void ffmpeg_enc_g711_reset(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_g711_data *g711 = e->codec_priv;

	(void)mod;
	if (!g711) {
		return;
	}

	if (g711->frame) {
		av_frame_unref(g711->frame);
	}
	if (g711->pkt) {
		av_packet_unref(g711->pkt);
	}
}

static void ffmpeg_enc_g711_free(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_g711_data *g711 = e->codec_priv;

	if (!g711) {
		return;
	}

	if (g711->frame) {
		av_frame_free(&g711->frame);
	}
	if (g711->pkt) {
		av_packet_free(&g711->pkt);
	}
	if (g711->avctx) {
		avcodec_free_context(&g711->avctx);
	}
	mod_free(mod, g711);
	e->codec_priv = NULL;
}

#if defined(CONFIG_FFMPEG_ENC_ALAW)
const struct ffmpeg_enc_codec_ops ffmpeg_enc_alaw_ops = {
	.id = FFMPEG_DEC_CODEC_ALAW,
	.snd_codec_id = SND_AUDIOCODEC_ALAW,
	.name = "alaw",
	.supports_mono_downmix = true,
	.supports_decim_3to1 = true,
	.supports_decim_6to1 = true,
	.validate_params = ffmpeg_enc_g711_validate_params,
	.init = ffmpeg_enc_g711_init,
	.configure = ffmpeg_enc_g711_configure,
	.encode_frame = ffmpeg_enc_g711_encode_frame,
	.reset = ffmpeg_enc_g711_reset,
	.free = ffmpeg_enc_g711_free,
};
#endif

#if defined(CONFIG_FFMPEG_ENC_MULAW)
const struct ffmpeg_enc_codec_ops ffmpeg_enc_mulaw_ops = {
	.id = FFMPEG_DEC_CODEC_MULAW,
	.snd_codec_id = SND_AUDIOCODEC_MULAW,
	.name = "mulaw",
	.supports_mono_downmix = true,
	.supports_decim_3to1 = true,
	.supports_decim_6to1 = true,
	.validate_params = ffmpeg_enc_g711_validate_params,
	.init = ffmpeg_enc_g711_init,
	.configure = ffmpeg_enc_g711_configure,
	.encode_frame = ffmpeg_enc_g711_encode_frame,
	.reset = ffmpeg_enc_g711_reset,
	.free = ffmpeg_enc_g711_free,
};
#endif
