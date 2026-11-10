// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// ALAC per-codec encoder implementation (native libavcodec alac) for ffmpeg_dec.

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

#define ALAC_EXTRADATA_SIZE	36
#define ALAC_ENC_FRAME_SAMPLES	1024

struct ffmpeg_enc_alac_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
	bool header_sent;
};

static int ffmpeg_enc_alac_validate_params(struct processing_module *mod,
					   const struct snd_codec *codec_params)
{
	struct comp_dev *dev = mod->dev;

	if (codec_params->ch_in > 2 || codec_params->ch_out > 2) {
		comp_err(dev, "unsupported ALAC channel count in=%u out=%u",
			 codec_params->ch_in, codec_params->ch_out);
		return -ENOTSUP;
	}

	/*
	 * Accept UNDEFINED (0), RAW stream format flag, and the ALSA
	 * SNDRV_PCM_FORMAT_S16_LE (2) / SNDRV_PCM_FORMAT_S32_LE (10) values
	 * that tinycompress crecord places in codec.format via -F.
	 */
	if (codec_params->format != SND_AUDIOSTREAMFORMAT_UNDEFINED &&
	    codec_params->format != SND_AUDIOSTREAMFORMAT_RAW &&
	    codec_params->format != 2 &&
	    codec_params->format != 10) {
		comp_err(dev, "unsupported ALAC stream format 0x%x",
			 codec_params->format);
		return -ENOTSUP;
	}

	return 0;
}

static int ffmpeg_enc_alac_init(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_alac_data *alac;

	alac = mod_zalloc(mod, sizeof(*alac));
	if (!alac)
		return -ENOMEM;

	alac->codec = avcodec_find_encoder(AV_CODEC_ID_ALAC);
	if (!alac->codec) {
		comp_err(dev, "libavcodec ALAC encoder not found");
		mod_free(mod, alac);
		return -ENODEV;
	}

	alac->avctx = avcodec_alloc_context3(alac->codec);
	alac->pkt = av_packet_alloc();
	alac->frame = av_frame_alloc();
	if (!alac->avctx || !alac->pkt || !alac->frame) {
		if (alac->frame)
			av_frame_free(&alac->frame);
		if (alac->pkt)
			av_packet_free(&alac->pkt);
		if (alac->avctx)
			avcodec_free_context(&alac->avctx);
		mod_free(mod, alac);
		return -ENOMEM;
	}

	e->codec_priv = alac;
	return 0;
}

static int ffmpeg_enc_alac_configure(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_alac_data *alac = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	if (cd->out_channels < 1 || cd->out_channels > 2) {
		comp_err(dev, "unsupported ALAC channel count %u", cd->out_channels);
		return -EINVAL;
	}

	if (avcodec_is_open(alac->avctx)) {
		avcodec_free_context(&alac->avctx);
		alac->avctx = avcodec_alloc_context3(alac->codec);
		if (!alac->avctx)
			return -ENOMEM;
	}

	alac->avctx->sample_rate = cd->out_rate;
	av_channel_layout_default(&alac->avctx->ch_layout, cd->out_channels);
	alac->avctx->sample_fmt = AV_SAMPLE_FMT_S16P;
	/*
	 * Compression level 1 selects ALAC's fixed 6th-order integer predictor,
	 * avoiding double-precision autocorrelation/Levinson-Durbin at runtime.
	 */
	alac->avctx->compression_level = 1;
	alac->avctx->thread_count = 1;

	ret = avcodec_open2(alac->avctx, alac->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 (ALAC encoder) failed %d", ret);
		return -EIO;
	}

	/*
	 * Use 1024-sample frames (~21.3 ms @ 48 kHz) so each frame fits within
	 * the 16 KiB DP PCM/output staging rings while writing explicit frame
	 * sample counts in the ALAC frame headers.
	 */
	alac->avctx->frame_size = ALAC_ENC_FRAME_SAMPLES;
	e->nb = ALAC_ENC_FRAME_SAMPLES;
	alac->header_sent = false;
	return 0;
}

static int ffmpeg_enc_alac_encode_frame(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_alac_data *alac = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ch = cd->out_channels;
	int ret;

	/*
	 * Prepend the 36-byte QuickTime 'alac' magic cookie atom before the
	 * first encoded frame so the captured stream is self-describing and
	 * can be decoded directly by ffmpeg_dec-alac.
	 */
	if (!alac->header_sent) {
		if (alac->avctx->extradata &&
		    alac->avctx->extradata_size >= ALAC_EXTRADATA_SIZE) {
			ffmpeg_enc_stage_packet(mod, e, alac->avctx->extradata,
						ALAC_EXTRADATA_SIZE);
		}
		alac->header_sent = true;
	}

	alac->frame->nb_samples = e->nb;
	alac->frame->format = alac->avctx->sample_fmt;
	alac->frame->sample_rate = cd->out_rate;
	av_channel_layout_copy(&alac->frame->ch_layout, &alac->avctx->ch_layout);
	if (av_frame_get_buffer(alac->frame, 0) < 0)
		return -ENOMEM;

	ffmpeg_dec_pcm_to_frame(e->pcm_in, alac->frame,
				e->src_fmt, ch, e->nb);

	ret = avcodec_send_frame(alac->avctx, alac->frame);
	av_frame_unref(alac->frame);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_frame (ALAC) failed %d", ret);
		return -EIO;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(alac->avctx, alac->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_packet (ALAC) failed %d", ret);
			return -EIO;
		}
		if (alac->pkt->size > 0)
			ffmpeg_enc_stage_packet(mod, e, alac->pkt->data,
						(size_t)alac->pkt->size);
		av_packet_unref(alac->pkt);
		ret = 0;
	}

	return 0;
}

static void ffmpeg_enc_alac_reset(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_alac_data *alac = e->codec_priv;

	(void)mod;
	if (!alac)
		return;

	if (alac->frame)
		av_frame_unref(alac->frame);
	if (alac->pkt)
		av_packet_unref(alac->pkt);
	alac->header_sent = false;
}

static void ffmpeg_enc_alac_free(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_alac_data *alac = e->codec_priv;

	if (!alac)
		return;

	if (alac->frame)
		av_frame_free(&alac->frame);
	if (alac->pkt)
		av_packet_free(&alac->pkt);
	if (alac->avctx)
		avcodec_free_context(&alac->avctx);
	mod_free(mod, alac);
	e->codec_priv = NULL;
}

const struct ffmpeg_enc_codec_ops ffmpeg_enc_alac_ops = {
	.id = FFMPEG_DEC_CODEC_ALAC,
	.snd_codec_id = SND_AUDIOCODEC_ALAC,
	.name = "alac",
	.validate_params = ffmpeg_enc_alac_validate_params,
	.init = ffmpeg_enc_alac_init,
	.configure = ffmpeg_enc_alac_configure,
	.encode_frame = ffmpeg_enc_alac_encode_frame,
	.reset = ffmpeg_enc_alac_reset,
	.free = ffmpeg_enc_alac_free,
};
