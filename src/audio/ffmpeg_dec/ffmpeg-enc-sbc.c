// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Bluetooth SBC / mSBC per-codec encoder implementation for ffmpeg_dec.

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

#define SBC_DEFAULT_STEREO_BITRATE	328000
#define SBC_DEFAULT_MONO_BITRATE	198000
#define MSBC_SAMPLE_RATE		16000
#define MSBC_FRAME_SAMPLES		120

struct ffmpeg_enc_sbc_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
	bool is_msbc;
	int decimate;
};

static int ffmpeg_enc_sbc_validate_params(struct processing_module *mod,
					  const struct snd_codec *codec_params)
{
	struct comp_dev *dev = mod->dev;

	if (codec_params->format != SND_AUDIOSTREAMFORMAT_UNDEFINED &&
	    codec_params->format != SND_AUDIOSTREAMFORMAT_RAW &&
	    codec_params->format != 2 &&
	    codec_params->format != 10) {
		comp_err(dev, "unsupported SBC stream format 0x%x",
			 codec_params->format);
		return -ENOTSUP;
	}

	return 0;
}

static int ffmpeg_enc_sbc_init(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_sbc_data *sbc;

	sbc = mod_zalloc(mod, sizeof(*sbc));
	if (!sbc)
		return -ENOMEM;

	sbc->codec = avcodec_find_encoder(AV_CODEC_ID_SBC);
	if (!sbc->codec) {
		comp_err(dev, "libavcodec SBC encoder not found");
		mod_free(mod, sbc);
		return -ENODEV;
	}

	sbc->avctx = avcodec_alloc_context3(sbc->codec);
	sbc->pkt = av_packet_alloc();
	sbc->frame = av_frame_alloc();
	if (!sbc->avctx || !sbc->pkt || !sbc->frame) {
		if (sbc->frame)
			av_frame_free(&sbc->frame);
		if (sbc->pkt)
			av_packet_free(&sbc->pkt);
		if (sbc->avctx)
			avcodec_free_context(&sbc->avctx);
		mod_free(mod, sbc);
		return -ENOMEM;
	}

	e->codec_priv = sbc;
	return 0;
}

static int ffmpeg_enc_sbc_configure(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_sbc_data *sbc = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	uint32_t enc_ch = e->req_channels ? e->req_channels : cd->out_channels;
	uint32_t enc_rate = e->req_rate ? e->req_rate : cd->out_rate;
	int ret;

	if (enc_ch < 1 || enc_ch > 2) {
		comp_err(dev, "unsupported SBC channel count %u", enc_ch);
		return -EINVAL;
	}

	if (avcodec_is_open(sbc->avctx)) {
		avcodec_free_context(&sbc->avctx);
		sbc->avctx = avcodec_alloc_context3(sbc->codec);
		if (!sbc->avctx)
			return -ENOMEM;
	}

	/*
	 * Select Bluetooth HFP Wideband Speech mSBC only when explicitly
	 * requested via profile == AV_PROFILE_SBC_MSBC (1).
	 */
	sbc->is_msbc = (e->req_profile == AV_PROFILE_SBC_MSBC);
	sbc->decimate = 1;

	if (sbc->is_msbc) {
		if (cd->out_rate == 48000) {
			sbc->decimate = 3;
		} else if (cd->out_rate != MSBC_SAMPLE_RATE) {
			comp_err(dev, "mSBC requires 16 kHz or 48 kHz source rate (got %u)",
				 cd->out_rate);
			return -EINVAL;
		}
		sbc->avctx->profile = AV_PROFILE_SBC_MSBC;
		sbc->avctx->sample_rate = MSBC_SAMPLE_RATE;
		av_channel_layout_default(&sbc->avctx->ch_layout, 1);
	} else {
		if (enc_rate != cd->out_rate) {
			comp_err(dev, "SBC sample rate %u does not match source rate %u",
				 enc_rate, cd->out_rate);
			return -EINVAL;
		}
		sbc->avctx->sample_rate = enc_rate;
		av_channel_layout_default(&sbc->avctx->ch_layout, enc_ch);
		sbc->avctx->bit_rate = e->req_bit_rate ? e->req_bit_rate :
				       (enc_ch == 2 ? SBC_DEFAULT_STEREO_BITRATE :
						      SBC_DEFAULT_MONO_BITRATE);
	}

	sbc->avctx->sample_fmt = AV_SAMPLE_FMT_S16;
	sbc->avctx->thread_count = 1;

	ret = avcodec_open2(sbc->avctx, sbc->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 (SBC encoder) failed %d", ret);
		return -EIO;
	}

	if (sbc->is_msbc)
		e->nb = MSBC_FRAME_SAMPLES * sbc->decimate;
	else
		e->nb = sbc->avctx->frame_size ? sbc->avctx->frame_size : 128;

	return 0;
}

