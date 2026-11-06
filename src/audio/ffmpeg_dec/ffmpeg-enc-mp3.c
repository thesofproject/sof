// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// MP3 per-codec encoder implementation (libshine via libavcodec) for ffmpeg_dec.

#include <sof/audio/module_adapter/module/generic.h>
#include <ipc/compress_params.h>
#include <errno.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include "ffmpeg_dec.h"

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

struct ffmpeg_enc_mp3_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
};

static int ffmpeg_enc_mp3_validate_params(struct processing_module *mod,
					  const struct snd_codec *codec_params)
{
	struct comp_dev *dev = mod->dev;
	uint32_t ch = codec_params->ch_in ? codec_params->ch_in : codec_params->ch_out;

	if (codec_params->format != SND_AUDIOSTREAMFORMAT_UNDEFINED) {
		comp_err(dev, "unsupported MP3 stream format 0x%x",
			 codec_params->format);
		return -ENOTSUP;
	}
	if (codec_params->ch_mode != 0 &&
	    codec_params->ch_mode != SND_AUDIOCHANMODE_MP3_MONO &&
	    codec_params->ch_mode != SND_AUDIOCHANMODE_MP3_STEREO) {
		comp_err(dev, "unsupported MP3 channel mode 0x%x",
			 codec_params->ch_mode);
		return -ENOTSUP;
	}
	if (ch &&
	    ((codec_params->ch_mode == SND_AUDIOCHANMODE_MP3_MONO && ch != 1) ||
	     (codec_params->ch_mode == SND_AUDIOCHANMODE_MP3_STEREO && ch != 2))) {
		comp_err(dev, "MP3 channel mode 0x%x does not match channels %u",
			 codec_params->ch_mode, ch);
		return -EINVAL;
	}
	if (codec_params->bit_rate % 1000 != 0) {
		comp_err(dev, "MP3 bit_rate %u is not a multiple of 1000 bps",
			 codec_params->bit_rate);
		return -EINVAL;
	}

	return 0;
}

static int ffmpeg_enc_mp3_init(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_mp3_data *mp3;

	mp3 = mod_zalloc(mod, sizeof(*mp3));
	if (!mp3)
		return -ENOMEM;

	mp3->codec = avcodec_find_encoder_by_name("libshine");
	if (!mp3->codec) {
		comp_err(dev, "libshine MP3 encoder not found");
		mod_free(mod, mp3);
		return -ENODEV;
	}

	mp3->avctx = avcodec_alloc_context3(mp3->codec);
	mp3->pkt = av_packet_alloc();
	mp3->frame = av_frame_alloc();
	if (!mp3->avctx || !mp3->pkt || !mp3->frame) {
		if (mp3->frame)
			av_frame_free(&mp3->frame);
		if (mp3->pkt)
			av_packet_free(&mp3->pkt);
		if (mp3->avctx)
			avcodec_free_context(&mp3->avctx);
		mod_free(mod, mp3);
		return -ENOMEM;
	}

	e->codec_priv = mp3;
	return 0;
}

static int ffmpeg_enc_mp3_configure(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_mp3_data *mp3 = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	if ((e->req_ch_mode == SND_AUDIOCHANMODE_MP3_MONO && cd->out_channels != 1) ||
	    (e->req_ch_mode == SND_AUDIOCHANMODE_MP3_STEREO && cd->out_channels != 2)) {
		comp_err(dev, "MP3 channel mode 0x%x does not match source channels %u",
			 e->req_ch_mode, cd->out_channels);
		return -EINVAL;
	}
	if (e->bit_rate == 0 || e->bit_rate % 1000 != 0) {
		comp_err(dev, "MP3 bit_rate %u is invalid or not a multiple of 1000 bps",
			 e->bit_rate);
		return -EINVAL;
	}

	if (avcodec_is_open(mp3->avctx)) {
		avcodec_free_context(&mp3->avctx);
		mp3->avctx = avcodec_alloc_context3(mp3->codec);
		if (!mp3->avctx)
			return -ENOMEM;
	}

	mp3->avctx->sample_rate = cd->out_rate;
	av_channel_layout_default(&mp3->avctx->ch_layout, cd->out_channels);
	mp3->avctx->sample_fmt = AV_SAMPLE_FMT_S16P;
	mp3->avctx->bit_rate = e->bit_rate;
	mp3->avctx->thread_count = 1;

	ret = avcodec_open2(mp3->avctx, mp3->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 (MP3 encoder) failed %d", ret);
		return -EIO;
	}

	e->nb = mp3->avctx->frame_size ? mp3->avctx->frame_size : 1152;
	return 0;
}

static int ffmpeg_enc_mp3_encode_frame(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_mp3_data *mp3 = e->codec_priv;
	struct comp_dev *dev = mod->dev;
	int ch = cd->out_channels;
	int planar;
	int ret;

	mp3->frame->nb_samples = e->nb;
	mp3->frame->format = mp3->avctx->sample_fmt;
	mp3->frame->sample_rate = cd->out_rate;
	av_channel_layout_copy(&mp3->frame->ch_layout, &mp3->avctx->ch_layout);
	if (av_frame_get_buffer(mp3->frame, 0) < 0)
		return -ENOMEM;

	planar = av_sample_fmt_is_planar(mp3->frame->format);
	if (planar) {
		ffmpeg_enc_pcm_to_s16_planar(e->pcm_in, mp3->frame->data,
					     e->nb, ch, e->src_fmt);
	} else {
		ffmpeg_enc_pcm_to_s16_interleaved(e->pcm_in,
						  (int16_t *)mp3->frame->data[0],
						  (size_t)e->nb * ch,
						  e->src_fmt);
	}

	ret = avcodec_send_frame(mp3->avctx, mp3->frame);
	av_frame_unref(mp3->frame);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_frame failed %d", ret);
		return -EIO;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(mp3->avctx, mp3->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_packet failed %d", ret);
			return -EIO;
		}
		if (mp3->pkt->size > 0)
			ffmpeg_enc_stage_packet(mod, e, mp3->pkt->data,
						(size_t)mp3->pkt->size);
		av_packet_unref(mp3->pkt);
		ret = 0;
	}

	return 0;
}

static void ffmpeg_enc_mp3_reset(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_mp3_data *mp3 = e->codec_priv;

	(void)mod;
	if (!mp3)
		return;

	if (mp3->frame)
		av_frame_unref(mp3->frame);
	if (mp3->pkt)
		av_packet_unref(mp3->pkt);
}

static void ffmpeg_enc_mp3_free(struct processing_module *mod, struct ffmpeg_enc_data *e)
{
	struct ffmpeg_enc_mp3_data *mp3 = e->codec_priv;

	if (!mp3)
		return;

	if (mp3->frame)
		av_frame_free(&mp3->frame);
	if (mp3->pkt)
		av_packet_free(&mp3->pkt);
	if (mp3->avctx)
		avcodec_free_context(&mp3->avctx);
	mod_free(mod, mp3);
	e->codec_priv = NULL;
}

const struct ffmpeg_enc_codec_ops ffmpeg_enc_mp3_ops = {
	.id = FFMPEG_DEC_CODEC_MP3,
	.snd_codec_id = SND_AUDIOCODEC_MP3,
	.name = "mp3",
	.validate_params = ffmpeg_enc_mp3_validate_params,
	.init = ffmpeg_enc_mp3_init,
	.configure = ffmpeg_enc_mp3_configure,
	.encode_frame = ffmpeg_enc_mp3_encode_frame,
	.reset = ffmpeg_enc_mp3_reset,
	.free = ffmpeg_enc_mp3_free,
};
