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
#include "ffmpeg_dec-convert.h"

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

	if (level <= AV_LOG_ERROR)
		LOG_ERR("ffmpeg: %s", ffmpeg_dec_log_line);
	else if (level <= AV_LOG_WARNING)
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
	size_t pktbuf_sz;
	size_t pktbuf_len;
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
#if defined(CONFIG_FFMPEG_DEC_ALAC)
	case FFMPEG_DEC_CODEC_ALAC:
		return AV_CODEC_ID_ALAC;
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
	if (cd->codec_ops && cd->codec_ops->decoder_name)
		ff->codec = avcodec_find_decoder_by_name(cd->codec_ops->decoder_name);
	if (!ff->codec)
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

	if (cd->codec_ops && cd->codec_ops->prepare_extradata)
		cd->codec_ops->prepare_extradata(mod);

	if (cd->codec_ops && cd->codec_ops->max_frame_samples && cd->out_frame_bytes) {
		size_t need = (size_t)cd->codec_ops->max_frame_samples(cd) * cd->out_frame_bytes;

		if (cd->pcm_buf_size < need)
			return -ENOMEM;
	}

	if (cd->codec == FFMPEG_DEC_CODEC_ALAC) {
		uint32_t max_ch = cd->out_channels ? cd->out_channels : 2;
		uint32_t cookie_samples = 0;
		uint8_t cookie_bits = 0;
		uint8_t cookie_ch = 0;

		if (cd->extradata_size >= 22 &&
		    memcmp(&cd->extradata[4], "alac", 4) == 0) {
			cookie_samples = ((uint32_t)cd->extradata[12] << 24) |
					 ((uint32_t)cd->extradata[13] << 16) |
					 ((uint32_t)cd->extradata[14] << 8) |
					 (uint32_t)cd->extradata[15];
			cookie_bits = cd->extradata[17];
			cookie_ch = cd->extradata[21];
		}
		if (cookie_samples == 0 || cookie_samples > 16384 ||
		    cookie_bits == 0 || cookie_bits > 32 ||
		    cookie_ch == 0 || cookie_ch > max_ch) {
			comp_err(dev, "unsupported ALAC cookie (samples=%u bits=%u ch=%u)",
				 cookie_samples, cookie_bits, cookie_ch);
			return -EINVAL;
		}
	}

	if (cd->extradata_size) {
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
	comp_info(dev, "libavcodec decoder opened (%s)", ff->codec->name);

	/*
	 * Padded scratch for a parsed or reassembled packet payload, sized to
	 * hold a worst-case uncompressed frame (up to 4 bytes/sample/ch) plus
	 * framing overhead and AV_INPUT_BUFFER_PADDING_SIZE.
	 * Route through av_malloc (the sys_heap-backed FFmpeg heap, see
	 * ffmpeg_dec-builtin-alloc.c) rather than mod_alloc: the SOF per-module
	 * heap is far too small for this tens-of-KiB scratch.
	 */
	{
		uint32_t max_samples = (cd->codec_ops && cd->codec_ops->max_frame_samples) ?
				       cd->codec_ops->max_frame_samples(cd) : 4096;
		size_t max_ch = cd->out_channels ? cd->out_channels : 2;
		size_t frame_bytes = MAX((size_t)cd->out_frame_bytes,
					 max_ch * sizeof(int32_t));

		ff->pktbuf_sz = frame_bytes * (size_t)max_samples;
		if (ff->pktbuf_sz < 65536)
			ff->pktbuf_sz = 65536;
		ff->pktbuf_sz += 1024 + AV_INPUT_BUFFER_PADDING_SIZE;
	}
	ff->pktbuf = av_malloc(ff->pktbuf_sz);
	if (!ff->pktbuf)
		return -ENOMEM;
	ff->pktbuf_len = 0;

	return 0;
}

