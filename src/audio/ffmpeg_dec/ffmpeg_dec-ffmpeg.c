// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// libavcodec decode backend for the ffmpeg_dec module.
//
// Drives the modern send-packet / receive-frame libavcodec API standalone (no
// libavformat, no file I/O). A raw compressed elementary stream is parsed on
// the DSP with an AVCodecParser (av_parser_parse2), each recovered frame is fed
// to avcodec_send_packet, and decoded PCM is drained with avcodec_receive_frame
// and interleaved into the SOF output buffer.
//
// Requires pre-compiled libavcodec / libavutil / libswresample static libraries
// and headers for the target under third_party/ (see the Phase 0 build). It is
// only compiled when CONFIG_COMP_FFMPEG_DEC_STUB is not selected.

#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/string.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include "ffmpeg_dec.h"

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

/* Padding libavcodec's bitstream readers over-read past the end of a packet. */
#ifndef AV_INPUT_BUFFER_PADDING_SIZE
#define AV_INPUT_BUFFER_PADDING_SIZE	64
#endif

/*
 * Route FFmpeg's av_log() into the SOF/Zephyr logging subsystem instead of its
 * default callback (which writes to stderr via fprintf). The line buffer is
 * static: decode is single-threaded, and a static buffer avoids handing Zephyr's
 * deferred logging a pointer into a stack frame that has since been unwound.
 */
static char ffmpeg_dec_log_line[160];

static void ffmpeg_dec_av_log(void *avcl, int level, const char *fmt, va_list vl)
{
	int n;

	if (level > av_log_get_level())
		return;

	n = vsnprintf(ffmpeg_dec_log_line, sizeof(ffmpeg_dec_log_line), fmt, vl);
	if (n <= 0)
		return;

	/* Trim the trailing newline FFmpeg conventionally appends. */
	if (n < (int)sizeof(ffmpeg_dec_log_line) && ffmpeg_dec_log_line[n - 1] == '\n')
		ffmpeg_dec_log_line[n - 1] = '\0';

	if (level <= AV_LOG_ERROR) {
		/* A stalled/hostile stream (e.g. the silence padding a compress
		 * host emits after the file ends) can make libavcodec log the
		 * same error every DSP cycle. Collapse identical repeats so the
		 * trace ring keeps room for other messages.
		 */
		static char last[64];
		static int reps;

		if (!strncmp(last, ffmpeg_dec_log_line, sizeof(last) - 1)) {
			if (++reps > 3)
				return;
		} else {
			reps = 0;
			strncpy(last, ffmpeg_dec_log_line, sizeof(last) - 1);
		}
		LOG_ERR("ffmpeg: %s", ffmpeg_dec_log_line);
	} else if (level <= AV_LOG_WARNING)
		LOG_WRN("ffmpeg: %s", ffmpeg_dec_log_line);
	else if (level <= AV_LOG_INFO)
		LOG_INF("ffmpeg: %s", ffmpeg_dec_log_line);
	else
		LOG_DBG("ffmpeg: %s", ffmpeg_dec_log_line);
}

/**
 * struct ffmpeg_dec_ffmpeg_data - libavcodec backend private state.
 * @avctx:   Decoder context.
 * @parser:  Elementary-stream frame parser.
 * @pkt:     Reusable packet handed to the decoder.
 * @frame:   Reusable decoded PCM frame.
 * @pktbuf:  Padded scratch buffer for parsed packet payloads.
 * @codec:   Resolved AVCodec for the selected codec id.
 */
struct ffmpeg_dec_ffmpeg_data {
	AVCodecContext *avctx;
	AVCodecParserContext *parser;
	AVPacket *pkt;
	AVFrame *frame;
	uint8_t *pktbuf;
	const AVCodec *codec;
};

/* Map the SOF codec enum to a libavcodec decoder id. */
static enum AVCodecID ffmpeg_dec_av_codec_id(enum ffmpeg_dec_codec codec)
{
	switch (codec) {
#if defined(CONFIG_FFMPEG_DEC_FLAC)
	case FFMPEG_DEC_CODEC_FLAC:
		return AV_CODEC_ID_FLAC;
#endif
#if defined(CONFIG_FFMPEG_DEC_AAC)
	case FFMPEG_DEC_CODEC_AAC:
		return AV_CODEC_ID_AAC;
#endif
#if defined(CONFIG_FFMPEG_DEC_OPUS)
	case FFMPEG_DEC_CODEC_OPUS:
		return AV_CODEC_ID_OPUS;
#endif
#if defined(CONFIG_FFMPEG_DEC_MP3)
	case FFMPEG_DEC_CODEC_MP3:
		return AV_CODEC_ID_MP3;
#endif
	default:
		return AV_CODEC_ID_NONE;
	}
}

