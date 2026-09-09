// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Encode backend for the ffmpeg_dec module (encode mode): PCM in,
// compressed elementary stream out (AAC-LC via vo-aacenc or MP3 via libshine).
//
// Like the decoder this is a sink/source (DP-domain) module: PCM arrives on the
// circular source buffer, compressed frames are written to the circular
// sink buffer. RAW_DATA processing is only prepared by module_adapter in the LL
// domain, so a DP-scheduled codec (needed to keep the heavy codec init and
// per-frame encode off the LL tick) must use the sink/source .process interface.
//
// SOF PCM is interleaved S32; the encoder is opened in whatever sample format it
// supports (S16 for both vo-aacenc and libshine), so S32 is converted to S16 per
// frame.
//   - AAC-LC uses 1024-sample frames (21.33 ms @ 48 kHz).
//   - MP3 uses 1152-sample frames (24.00 ms @ 48 kHz).
// One encoder frame worth of input is consumed per process cycle once enough
// source PCM has accumulated in pcm_in.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <rtos/string.h>
#include <errno.h>
#include "ffmpeg_dec.h"

#if defined(CONFIG_FFMPEG_ENC_MP3)
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#endif

#if defined(CONFIG_FFMPEG_ENC_AAC)
#include <vo-aacenc/voAAC.h>
#include <vo-aacenc/cmnMemory.h>
#endif

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define FFMPEG_ENC_DEFAULT_BITRATE	128000

/* Linear staging for compressed output, drained into the circular sink over as
 * many cycles as needed. An AAC frame at 128 kbps is ~340 bytes; MP3 is ~384 bytes.
 * 16 KiB holds dozens of frames comfortably.
 */
#define FFMPEG_ENC_STAGE_SIZE		16384

/* LL->DP input ring depth hint (bytes). bind allocates a ~3x ring; this must
 * hold one whole worst-case encode of real-time capture PCM so a blocking
 * frame encode does not overflow the input and overrun the DAI.
 */
#define FFMPEG_ENC_PCM_STAGE		16384

enum ffmpeg_enc_type {
	FFMPEG_ENC_TYPE_NONE = 0,
	FFMPEG_ENC_TYPE_MP3,
	FFMPEG_ENC_TYPE_AAC,
};

struct ffmpeg_enc_data {
	enum ffmpeg_enc_type type;
	int frame_bytes;	/* SOF S32 input frame size (all channels) */
	int nb;			/* encoder frame size in samples */
	size_t need;		/* source bytes for one encoder frame (nb*frame_bytes) */
	uint8_t *pcm_in;	/* linear S32 interleaved, one encoder frame */
	size_t pcm_fill;	/* bytes accumulated in pcm_in so far (< need) */
	uint8_t *out_stage;	/* linear compressed staging */
	size_t out_cap;		/* capacity of out_stage */
	size_t out_avail;	/* staged compressed bytes not yet drained to sink */
	size_t out_rd;		/* read offset of next byte to drain from out_stage */

#if defined(CONFIG_FFMPEG_ENC_MP3)
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
#endif

#if defined(CONFIG_FFMPEG_ENC_AAC)
	VO_AUDIO_CODECAPI aac_api;
	VO_HANDLE aac_handle;
	VO_MEM_OPERATOR aac_memop;
	int16_t *pcm_s16;
#endif
};

/* Copy @bytes out of a circular source buffer (starting at @src) into the
 * linear @dst, wrapping at the buffer end if necessary.
 */
static void ffmpeg_enc_copy_from_circular(void *dst, const void *src,
					  const void *buf_start, size_t buf_size,
					  size_t bytes)
{
	size_t to_end = (size_t)((const uint8_t *)buf_start + buf_size -
				 (const uint8_t *)src);

	if (to_end >= bytes) {
		memcpy_s(dst, bytes, src, bytes);
		return;
	}
	memcpy_s(dst, to_end, src, to_end);
	memcpy_s((uint8_t *)dst + to_end, bytes - to_end,
		 buf_start, bytes - to_end);
}

/* Copy @bytes from the linear @src into a circular sink buffer (starting at
 * @dst), wrapping at the buffer end if necessary.
 */
