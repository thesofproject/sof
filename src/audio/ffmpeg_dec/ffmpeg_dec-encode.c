// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// libavcodec encode backend for the ffmpeg_dec module (encode mode): PCM in,
// compressed elementary stream out (MP3 via libshine). It is the mirror of the
// decoder path - avcodec_send_frame / avcodec_receive_packet instead of
// send_packet / receive_frame.
//
// SOF PCM is interleaved S32; the encoder is opened in whatever sample format it
// supports (libshine: S16), so S32 is converted to the encoder's format per
// frame. MP3 uses fixed 1152-sample frames; this feeds one encoder frame worth
// of input per process call when enough is available.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/source_api.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdio.h>
#include "ffmpeg_dec.h"

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define FFMPEG_ENC_DEFAULT_BITRATE	128000

struct ffmpeg_enc_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
	int frame_bytes;	/* SOF S32 input frame size (all channels) */
};

int ffmpeg_enc_mod_init(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_enc_data *e;

	ffmpeg_dec_libc_bind(mod);

	e = mod_zalloc(mod, sizeof(*e));
	if (!e)
		return -ENOMEM;

	/* libshine is the fixed-point MP3 encoder built into the archive. */
	e->codec = avcodec_find_encoder_by_name("libshine");
	if (!e->codec) {
		comp_err(dev, "libshine MP3 encoder not found");
		mod_free(mod, e);
		return -ENODEV;
	}

	e->avctx = avcodec_alloc_context3(e->codec);
	e->pkt = av_packet_alloc();
	e->frame = av_frame_alloc();
	if (!e->avctx || !e->pkt || !e->frame) {
		if (e->frame)
			av_frame_free(&e->frame);
		if (e->pkt)
			av_packet_free(&e->pkt);
		if (e->avctx)
			avcodec_free_context(&e->avctx);
		mod_free(mod, e);
		return -ENOMEM;
	}

	cd->backend_data = e;
	return 0;
}

int ffmpeg_enc_mod_prepare(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;
	struct comp_dev *dev = mod->dev;
	int ret;

	if (num_of_sources != 1)
		return -EINVAL;

	ffmpeg_dec_libc_bind(mod);

	cd->out_channels = source_get_channels(sources[0]);
	cd->out_rate = source_get_rate(sources[0]);
	e->frame_bytes = source_get_frame_bytes(sources[0]);

	e->avctx->sample_rate = cd->out_rate;
	av_channel_layout_default(&e->avctx->ch_layout, cd->out_channels);
	/* Use the encoder's native sample format (libshine: S16). */
	e->avctx->sample_fmt = e->codec->sample_fmts ?
			       e->codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
	e->avctx->bit_rate = FFMPEG_ENC_DEFAULT_BITRATE;
	e->avctx->thread_count = 1;

	ret = avcodec_open2(e->avctx, e->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 (encoder) failed %d", ret);
		return -EIO;
	}

	comp_info(dev, "ffmpeg_enc: libshine MP3, rate %u ch %u frame_size %d",
		  cd->out_rate, cd->out_channels, e->avctx->frame_size);
	return 0;
}

/* Convert one interleaved S32 frame block to the encoder AVFrame, then encode. */
int ffmpeg_enc_mod_process(struct processing_module *mod,
			   struct input_stream_buffer *input_buffers, int num_input_buffers,
			   struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;
	struct comp_dev *dev = mod->dev;
	int ch = cd->out_channels;
	int nb = e->avctx->frame_size ? e->avctx->frame_size : 1152;
	const int32_t *in;
	uint8_t *out = output_buffers[0].data;
	size_t out_off = 0;
	int planar, i, c, ret;

	if (num_input_buffers != 1 || num_output_buffers != 1)
		return -EINVAL;

	ffmpeg_dec_libc_bind(mod);

	/* Need a full encoder frame of input. */
	if (input_buffers[0].size < (uint32_t)(nb * e->frame_bytes))
		return -ENODATA;

	e->frame->nb_samples = nb;
	e->frame->format = e->avctx->sample_fmt;
	e->frame->sample_rate = cd->out_rate;
	av_channel_layout_copy(&e->frame->ch_layout, &e->avctx->ch_layout);
	if (av_frame_get_buffer(e->frame, 0) < 0)
		return -ENOMEM;

	/* S32 interleaved -> S16 (planar or packed per the encoder format). */
	in = input_buffers[0].data;
	planar = av_sample_fmt_is_planar(e->frame->format);
	for (i = 0; i < nb; i++)
		for (c = 0; c < ch; c++) {
			int16_t s = (int16_t)(in[i * ch + c] >> 16);

			if (planar)
				((int16_t *)e->frame->data[c])[i] = s;
			else
				((int16_t *)e->frame->data[0])[i * ch + c] = s;
		}
	input_buffers[0].consumed = nb * e->frame_bytes;

	ret = avcodec_send_frame(e->avctx, e->frame);
	av_frame_unref(e->frame);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_frame failed %d", ret);
		return -EIO;
	}

	/* Drain compressed packets into the output buffer. */
	while (ret >= 0) {
		ret = avcodec_receive_packet(e->avctx, e->pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_packet failed %d", ret);
			return -EIO;
		}
		if (out_off + e->pkt->size <= output_buffers[0].size) {
			memcpy_s(out + out_off, output_buffers[0].size - out_off,
				 e->pkt->data, e->pkt->size);
			out_off += e->pkt->size;
		} else {
			comp_warn(dev, "output full, MP3 packet dropped");
		}
		av_packet_unref(e->pkt);
		ret = 0;
	}

	output_buffers[0].size = out_off;
	return 0;
}

int ffmpeg_enc_mod_free(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;

	ffmpeg_dec_libc_bind(mod);
	if (!e)
		return 0;

	if (e->frame)
		av_frame_free(&e->frame);
	if (e->pkt)
		av_packet_free(&e->pkt);
	if (e->avctx)
		avcodec_free_context(&e->avctx);
	mod_free(mod, e);
	cd->backend_data = NULL;
	return 0;
}