static int ffmpeg_dec_ff_init(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_dec_ffmpeg_data *ff;
	enum AVCodecID id;

	/* Bind the heap that backs FFmpeg's malloc before any av_malloc runs. */
	ffmpeg_dec_libc_bind(mod);

	/* Redirect libavcodec logging into SOF/Zephyr before anything can log. */
	av_log_set_level(AV_LOG_ERROR);
	av_log_set_callback(ffmpeg_dec_av_log);

	ff = mod_zalloc(mod, sizeof(*ff));
	if (!ff)
		return -ENOMEM;

	id = ffmpeg_dec_av_codec_id(cd->codec);
	ff->codec = avcodec_find_decoder(id);
	if (!ff->codec) {
		comp_err(dev, "no libavcodec decoder for codec %d", cd->codec);
		mod_free(mod, ff);
		return -ENODEV;
	}

	ff->parser = av_parser_init(ff->codec->id);
	ff->avctx = avcodec_alloc_context3(ff->codec);
	ff->pkt = av_packet_alloc();
	ff->frame = av_frame_alloc();
	if (!ff->avctx || !ff->pkt || !ff->frame) {
		comp_err(dev, "libavcodec object allocation failed");
		goto err;
	}
	comp_info(dev, "ff_init: libavcodec ready (%s)", ff->codec->name);

	cd->backend_data = ff;
	return 0;

err:
	if (ff->frame)
		av_frame_free(&ff->frame);
	if (ff->pkt)
		av_packet_free(&ff->pkt);
	if (ff->avctx)
		avcodec_free_context(&ff->avctx);
	if (ff->parser)
		av_parser_close(ff->parser);
	mod_free(mod, ff);
	return -ENOMEM;
}

static int ffmpeg_dec_ff_configure(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_dec_ffmpeg_data *ff = cd->backend_data;
	struct comp_dev *dev = mod->dev;
	int ret;

	ffmpeg_dec_libc_bind(mod);

	/* Some codecs (FLAC, Opus, Vorbis, AAC-in-MP4) need their setup header
	 * in avctx->extradata before avcodec_open2().
	 */
	if (cd->extradata && cd->extradata_size) {
		av_freep(&ff->avctx->extradata);
		ff->avctx->extradata =
			av_mallocz(cd->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
		if (!ff->avctx->extradata)
			return -ENOMEM;
		memcpy_s(ff->avctx->extradata, cd->extradata_size,
			 cd->extradata, cd->extradata_size);
		ff->avctx->extradata_size = cd->extradata_size;
	}

	/* Force single-threaded decode: no pthreads, no frame-threading latency,
	 * and get_buffer2 need not be thread-safe.
	 */
	ff->avctx->thread_count = 1;

	/* avcodec_alloc_context3() does not apply the AVOption defaults on this
	 * cut-down static build, so max_samples is left zero-initialised. That
	 * makes ff_get_buffer() reject every audio frame ("samples per frame N
	 * exceeds max_samples 0", -EINVAL). Restore the upstream default.
	 */
	if (ff->avctx->max_samples <= 0)
		ff->avctx->max_samples = INT_MAX;

	ret = avcodec_open2(ff->avctx, ff->codec, NULL);
	if (ret < 0) {
		comp_err(dev, "avcodec_open2 failed %d", ret);
		return -EIO;
	}

	/* Padded scratch for a parsed packet payload, sized to OBS as a bound. */
	ff->pktbuf = mod_alloc(mod, cd->out_frame_bytes ?
			       cd->out_frame_bytes * 4096 : 65536);
	if (!ff->pktbuf)
		return -ENOMEM;

	return 0;
}

/*
 * Interleave one decoded AVFrame into @out, honouring planar vs packed layout.
 * Returns bytes written, or a negative errno if @out cannot hold the frame.
 * NOTE: for the first FLAC bring-up the output sample format is assumed to match
 * the sink (S16/S32). Format/rate conversion (libswresample) is a follow-up.
 */
static int ffmpeg_dec_emit_frame(struct processing_module *mod, AVFrame *frame,
				 uint8_t *out, size_t out_size)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	int channels = cd->out_channels;
	int bps = av_get_bytes_per_sample(frame->format);
	int planar = av_sample_fmt_is_planar(frame->format);
	size_t need = (size_t)frame->nb_samples * channels * bps;
	int i, ch;

	if (need > out_size)
		return -ENOSPC;

	if (!planar) {
		memcpy_s(out, out_size, frame->data[0], need);
		return need;
	}

	for (i = 0; i < frame->nb_samples; i++)
		for (ch = 0; ch < channels; ch++)
			memcpy_s(out + (((size_t)i * channels + ch) * bps),
				 out_size, frame->data[ch] + (size_t)i * bps, bps);

	return need;
}