static void ffmpeg_enc_copy_to_circular(void *dst, const void *buf_start,
					size_t buf_size, const void *src,
					size_t bytes)
{
	size_t to_end = (size_t)((uint8_t *)buf_start + buf_size -
				 (uint8_t *)dst);

	if (to_end >= bytes) {
		memcpy_s(dst, bytes, src, bytes);
		return;
	}
	memcpy_s(dst, to_end, src, to_end);
	memcpy_s(buf_start, bytes - to_end,
		 (const uint8_t *)src + to_end, bytes - to_end);
}

static inline size_t ffmpeg_enc_min(size_t a, size_t b)
{
	return a < b ? a : b;
}

/* Drain as much staged compressed output as the sink can currently accept. */
static void ffmpeg_enc_drain(struct ffmpeg_enc_data *e, struct sof_sink *sink)
{
	void *dst, *buf_start;
	size_t buf_size, n;
	int ret;

	n = ffmpeg_enc_min(e->out_avail, sink_get_free_size(sink));
	if (!n)
		return;

	ret = sink_get_buffer(sink, n, &dst, &buf_start, &buf_size);
	if (ret)
		return;

	ffmpeg_enc_copy_to_circular(dst, buf_start, buf_size,
				    e->out_stage + e->out_rd, n);
	sink_commit_buffer(sink, n);
	e->out_rd += n;
	e->out_avail -= n;
	if (!e->out_avail)
		e->out_rd = 0;
}

int ffmpeg_enc_mod_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_dec_comp_data *cd;
	struct ffmpeg_enc_data *e;

	ffmpeg_dec_libc_bind(mod);

	/* Allocate the module private data and publish it (module_adapter does
	 * not allocate it for us; the decoder path does the same in its init).
	 */
	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;
	md->private = cd;

	/* Size the DP output ring so it holds several compressed frames
	 * (an AAC frame at 128 kbps is ~340 bytes; MP3 is ~384 bytes).
	 * bind sizes the ring between this DP module and downstream copier as 3x this.
	 */
	md->mpd.out_buff_size = FFMPEG_ENC_STAGE_SIZE;

	/* Size the LL->DP input ring deep enough to absorb one whole worst-case
	 * encode of real-time capture PCM.
	 */
	md->mpd.in_buff_size = FFMPEG_ENC_PCM_STAGE;

	e = mod_zalloc(mod, sizeof(*e));
	if (!e) {
		mod_free(mod, cd);
		return -ENOMEM;
	}

#if defined(CONFIG_FFMPEG_ENC_DEFAULT_AAC) || (defined(CONFIG_FFMPEG_ENC_AAC) && !defined(CONFIG_FFMPEG_ENC_MP3))
	e->type = FFMPEG_ENC_TYPE_AAC;
	cd->codec = FFMPEG_DEC_CODEC_AAC;
#else
	e->type = FFMPEG_ENC_TYPE_MP3;
	cd->codec = FFMPEG_DEC_CODEC_MP3;
#endif

#if defined(CONFIG_FFMPEG_ENC_AAC)
	if (e->type == FFMPEG_ENC_TYPE_AAC) {
		VO_CODEC_INIT_USERDATA user_data;
		int ret;

		voGetAACEncAPI(&e->aac_api);
		e->aac_memop.Alloc = cmnMemAlloc;
		e->aac_memop.Copy = cmnMemCopy;
		e->aac_memop.Free = cmnMemFree;
		e->aac_memop.Set = cmnMemSet;
		e->aac_memop.Check = cmnMemCheck;
		user_data.memflag = VO_IMF_USERMEMOPERATOR;
		user_data.memData = &e->aac_memop;

		ret = e->aac_api.Init(&e->aac_handle, VO_AUDIO_CodingAAC, &user_data);
		if (ret != VO_ERR_NONE) {
			comp_err(dev, "vo-aacenc Init failed %d", ret);
			mod_free(mod, e);
			mod_free(mod, cd);
			return -EIO;
		}
	}
#endif

#if defined(CONFIG_FFMPEG_ENC_MP3)
	if (e->type == FFMPEG_ENC_TYPE_MP3) {
		e->codec = avcodec_find_encoder_by_name("libshine");
		if (!e->codec) {
			comp_err(dev, "libshine MP3 encoder not found");
			mod_free(mod, e);
			mod_free(mod, cd);
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
			mod_free(mod, cd);
			return -ENOMEM;
		}
	}
