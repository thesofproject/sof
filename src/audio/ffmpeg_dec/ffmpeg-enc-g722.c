// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// ITU-T G.722 ADPCM wideband speech per-codec encoder implementation for ffmpeg_dec.

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

#define G722_SAMPLE_RATE	16000
#define G722_ENC_FRAME_SAMPLES	1024

struct ffmpeg_enc_g722_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
	int decimate;
};

static int ffmpeg_enc_g722_validate_params(struct processing_module *mod,
					   const struct snd_codec *codec_params)
{
	struct comp_dev *dev = mod->dev;

	if (codec_params->sample_rate != 0 &&
	    codec_params->sample_rate != G722_SAMPLE_RATE &&
	    codec_params->sample_rate != 48000) {
		comp_err(dev, "unsupported G.722 sample rate %u",
			 codec_params->sample_rate);
		return -ENOTSUP;
	}

	if (codec_params->bit_rate != 0 &&
	    codec_params->bit_rate != 64000) {
		comp_err(dev, "unsupported G.722 encoder bit_rate %u",
			 codec_params->bit_rate);
		return -ENOTSUP;
	}

	if (codec_params->format != SND_AUDIOSTREAMFORMAT_UNDEFINED &&
	    codec_params->format != SND_AUDIOSTREAMFORMAT_RAW &&
	    codec_params->format != 2 &&
	    codec_params->format != 10) {
		comp_err(dev, "unsupported G.722 stream format 0x%x",
			 codec_params->format);
		return -ENOTSUP;
	}

	return 0;
}

static int ffmpeg_enc_g722_init(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_g722_data *g722;

	g722 = mod_zalloc(mod, sizeof(*g722));
	if (!g722)
		return -ENOMEM;

	g722->codec = avcodec_find_encoder(AV_CODEC_ID_ADPCM_G722);
	if (!g722->codec) {
		comp_err(dev, "libavcodec G.722 encoder not found");
		mod_free(mod, g722);
		return -ENODEV;
	}

	g722->avctx = avcodec_alloc_context3(g722->codec);
	g722->pkt = av_packet_alloc();
	g722->frame = av_frame_alloc();
	if (!g722->avctx || !g722->pkt || !g722->frame) {
		if (g722->frame)
			av_frame_free(&g722->frame);
		if (g722->pkt)
			av_packet_free(&g722->pkt);
		if (g722->avctx)
			avcodec_free_context(&g722->avctx);
		mod_free(mod, g722);
		return -ENOMEM;
	}

	e->codec_priv = g722;
	return 0;
}

static int ffmpeg_enc_g722_configure(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_g722_data *g722 = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	if (e->req_bit_rate != 0 && e->req_bit_rate != 64000) {
		comp_err(dev, "unsupported G.722 encoder bit_rate %u",
			 e->req_bit_rate);
		return -EINVAL;
	}

	if (cd->out_rate == 48000) {
		g722->decimate = 3;
	} else if (cd->out_rate == G722_SAMPLE_RATE) {
		g722->decimate = 1;
	} else {
		comp_err(dev, "G.722 requires 16 kHz or 48 kHz source rate (got %u)",
			 cd->out_rate);
		return -EINVAL;
	}

	if (avcodec_is_open(g722->avctx)) {
		avcodec_free_context(&g722->avctx);
		g722->avctx = avcodec_alloc_context3(g722->codec);
		if (!g722->avctx)
			return -ENOMEM;
	}

	g722->avctx->sample_rate = G722_SAMPLE_RATE;
	av_channel_layout_default(&g722->avctx->ch_layout, 1);
	g722->avctx->sample_fmt = AV_SAMPLE_FMT_S16;
	g722->avctx->frame_size = G722_ENC_FRAME_SAMPLES;
	g722->avctx->thread_count = 1;

	ret = avcodec_open2(g722->avctx, g722->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 (G.722 encoder) failed %d", ret);
		return -EIO;
	}

	e->nb = G722_ENC_FRAME_SAMPLES * g722->decimate;
	return 0;
}

static int ffmpeg_enc_g722_encode_frame(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_g722_data *g722 = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	g722->frame->nb_samples = G722_ENC_FRAME_SAMPLES;
	g722->frame->format = g722->avctx->sample_fmt;
	g722->frame->sample_rate = G722_SAMPLE_RATE;
	av_channel_layout_copy(&g722->frame->ch_layout, &g722->avctx->ch_layout);
	if (av_frame_get_buffer(g722->frame, 0) < 0)
		return -ENOMEM;

	if (g722->decimate == 3) {
		ffmpeg_dec_pcm_decimate_3to1_s16_mono(e->pcm_in,
						      (int16_t *)g722->frame->data[0],
						      e->src_fmt, cd->out_channels,
						      G722_ENC_FRAME_SAMPLES,
						      e->decim_hist);
	} else {
		ffmpeg_dec_pcm_to_frame(e->pcm_in, g722->frame,
					e->src_fmt, cd->out_channels,
					G722_ENC_FRAME_SAMPLES);
	}

	ret = avcodec_send_frame(g722->avctx, g722->frame);
	av_frame_unref(g722->frame);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_frame (G.722) failed %d", ret);
		return -EIO;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(g722->avctx, g722->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_packet (G.722) failed %d", ret);
			return -EIO;
		}
		if (g722->pkt->size > 0)
			ffmpeg_enc_stage_packet(mod, e, g722->pkt->data,
						(size_t)g722->pkt->size);
		av_packet_unref(g722->pkt);
		ret = 0;
	}

	return 0;
}

static void ffmpeg_enc_g722_reset(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_g722_data *g722 = e->codec_priv;

	(void)mod;
	if (!g722)
		return;

	if (g722->frame)
		av_frame_unref(g722->frame);
	if (g722->pkt)
		av_packet_unref(g722->pkt);
}

static void ffmpeg_enc_g722_free(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_g722_data *g722 = e->codec_priv;

	if (!g722)
		return;

	if (g722->frame)
		av_frame_free(&g722->frame);
	if (g722->pkt)
		av_packet_free(&g722->pkt);
	if (g722->avctx)
		avcodec_free_context(&g722->avctx);
	mod_free(mod, g722);
	e->codec_priv = NULL;
}

const struct ffmpeg_enc_codec_ops ffmpeg_enc_g722_ops = {
	.id = FFMPEG_DEC_CODEC_G722,
	.snd_codec_id = SND_AUDIOCODEC_G722,
	.name = "g722",
	.validate_params = ffmpeg_enc_g722_validate_params,
	.init = ffmpeg_enc_g722_init,
	.configure = ffmpeg_enc_g722_configure,
	.encode_frame = ffmpeg_enc_g722_encode_frame,
	.reset = ffmpeg_enc_g722_reset,
	.free = ffmpeg_enc_g722_free,
};