static int ffmpeg_dec_ff_decode(struct processing_module *mod,
				const uint8_t *in, size_t in_size, size_t *consumed,
				uint8_t *out, size_t out_size, size_t *produced)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_dec_ffmpeg_data *ff = cd->backend_data;
	struct comp_dev *dev = mod->dev;
	size_t out_off = 0;
	int used;
	int ret;

	ffmpeg_dec_libc_bind(mod);

	*consumed = 0;
	*produced = 0;

	/* Parse one frame out of the raw stream. */
	used = av_parser_parse2(ff->parser, ff->avctx,
				&ff->pkt->data, &ff->pkt->size,
				in, in_size,
				AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
	if (used < 0) {
		comp_err(dev, "av_parser_parse2 failed %d", used);
		return -EIO;
	}
	*consumed = used;

	/* Parser still buffering: no complete frame yet. */
	if (ff->pkt->size == 0)
		return 0;

	ret = avcodec_send_packet(ff->avctx, ff->pkt);
	if (ret < 0) {
		comp_err(dev, "avcodec_send_packet failed %d", ret);
		return -EIO;
	}

	/* Drain all frames this packet produced. */
	while (ret >= 0) {
		ret = avcodec_receive_frame(ff->avctx, ff->frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		if (ret < 0) {
			comp_err(dev, "avcodec_receive_frame failed %d", ret);
			return -EIO;
		}

		ret = ffmpeg_dec_emit_frame(mod, ff->frame, out + out_off,
					    out_size - out_off);
		if (ret < 0) {
			comp_warn(dev, "output buffer full, PCM dropped");
			break;
		}
		out_off += ret;
		ret = 0;
	}

	*produced = out_off;
	return 0;
}

static int ffmpeg_dec_ff_reset(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_dec_ffmpeg_data *ff = cd->backend_data;

	ffmpeg_dec_libc_bind(mod);

	/*
	 * The pipeline may issue COMP_TRIGGER_RESET before prepare() has run
	 * ffmpeg_dec_ff_configure()/avcodec_open2(), so avctx->internal is still
	 * NULL. avcodec_flush_buffers() -> ff_decode_flush_buffers() dereferences
	 * that internal state unconditionally and faults (EXCCAUSE 13) on a
	 * not-yet-opened context. Only flush once the decoder is actually open.
	 */
	if (ff && ff->avctx && avcodec_is_open(ff->avctx))
		avcodec_flush_buffers(ff->avctx);

	return 0;
}

static int ffmpeg_dec_ff_free(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_dec_ffmpeg_data *ff = cd->backend_data;

	ffmpeg_dec_libc_bind(mod);

	if (!ff)
		return 0;

	if (ff->frame)
		av_frame_free(&ff->frame);
	if (ff->pkt)
		av_packet_free(&ff->pkt);
	if (ff->avctx)
		avcodec_free_context(&ff->avctx);
	if (ff->parser)
		av_parser_close(ff->parser);
	if (ff->pktbuf)
		mod_free(mod, ff->pktbuf);

	mod_free(mod, ff);
	cd->backend_data = NULL;
	return 0;
}

const struct ffmpeg_dec_backend ffmpeg_dec_backend = {
	.name = "libavcodec",
	.init = ffmpeg_dec_ff_init,
	.configure = ffmpeg_dec_ff_configure,
	.decode = ffmpeg_dec_ff_decode,
	.reset = ffmpeg_dec_ff_reset,
	.free = ffmpeg_dec_ff_free,
};