#endif

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

	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "ffmpeg_enc: need 1 source/1 sink, got %d/%d",
			 num_of_sources, num_of_sinks);
		return -EINVAL;
	}

	ffmpeg_dec_libc_bind(mod);

	if (e) {
		e->out_avail = 0;
		e->out_rd = 0;
		e->pcm_fill = 0;
	}

	dev->period = 96000; /* us */

	comp_info(dev, "ffmpeg_enc prepare: %d src / %d sink (open deferred to DP)",
		  num_of_sources, num_of_sinks);

	return 0;
}

/* Open encoder once the source PCM format is known. Runs on the DP thread. */
static int ffmpeg_enc_configure(struct processing_module *mod, struct sof_source *src)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;
	struct comp_dev *dev = mod->dev;
	int ret;

	cd->out_channels = source_get_channels(src);
	cd->out_rate = source_get_rate(src);
	e->frame_bytes = source_get_frame_bytes(src);

#if defined(CONFIG_FFMPEG_ENC_AAC)
	if (e->type == FFMPEG_ENC_TYPE_AAC) {
		AACENC_PARAM params = { 0 };
		params.sampleRate = cd->out_rate;
		params.bitRate = FFMPEG_ENC_DEFAULT_BITRATE;
		params.nChannels = cd->out_channels;
		params.adtsUsed = 1;
		ret = e->aac_api.SetParam(e->aac_handle, VO_PID_AAC_ENCPARAM, &params);
		if (ret != VO_ERR_NONE) {
			comp_err(dev, "vo-aacenc SetParam failed %d", ret);
			return -EINVAL;
		}

		e->nb = 1024;
		e->need = (size_t)e->nb * e->frame_bytes;
		e->pcm_in = mod_alloc(mod, e->need);
		e->pcm_s16 = mod_alloc(mod, (size_t)e->nb * cd->out_channels * sizeof(int16_t));
		e->out_stage = mod_alloc(mod, FFMPEG_ENC_STAGE_SIZE);
		if (!e->pcm_in || !e->pcm_s16 || !e->out_stage) {
			comp_err(dev, "ffmpeg_enc: AAC staging alloc failed");
			return -ENOMEM;
		}
		e->out_cap = FFMPEG_ENC_STAGE_SIZE;
		e->out_avail = 0;
		e->out_rd = 0;
		e->pcm_fill = 0;

		comp_info(dev, "ffmpeg_enc: vo-aacenc AAC open OK, rate %u ch %u frame_size %d need %zu",
			  cd->out_rate, cd->out_channels, e->nb, e->need);
		return 0;
	}
#endif

#if defined(CONFIG_FFMPEG_ENC_MP3)
	if (e->type == FFMPEG_ENC_TYPE_MP3) {
		e->avctx->sample_rate = cd->out_rate;
		av_channel_layout_default(&e->avctx->ch_layout, cd->out_channels);
		e->avctx->sample_fmt = e->codec->sample_fmts ?
				       e->codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
		e->avctx->bit_rate = FFMPEG_ENC_DEFAULT_BITRATE;
		e->avctx->thread_count = 1;

		ret = avcodec_open2(e->avctx, e->codec, NULL);
		if (ret < 0) {
			comp_err(dev, "avcodec_open2 (encoder) failed %d", ret);
			return -EIO;
		}

		e->nb = e->avctx->frame_size ? e->avctx->frame_size : 1152;
		e->need = (size_t)e->nb * e->frame_bytes;

		e->pcm_in = mod_alloc(mod, e->need);
		e->out_stage = mod_alloc(mod, FFMPEG_ENC_STAGE_SIZE);
		if (!e->pcm_in || !e->out_stage) {
			comp_err(dev, "ffmpeg_enc: staging alloc failed");
			return -ENOMEM;
		}
		e->out_cap = FFMPEG_ENC_STAGE_SIZE;
		e->out_avail = 0;
		e->out_rd = 0;
		e->pcm_fill = 0;

		comp_info(dev, "ffmpeg_enc: libshine MP3 open OK, rate %u ch %u frame_size %d need %zu",
			  cd->out_rate, cd->out_channels, e->nb, e->need);
		return 0;
	}
#endif

	return -EINVAL;
}

bool ffmpeg_enc_is_ready_to_process(struct processing_module *mod,
				    struct sof_source **sources, int num_of_sources,
				    struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;

	if (num_of_sources < 1 || num_of_sinks < 1)
		return false;

	if (!cd->configured)
		return true;

	if (e->out_avail)
		return sink_get_free_size(sinks[0]) > 0;

	return source_get_data_available(sources[0]) > 0;
}

