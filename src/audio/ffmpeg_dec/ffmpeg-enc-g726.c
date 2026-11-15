// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 *
 * ITU-T G.726 / G.726LE ADPCM per-codec encoder implementation for ffmpeg_dec.
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

#define G726_DEFAULT_CODE_SIZE	4
/* Multiple of 8 so 2/3/4/5-bit codewords align to whole byte boundaries */
#define G726_ENC_FRAME_SAMPLES	160

struct ffmpeg_enc_g726_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
	uint32_t enc_rate;
	int decimate;
};

static int ffmpeg_enc_g726_validate_params(struct processing_module *mod,
					   const struct snd_codec *codec_params)
{
	struct comp_dev *dev = mod->dev;

	if (codec_params->format != SND_AUDIOSTREAMFORMAT_UNDEFINED &&
	    codec_params->format != SND_AUDIOSTREAMFORMAT_RAW &&
	    codec_params->format != 2 &&
	    codec_params->format != 10) {
		comp_err(dev, "unsupported G.726 stream format 0x%x",
			 codec_params->format);
		return -ENOTSUP;
	}

	if (codec_params->profile > 1) {
		comp_err(dev, "unsupported G.726 profile %u",
			 codec_params->profile);
		return -ENOTSUP;
	}

	return 0;
}

static int ffmpeg_enc_g726_init(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_g726_data *g726;
	enum AVCodecID codec_id;

	g726 = mod_zalloc(mod, sizeof(*g726));
	if (!g726) {
		return -ENOMEM;
	}

	codec_id = (e->req_profile == 1) ?
		   AV_CODEC_ID_ADPCM_G726LE : AV_CODEC_ID_ADPCM_G726;
	g726->codec = avcodec_find_encoder(codec_id);
	if (!g726->codec) {
		comp_err(dev, "libavcodec G.726 encoder (profile %u) not found",
			 e->req_profile);
		mod_free(mod, g726);
		return -ENODEV;
	}

	g726->avctx = avcodec_alloc_context3(g726->codec);
	g726->pkt = av_packet_alloc();
	g726->frame = av_frame_alloc();
	if (!g726->avctx || !g726->pkt || !g726->frame) {
		if (g726->frame) {
			av_frame_free(&g726->frame);
		}
		if (g726->pkt) {
			av_packet_free(&g726->pkt);
		}
		if (g726->avctx) {
			avcodec_free_context(&g726->avctx);
		}
		mod_free(mod, g726);
		return -ENOMEM;
	}

	e->codec_priv = g726;
	return 0;
}

static int ffmpeg_enc_g726_configure(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_g726_data *g726 = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	uint32_t enc_rate = e->req_rate ? e->req_rate : cd->out_rate;
	uint32_t cs;
	int ret;

	if (enc_rate == cd->out_rate) {
		g726->decimate = 1;
	} else if (cd->out_rate == 48000 && enc_rate == 16000) {
		g726->decimate = 3;
	} else if (cd->out_rate == 48000 && enc_rate == 8000) {
		g726->decimate = 6;
	} else {
		comp_err(dev, "G.726 cannot convert %u Hz source to %u Hz",
			 cd->out_rate, enc_rate);
		return -EINVAL;
	}

	if (!e->req_bit_rate) {
		cs = G726_DEFAULT_CODE_SIZE;
	} else {
		cs = (e->req_bit_rate + enc_rate / 2) / enc_rate;
		if (cs < 2 || cs > 5 || cs * enc_rate != e->req_bit_rate) {
			comp_err(dev, "unsupported G.726 encoder bit_rate %u at %u Hz",
				 e->req_bit_rate, enc_rate);
			return -EINVAL;
		}
	}

	if (avcodec_is_open(g726->avctx)) {
		avcodec_free_context(&g726->avctx);
		g726->avctx = avcodec_alloc_context3(g726->codec);
		if (!g726->avctx) {
			return -ENOMEM;
		}
	}

	g726->enc_rate = enc_rate;
	g726->avctx->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
	g726->avctx->sample_rate = enc_rate;
	av_channel_layout_default(&g726->avctx->ch_layout, 1);
	g726->avctx->sample_fmt = AV_SAMPLE_FMT_S16;
	g726->avctx->bit_rate = (int64_t)cs * enc_rate;
	g726->avctx->thread_count = 1;

	ret = avcodec_open2(g726->avctx, g726->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 (G.726 encoder) failed %d", ret);
		return -EIO;
	}

	/* Override large default frame_size so streaming packets stay within SOF module heap */
	g726->avctx->frame_size = G726_ENC_FRAME_SAMPLES;
	e->nb = G726_ENC_FRAME_SAMPLES * g726->decimate;
	return 0;
}

