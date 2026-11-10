// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Bluetooth aptX per-codec encoder implementation for ffmpeg_dec.

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

#define APTX_ENC_FRAME_SAMPLES	1024

struct ffmpeg_enc_aptx_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
};

static int ffmpeg_enc_aptx_validate_params(struct processing_module *mod,
					   const struct snd_codec *codec_params)
{
	struct comp_dev *dev = mod->dev;

	if (codec_params->format != SND_AUDIOSTREAMFORMAT_UNDEFINED &&
	    codec_params->format != SND_AUDIOSTREAMFORMAT_RAW &&
	    codec_params->format != 2 &&
	    codec_params->format != 10) {
		comp_err(dev, "unsupported aptX stream format 0x%x",
			 codec_params->format);
		return -ENOTSUP;
	}

	return 0;
}

static int ffmpeg_enc_aptx_init(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_aptx_data *aptx;

	aptx = mod_zalloc(mod, sizeof(*aptx));
	if (!aptx)
		return -ENOMEM;

	aptx->codec = avcodec_find_encoder(AV_CODEC_ID_APTX);
	if (!aptx->codec) {
		comp_err(dev, "libavcodec aptX encoder not found");
		mod_free(mod, aptx);
		return -ENODEV;
	}

	aptx->avctx = avcodec_alloc_context3(aptx->codec);
	aptx->pkt = av_packet_alloc();
	aptx->frame = av_frame_alloc();
	if (!aptx->avctx || !aptx->pkt || !aptx->frame) {
		if (aptx->frame)
			av_frame_free(&aptx->frame);
		if (aptx->pkt)
			av_packet_free(&aptx->pkt);
		if (aptx->avctx)
			avcodec_free_context(&aptx->avctx);
		mod_free(mod, aptx);
		return -ENOMEM;
	}

	e->codec_priv = aptx;
	return 0;
}

static int ffmpeg_enc_aptx_configure(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_aptx_data *aptx = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	if (cd->out_channels != 2) {
		comp_err(dev, "aptX requires 2 (stereo) channels, got %u", cd->out_channels);
		return -EINVAL;
	}

	if (avcodec_is_open(aptx->avctx)) {
		avcodec_free_context(&aptx->avctx);
		aptx->avctx = avcodec_alloc_context3(aptx->codec);
		if (!aptx->avctx)
			return -ENOMEM;
	}

	aptx->avctx->sample_rate = cd->out_rate;
	av_channel_layout_default(&aptx->avctx->ch_layout, 2);
	aptx->avctx->sample_fmt = AV_SAMPLE_FMT_S32P;
	aptx->avctx->frame_size = APTX_ENC_FRAME_SAMPLES;
	aptx->avctx->thread_count = 1;

	ret = avcodec_open2(aptx->avctx, aptx->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 (aptX encoder) failed %d", ret);
		return -EIO;
	}

	e->nb = aptx->avctx->frame_size ? aptx->avctx->frame_size : APTX_ENC_FRAME_SAMPLES;
	return 0;
}

static int ffmpeg_enc_aptx_encode_frame(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_aptx_data *aptx = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	aptx->frame->nb_samples = e->nb;
	aptx->frame->format = aptx->avctx->sample_fmt;
	aptx->frame->sample_rate = cd->out_rate;
	av_channel_layout_copy(&aptx->frame->ch_layout, &aptx->avctx->ch_layout);
	if (av_frame_get_buffer(aptx->frame, 0) < 0)
		return -ENOMEM;

	ffmpeg_dec_pcm_to_frame(e->pcm_in, aptx->frame,
				e->src_fmt, 2, e->nb);

	ret = avcodec_send_frame(aptx->avctx, aptx->frame);
	av_frame_unref(aptx->frame);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_frame (aptX) failed %d", ret);
		return -EIO;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(aptx->avctx, aptx->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_packet (aptX) failed %d", ret);
			return -EIO;
		}
		if (aptx->pkt->size > 0)
			ffmpeg_enc_stage_packet(mod, e, aptx->pkt->data,
						(size_t)aptx->pkt->size);
		av_packet_unref(aptx->pkt);
		ret = 0;
	}

	return 0;
}

static void ffmpeg_enc_aptx_reset(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_aptx_data *aptx = e->codec_priv;

	(void)mod;
	if (!aptx)
		return;

	if (aptx->frame)
		av_frame_unref(aptx->frame);
	if (aptx->pkt)
		av_packet_unref(aptx->pkt);
}

static void ffmpeg_enc_aptx_free(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_aptx_data *aptx = e->codec_priv;

	if (!aptx)
		return;

	if (aptx->frame)
		av_frame_free(&aptx->frame);
	if (aptx->pkt)
		av_packet_free(&aptx->pkt);
	if (aptx->avctx)
		avcodec_free_context(&aptx->avctx);
	mod_free(mod, aptx);
	e->codec_priv = NULL;
}

const struct ffmpeg_enc_codec_ops ffmpeg_enc_aptx_ops = {
	.id = FFMPEG_DEC_CODEC_APTX,
	.snd_codec_id = SND_AUDIOCODEC_APTX,
	.name = "aptx",
	.validate_params = ffmpeg_enc_aptx_validate_params,
	.init = ffmpeg_enc_aptx_init,
	.configure = ffmpeg_enc_aptx_configure,
	.encode_frame = ffmpeg_enc_aptx_encode_frame,
	.reset = ffmpeg_enc_aptx_reset,
	.free = ffmpeg_enc_aptx_free,
};