/* Convert one interleaved S32 frame block to the encoder AVFrame, then encode. */
int ffmpeg_enc_mod_process(struct processing_module *mod,
			   struct sof_source **sources, int num_of_sources,
			   struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;
	struct comp_dev *dev = mod->dev;
	struct sof_source *src;
	struct sof_sink *sink;
	const void *sp, *sstart;
	size_t sbytes;
	const int32_t *in;
	int ch = cd->out_channels;
	int planar, i, c, ret;

	if (num_of_sources < 1 || num_of_sinks < 1)
		return -EINVAL;

	src = sources[0];
	sink = sinks[0];

	ffmpeg_dec_libc_bind(mod);

	/* One-time lazy open on the DP thread; encode on a later cycle. */
	if (!cd->configured) {
		ret = ffmpeg_enc_configure(mod, src);
		if (ret)
			return ret;
		cd->configured = true;
		return 0;
	}
	ch = cd->out_channels;
	/* Drain leftover compressed output before consuming more input. */
	if (e->out_avail) {
		ffmpeg_enc_drain(e, sink);
		return 0;
	}

	/* Accumulate source PCM into pcm_in until a full encoder frame is
	 * buffered. The LL->DP input ring holds only a few LL periods (far less
	 * than one encoder frame), so a frame must be gathered over
	 * several DP cycles rather than read in a single source_get_data() call.
	 */
	{
		size_t avail = source_get_data_available(src);
		size_t want = e->need - e->pcm_fill;
		size_t n = ffmpeg_enc_min(want, avail);

		if (n) {
			ret = source_get_data(src, n, &sp, &sstart, &sbytes);
			if (ret)
				return 0;
			ffmpeg_enc_copy_from_circular(e->pcm_in + e->pcm_fill, sp,
						      sstart, sbytes, n);
			source_release_data(src, n);
			e->pcm_fill += n;
		}
	}

	/* Not a full encoder frame yet - wait for more input next cycle. */
	if (e->pcm_fill < e->need)
		return 0;
	e->pcm_fill = 0;

#if defined(CONFIG_FFMPEG_ENC_AAC)
	if (e->type == FFMPEG_ENC_TYPE_AAC) {
		VO_CODECBUFFER input = { 0 }, output = { 0 };
		VO_AUDIO_OUTPUTINFO output_info = { 0 };
		uint8_t outbuf[2048];
		size_t total_samples = (size_t)e->nb * ch;

		in = (const int32_t *)e->pcm_in;
		for (i = 0; i < total_samples; i++)
			e->pcm_s16[i] = (int16_t)(in[i] >> 16);

		input.Buffer = (uint8_t *)e->pcm_s16;
		input.Length = total_samples * sizeof(int16_t);
		e->aac_api.SetInputData(e->aac_handle, &input);

		output.Buffer = outbuf;
		output.Length = sizeof(outbuf);
		ret = e->aac_api.GetOutputData(e->aac_handle, &output, &output_info);
		if (ret != VO_ERR_NONE) {
			comp_err(dev, "vo-aacenc GetOutputData failed %d", ret);
			return -EIO;
		}

		if (output.Length > 0) {
			if (e->out_avail + output.Length <= e->out_cap) {
				memcpy_s(e->out_stage + e->out_avail,
					 e->out_cap - e->out_avail,
					 outbuf, output.Length);
				e->out_avail += output.Length;
			} else {
				comp_warn(dev, "AAC stage full, packet dropped");
			}
		}

		ffmpeg_enc_drain(e, sink);
		return 0;
	}
#endif

#if defined(CONFIG_FFMPEG_ENC_MP3)
	if (e->type == FFMPEG_ENC_TYPE_MP3) {
		int planar, c;

		e->frame->nb_samples = e->nb;
		e->frame->format = e->avctx->sample_fmt;
		e->frame->sample_rate = cd->out_rate;
		av_channel_layout_copy(&e->frame->ch_layout, &e->avctx->ch_layout);
		if (av_frame_get_buffer(e->frame, 0) < 0)
			return -ENOMEM;

		/* S32 interleaved -> S16 (planar or packed per the encoder format). */
		in = (const int32_t *)e->pcm_in;
		planar = av_sample_fmt_is_planar(e->frame->format);
		if (planar && ch == 2) {
			int16_t *out0 = (int16_t *)e->frame->data[0];
			int16_t *out1 = (int16_t *)e->frame->data[1];
			for (i = 0; i + 4 <= e->nb; i += 4) {
				out0[i + 0] = (int16_t)(in[0] >> 16);
				out1[i + 0] = (int16_t)(in[1] >> 16);
				out0[i + 1] = (int16_t)(in[2] >> 16);
				out1[i + 1] = (int16_t)(in[3] >> 16);
				out0[i + 2] = (int16_t)(in[4] >> 16);
				out1[i + 2] = (int16_t)(in[5] >> 16);
				out0[i + 3] = (int16_t)(in[6] >> 16);
				out1[i + 3] = (int16_t)(in[7] >> 16);
				in += 8;
			}
			for (; i < e->nb; i++) {
				out0[i] = (int16_t)(in[0] >> 16);
				out1[i] = (int16_t)(in[1] >> 16);
				in += 2;
			}
		} else if (planar) {
			for (c = 0; c < ch; c++) {
				int16_t *out = (int16_t *)e->frame->data[c];
				for (i = 0; i < e->nb; i++)
					out[i] = (int16_t)(in[i * ch + c] >> 16);
			}
		} else {
			int16_t *out = (int16_t *)e->frame->data[0];
			size_t total = (size_t)e->nb * ch;
			for (i = 0; i + 4 <= total; i += 4) {
				out[i + 0] = (int16_t)(in[i + 0] >> 16);
				out[i + 1] = (int16_t)(in[i + 1] >> 16);
				out[i + 2] = (int16_t)(in[i + 2] >> 16);
				out[i + 3] = (int16_t)(in[i + 3] >> 16);
			}
			for (; i < total; i++)
				out[i] = (int16_t)(in[i] >> 16);
		}

		ret = avcodec_send_frame(e->avctx, e->frame);
		av_frame_unref(e->frame);
		if (ret < 0) {
			comp_err(dev, "avcodec_send_frame failed %d", ret);
			return -EIO;
		}

		/* Stage compressed packets; they are drained into the sink over the
		 * following cycles (starting with the one after this returns).
		 */
		while (ret >= 0) {
			ret = avcodec_receive_packet(e->avctx, e->pkt);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;
			if (ret < 0) {
				comp_err(dev, "avcodec_receive_packet failed %d", ret);
				return -EIO;
			}
			if (e->out_avail + e->pkt->size <= e->out_cap) {
				memcpy_s(e->out_stage + e->out_avail,
					 e->out_cap - e->out_avail,
					 e->pkt->data, e->pkt->size);
				e->out_avail += e->pkt->size;
			} else {
				comp_warn(dev, "mp3 stage full, packet dropped");
			}
			av_packet_unref(e->pkt);
			ret = 0;
		}

		/* Drain what fits now; the rest is carried to the next cycle. */
		ffmpeg_enc_drain(e, sink);
		return 0;
	}
#endif

	return 0;
}