static int ffmpeg_dec_emit_frame(struct processing_module *mod, AVFrame *frame,
				 uint8_t *out, size_t out_size)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	return ffmpeg_dec_convert_frame(frame, out, out_size,
					cd->out_frame_fmt, cd->out_channels);
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

	/*
	 * Unparsed decode path for self-delimiting codecs without an
	 * AVCodecParser (e.g. ALAC, where alac_decode_frame reads bit-by-bit
	 * up to TYPE_END and reports the exact bit count in gb.index).
	 */
	if (!ff->parser) {
		size_t prev_len = ff->pktbuf_len;
		size_t cap = ff->pktbuf_sz > AV_INPUT_BUFFER_PADDING_SIZE ?
			     ff->pktbuf_sz - AV_INPUT_BUFFER_PADDING_SIZE : 0;
		const uint8_t *pkt_data;
		size_t pkt_len;

		if (cd->alac_skip_to_eof) {
			*consumed = in_size;
			return 0;
		}

		if (!in || !in_size) {
			if (!prev_len)
				return 0;
			pkt_data = ff->pktbuf;
			pkt_len = prev_len;
		} else if (prev_len == 0) {
			pkt_data = in;
			pkt_len = in_size;
		} else {
			size_t space = prev_len < cap ? cap - prev_len : 0;
			size_t copy = in_size < space ? in_size : space;

			if (copy) {
				memcpy_s(ff->pktbuf + prev_len, space, in, copy);
				memset(ff->pktbuf + prev_len + copy, 0,
				       AV_INPUT_BUFFER_PADDING_SIZE);
			}
			pkt_data = ff->pktbuf;
			pkt_len = prev_len + copy;
		}

		/* Stop cleanly if a trailing CAF chunk ("pakt" or "free") is reached */
		if (cd->codec == FFMPEG_DEC_CODEC_ALAC && pkt_len >= 4 &&
		    (memcmp(pkt_data, "pakt", 4) == 0 ||
		     memcmp(pkt_data, "free", 4) == 0)) {
			cd->alac_skip_to_eof = true;
			ff->pktbuf_len = 0;
			*consumed = in_size;
			return 0;
		}

		ff->pkt->data = (uint8_t *)pkt_data;
		ff->pkt->size = (int)pkt_len;

		av_log_set_level(AV_LOG_FATAL);
		ret = avcodec_send_packet(ff->avctx, ff->pkt);
		if (ret == 0)
			ret = avcodec_receive_frame(ff->avctx, ff->frame);
		av_log_set_level(AV_LOG_ERROR);
		if (ret < 0) {
			avcodec_flush_buffers(ff->avctx);
			if (!in || !in_size) {
				ff->pktbuf_len = 0;
				return 0;
			}
			if (pkt_len >= cap) {
				comp_err(dev, "unparsed packet reassembly buffer full without valid frame");
				ff->pktbuf_len = 0;
				return -EIO;
			}
			if (prev_len == 0) {
				size_t copy = in_size < cap ? in_size : cap;

				memcpy_s(ff->pktbuf, cap, in, copy);
				memset(ff->pktbuf + copy, 0,
				       AV_INPUT_BUFFER_PADDING_SIZE);
				ff->pktbuf_len = copy;
				*consumed = copy;
			} else {
				size_t space = prev_len < cap ? cap - prev_len : 0;
				size_t copy = in_size < space ? in_size : space;

				ff->pktbuf_len = prev_len + copy;
				*consumed = copy;
			}
			return 0;
		}

		size_t fbytes = pkt_len;

		if (cd->codec_ops && cd->codec_ops->frame_bytes_consumed) {
			size_t b = cd->codec_ops->frame_bytes_consumed(ff->avctx->priv_data);

			if (b > 0 && b <= pkt_len)
				fbytes = b;
		}
		*consumed = (fbytes > prev_len) ? (fbytes - prev_len) : 0;
		ff->pktbuf_len = 0;

		ret = ffmpeg_dec_emit_frame(mod, ff->frame, out, out_size);
		if (ret < 0) {
			comp_warn(dev, "output buffer full, PCM dropped");
			return 0;
		}
		*produced = (size_t)ret;
		return 0;
	}

	if (!in || !in_size) {
		/* Flush parser first */
		used = av_parser_parse2(ff->parser, ff->avctx,
					&ff->pkt->data, &ff->pkt->size,
					NULL, 0,
					AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
		if (ff->pkt->size > 0) {
			ret = avcodec_send_packet(ff->avctx, ff->pkt);
		} else {
			/* No packet from parser: send flush packet (NULL) to decoder */
			ret = avcodec_send_packet(ff->avctx, NULL);
		}
	} else {
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

		/*
		 * Zero-copy fast path: FFmpeg decoders read AV_INPUT_BUFFER_PADDING_SIZE bytes
		 * past the packet payload. If the packet is already padded, avoid copying:
		 *   1. ff->pkt->data is in ff->parser->buffer: FFmpeg allocates with padding.
		 *   2. ff->pkt->data ends at in + in_size: ffmpeg_dec_process() zeroed
		 *      FFMPEG_DEC_INPUT_PADDING bytes immediately at that offset.
		 * Only copy to ff->pktbuf if the packet is in the interior of in_buf.
		 */
		{
			bool direct_packet = false;

			if (ff->pkt->data < in || ff->pkt->data >= in + in_size) {
				direct_packet = true;
			} else if (ff->pkt->data + ff->pkt->size == in + in_size) {
				direct_packet = true;
			}

			if (!direct_packet) {
				size_t pktbuf_sz = cd->out_frame_bytes ?
					(size_t)cd->out_frame_bytes * 4096 : 65536;

				if ((size_t)ff->pkt->size + AV_INPUT_BUFFER_PADDING_SIZE <= pktbuf_sz) {
					memcpy_s(ff->pktbuf, pktbuf_sz, ff->pkt->data, ff->pkt->size);
					memset(ff->pktbuf + ff->pkt->size, 0,
					       AV_INPUT_BUFFER_PADDING_SIZE);
					ff->pkt->data = ff->pktbuf;
				}
			}
		}

		ret = avcodec_send_packet(ff->avctx, ff->pkt);
	}

	if (ret < 0 && ret != AVERROR_EOF) {
		comp_err(dev, "avcodec_send_packet failed %d", ret);
		return -EIO;
	}

	/* Drain all frames this packet produced. */
	while (1) {
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

	if (ff)
		ff->pktbuf_len = 0;

	/*
	 * Re-initialize the parser to clear any partial frame bytes buffered
	 * from the previous stream.
	 */
	if (ff && ff->codec && ff->parser) {
		av_parser_close(ff->parser);
		ff->parser = av_parser_init(ff->codec->id);
		if (!ff->parser)
			return -ENOMEM;
	}

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
		av_free(ff->pktbuf);

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