static int ffmpeg_enc_sbc_encode_frame(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_sbc_data *sbc = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int enc_samples = sbc->is_msbc ? MSBC_FRAME_SAMPLES : e->nb;
	int ret;

	sbc->frame->nb_samples = enc_samples;
	sbc->frame->format = sbc->avctx->sample_fmt;
	sbc->frame->sample_rate = sbc->avctx->sample_rate;
	av_channel_layout_copy(&sbc->frame->ch_layout, &sbc->avctx->ch_layout);
	if (av_frame_get_buffer(sbc->frame, 0) < 0)
		return -ENOMEM;

	if (sbc->is_msbc && sbc->decimate == 3) {
		ffmpeg_dec_pcm_decimate_3to1_s16_mono(e->pcm_in,
						      (int16_t *)sbc->frame->data[0],
						      e->src_fmt, cd->out_channels,
						      MSBC_FRAME_SAMPLES,
						      e->decim_hist);
	} else {
		ffmpeg_dec_pcm_to_frame(e->pcm_in, sbc->frame,
					e->src_fmt, cd->out_channels,
					enc_samples);
	}

	ret = avcodec_send_frame(sbc->avctx, sbc->frame);
	av_frame_unref(sbc->frame);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_frame (SBC) failed %d", ret);
		return -EIO;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(sbc->avctx, sbc->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_packet (SBC) failed %d", ret);
			return -EIO;
		}
		if (sbc->pkt->size > 0)
			ffmpeg_enc_stage_packet(mod, e, sbc->pkt->data,
						(size_t)sbc->pkt->size);
		av_packet_unref(sbc->pkt);
		ret = 0;
	}

	return 0;
}

static void ffmpeg_enc_sbc_reset(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_sbc_data *sbc = e->codec_priv;

	(void)mod;
	if (!sbc)
		return;

	if (sbc->frame)
		av_frame_unref(sbc->frame);
	if (sbc->pkt)
		av_packet_unref(sbc->pkt);
}

static void ffmpeg_enc_sbc_free(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_sbc_data *sbc = e->codec_priv;

	if (!sbc)
		return;

	if (sbc->frame)
		av_frame_free(&sbc->frame);
	if (sbc->pkt)
		av_packet_free(&sbc->pkt);
	if (sbc->avctx)
		avcodec_free_context(&sbc->avctx);
	mod_free(mod, sbc);
	e->codec_priv = NULL;
}

const struct ffmpeg_enc_codec_ops ffmpeg_enc_sbc_ops = {
	.id = FFMPEG_DEC_CODEC_SBC,
	.snd_codec_id = SND_AUDIOCODEC_SBC,
	.name = "sbc",
	.validate_params = ffmpeg_enc_sbc_validate_params,
	.init = ffmpeg_enc_sbc_init,
	.configure = ffmpeg_enc_sbc_configure,
	.encode_frame = ffmpeg_enc_sbc_encode_frame,
	.reset = ffmpeg_enc_sbc_reset,
	.free = ffmpeg_enc_sbc_free,
};