int ffmpeg_enc_mod_reset(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;

	ffmpeg_dec_libc_bind(mod);
	if (e) {
		e->out_avail = 0;
		e->out_rd = 0;
		e->pcm_fill = 0;
	}
	cd->configured = false;
	return 0;
}

int ffmpeg_enc_mod_free(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;

	ffmpeg_dec_libc_bind(mod);
	if (!e)
		return 0;

#if defined(CONFIG_FFMPEG_ENC_AAC)
	if (e->type == FFMPEG_ENC_TYPE_AAC) {
		if (e->aac_handle)
			e->aac_api.Uninit(e->aac_handle);
		if (e->pcm_s16)
			mod_free(mod, e->pcm_s16);
	}
#endif

#if defined(CONFIG_FFMPEG_ENC_MP3)
	if (e->type == FFMPEG_ENC_TYPE_MP3) {
		if (e->frame)
			av_frame_free(&e->frame);
		if (e->pkt)
			av_packet_free(&e->pkt);
		if (e->avctx)
			avcodec_free_context(&e->avctx);
	}
#endif

	if (e->pcm_in)
		mod_free(mod, e->pcm_in);
	if (e->out_stage)
		mod_free(mod, e->out_stage);
	mod_free(mod, e);
	cd->backend_data = NULL;
	mod_free(mod, cd);
	mod->priv.private = NULL;
	return 0;
}