static int ffmpeg_enc_g726_encode_frame(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_g726_data *g726 = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	g726->frame->nb_samples = G726_ENC_FRAME_SAMPLES;
	g726->frame->format = g726->avctx->sample_fmt;
	g726->frame->sample_rate = g726->enc_rate;
	av_channel_layout_copy(&g726->frame->ch_layout, &g726->avctx->ch_layout);
	if (av_frame_get_buffer(g726->frame, 0) < 0) {
		return -ENOMEM;
	}

	if (g726->decimate == 6) {
		ffmpeg_dec_pcm_decimate_6to1_s16_mono(e->pcm_in,
						      (int16_t *)g726->frame->data[0],
						      e->src_fmt, cd->out_channels,
						      G726_ENC_FRAME_SAMPLES,
						      e->decim_hist);
	} else if (g726->decimate == 3) {
		ffmpeg_dec_pcm_decimate_3to1_s16_mono(e->pcm_in,
						      (int16_t *)g726->frame->data[0],
						      e->src_fmt, cd->out_channels,
						      G726_ENC_FRAME_SAMPLES,
						      e->decim_hist);
	} else {
		ret = ffmpeg_dec_pcm_to_frame(e->pcm_in, g726->frame,
					      e->src_fmt, cd->out_channels,
					      G726_ENC_FRAME_SAMPLES);
		if (ret < 0) {
			av_frame_unref(g726->frame);
			return ret;
		}
	}

	ret = avcodec_send_frame(g726->avctx, g726->frame);
	av_frame_unref(g726->frame);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_frame (G.726) failed %d", ret);
		return -EIO;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(g726->avctx, g726->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;
		}
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_packet (G.726) failed %d", ret);
			return -EIO;
		}
		if (g726->pkt->size > 0) {
			ffmpeg_enc_stage_packet(mod, e, g726->pkt->data,
						(size_t)g726->pkt->size);
		}
		av_packet_unref(g726->pkt);
		ret = 0;
	}

	return 0;
}

static void ffmpeg_enc_g726_reset(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_g726_data *g726 = e->codec_priv;

	(void)mod;
	if (!g726) {
		return;
	}

	if (g726->frame) {
		av_frame_unref(g726->frame);
	}
	if (g726->pkt) {
		av_packet_unref(g726->pkt);
	}
}

static void ffmpeg_enc_g726_free(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_g726_data *g726 = e->codec_priv;

	if (!g726) {
		return;
	}

	if (g726->frame) {
		av_frame_free(&g726->frame);
	}
	if (g726->pkt) {
		av_packet_free(&g726->pkt);
	}
	if (g726->avctx) {
		avcodec_free_context(&g726->avctx);
	}
	mod_free(mod, g726);
	e->codec_priv = NULL;
}

const struct ffmpeg_enc_codec_ops ffmpeg_enc_g726_ops = {
	.id = FFMPEG_DEC_CODEC_G726,
	.snd_codec_id = SND_AUDIOCODEC_G726,
	.name = "g726",
	.supports_mono_downmix = true,
	.supports_decim_3to1 = true,
	.supports_decim_6to1 = true,
	.validate_params = ffmpeg_enc_g726_validate_params,
	.init = ffmpeg_enc_g726_init,
	.configure = ffmpeg_enc_g726_configure,
	.encode_frame = ffmpeg_enc_g726_encode_frame,
	.reset = ffmpeg_enc_g726_reset,
	.free = ffmpeg_enc_g726_free,
};
