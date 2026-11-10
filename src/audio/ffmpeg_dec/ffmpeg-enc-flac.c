// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// FLAC per-codec encoder implementation (native libavcodec flac) for ffmpeg_dec.

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

#define FLAC_STREAMINFO_SIZE	34
#define FLAC_CONTAINER_HDR_SIZE	(4 + 4 + FLAC_STREAMINFO_SIZE)

struct ffmpeg_enc_flac_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
	bool header_sent;
};

static int ffmpeg_enc_flac_validate_params(struct processing_module *mod,
					   const struct snd_codec *codec_params)
{
	struct comp_dev *dev = mod->dev;

	/*
	 * Accept UNDEFINED (0), FLAC/RAW stream format flags, and the ALSA
	 * SNDRV_PCM_FORMAT_S16_LE (2) / SNDRV_PCM_FORMAT_S32_LE (10) values
	 * that tinycompress crecord places in codec.format via -F.
	 */
	if (codec_params->format != SND_AUDIOSTREAMFORMAT_UNDEFINED &&
	    codec_params->format != SND_AUDIOSTREAMFORMAT_FLAC &&
	    codec_params->format != SND_AUDIOSTREAMFORMAT_RAW &&
	    codec_params->format != 2 &&
	    codec_params->format != 10) {
		comp_err(dev, "unsupported FLAC stream format 0x%x",
			 codec_params->format);
		return -ENOTSUP;
	}

	return 0;
}

static int ffmpeg_enc_flac_init(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_flac_data *flac;

	flac = mod_zalloc(mod, sizeof(*flac));
	if (!flac)
		return -ENOMEM;

	flac->codec = avcodec_find_encoder(AV_CODEC_ID_FLAC);
	if (!flac->codec) {
		comp_err(dev, "libavcodec FLAC encoder not found");
		mod_free(mod, flac);
		return -ENODEV;
	}

	flac->avctx = avcodec_alloc_context3(flac->codec);
	flac->pkt = av_packet_alloc();
	flac->frame = av_frame_alloc();
	if (!flac->avctx || !flac->pkt || !flac->frame) {
		if (flac->frame)
			av_frame_free(&flac->frame);
		if (flac->pkt)
			av_packet_free(&flac->pkt);
		if (flac->avctx)
			avcodec_free_context(&flac->avctx);
		mod_free(mod, flac);
		return -ENOMEM;
	}

	e->codec_priv = flac;
	return 0;
}

static int ffmpeg_enc_flac_configure(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_flac_data *flac = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	if (cd->out_channels < 1 || cd->out_channels > 2) {
		comp_err(dev, "unsupported FLAC channel count %u", cd->out_channels);
		return -EINVAL;
	}

	if (avcodec_is_open(flac->avctx)) {
		avcodec_free_context(&flac->avctx);
		flac->avctx = avcodec_alloc_context3(flac->codec);
		if (!flac->avctx)
			return -ENOMEM;
	}

	flac->avctx->sample_rate = cd->out_rate;
	av_channel_layout_default(&flac->avctx->ch_layout, cd->out_channels);
	flac->avctx->sample_fmt = AV_SAMPLE_FMT_S16;
	/*
	 * Compression level 1 selects FF_LPC_TYPE_FIXED (pure integer fixed
	 * predictors of order 0..4, 1152-sample block size), avoiding
	 * double-precision autocorrelation/Levinson-Durbin at runtime.
	 */
	flac->avctx->compression_level = 1;
	flac->avctx->frame_size = 1152;
	flac->avctx->thread_count = 1;

	ret = avcodec_open2(flac->avctx, flac->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 (FLAC encoder) failed %d", ret);
		return -EIO;
	}

	e->nb = flac->avctx->frame_size ? flac->avctx->frame_size : 1152;
	flac->header_sent = false;
	return 0;
}

static int ffmpeg_enc_flac_encode_frame(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_flac_data *flac = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ch = cd->out_channels;
	int ret;

	/*
	 * Emit the 42-byte native FLAC container header ("fLaC" + 4-byte
	 * metadata block header [is_last=1, type=STREAMINFO(0), len=34] +
	 * 34-byte STREAMINFO from avctx->extradata) before the first frame
	 * so the captured stream is a valid standalone .flac file.
	 */
	if (!flac->header_sent) {
		if (flac->avctx->extradata &&
		    flac->avctx->extradata_size >= FLAC_STREAMINFO_SIZE) {
			uint8_t hdr[FLAC_CONTAINER_HDR_SIZE];

			memcpy_s(&hdr[0], 4, "fLaC", 4);
			hdr[4] = 0x80;	/* is_last = 1, type = 0 (STREAMINFO) */
			hdr[5] = 0x00;
			hdr[6] = 0x00;
			hdr[7] = FLAC_STREAMINFO_SIZE;
			memcpy_s(&hdr[8], FLAC_STREAMINFO_SIZE,
				 flac->avctx->extradata, FLAC_STREAMINFO_SIZE);
			ffmpeg_enc_stage_packet(mod, e, hdr, sizeof(hdr));
		}
		flac->header_sent = true;
	}

	flac->frame->nb_samples = e->nb;
	flac->frame->format = flac->avctx->sample_fmt;
	flac->frame->sample_rate = cd->out_rate;
	av_channel_layout_copy(&flac->frame->ch_layout, &flac->avctx->ch_layout);
	if (av_frame_get_buffer(flac->frame, 0) < 0)
		return -ENOMEM;

	ffmpeg_dec_pcm_to_frame(e->pcm_in, flac->frame,
				e->src_fmt, ch, e->nb);

	ret = avcodec_send_frame(flac->avctx, flac->frame);
	av_frame_unref(flac->frame);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_frame (FLAC) failed %d", ret);
		return -EIO;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(flac->avctx, flac->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_packet (FLAC) failed %d", ret);
			return -EIO;
		}
		if (flac->pkt->size > 0)
			ffmpeg_enc_stage_packet(mod, e, flac->pkt->data,
						(size_t)flac->pkt->size);
		av_packet_unref(flac->pkt);
		ret = 0;
	}

	return 0;
}

static void ffmpeg_enc_flac_reset(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_flac_data *flac = e->codec_priv;

	(void)mod;
	if (!flac)
		return;

	if (flac->frame)
		av_frame_unref(flac->frame);
	if (flac->pkt)
		av_packet_unref(flac->pkt);
	flac->header_sent = false;
}

static void ffmpeg_enc_flac_free(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_flac_data *flac = e->codec_priv;

	if (!flac)
		return;

	if (flac->frame)
		av_frame_free(&flac->frame);
	if (flac->pkt)
		av_packet_free(&flac->pkt);
	if (flac->avctx)
		avcodec_free_context(&flac->avctx);
	mod_free(mod, flac);
	e->codec_priv = NULL;
}

const struct ffmpeg_enc_codec_ops ffmpeg_enc_flac_ops = {
	.id = FFMPEG_DEC_CODEC_FLAC,
	.snd_codec_id = SND_AUDIOCODEC_FLAC,
	.name = "flac",
	.validate_params = ffmpeg_enc_flac_validate_params,
	.init = ffmpeg_enc_flac_init,
	.configure = ffmpeg_enc_flac_configure,
	.encode_frame = ffmpeg_enc_flac_encode_frame,
	.reset = ffmpeg_enc_flac_reset,
	.free = ffmpeg_enc_flac_free,
};
