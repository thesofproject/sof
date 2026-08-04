// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// libavcodec encode backend for the ffmpeg_dec module (encode mode): PCM in,
// compressed elementary stream out (MP3 via libshine). It is the mirror of the
// decoder path - avcodec_send_frame / avcodec_receive_packet instead of
// send_packet / receive_frame.
//
// Like the decoder this is a sink/source (DP-domain) module: PCM arrives on the
// circular source buffer, compressed MP3 frames are written to the circular
// sink buffer. RAW_DATA processing is only prepared by module_adapter in the LL
// domain, so a DP-scheduled codec (needed to keep the heavy avcodec_open2 and
// per-frame encode off the LL tick) must use the sink/source .process interface.
//
// SOF PCM is interleaved S32; the encoder is opened in whatever sample format it
// supports (libshine: S16), so S32 is converted to the encoder's format per
// frame. MP3 uses fixed 1152-sample frames; one encoder frame worth of input is
// consumed per process cycle once enough source PCM is available.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <rtos/string.h>
#include <errno.h>
#include "ffmpeg_dec.h"

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define FFMPEG_ENC_DEFAULT_BITRATE	128000

/* Linear staging for compressed output, drained into the circular sink over as
 * many cycles as needed. A 1152-sample MP3 frame is at most ~1.5 KiB even at
 * 320 kbps; 16 KiB holds several frames comfortably.
 */
#define FFMPEG_ENC_MP3_STAGE		16384

struct ffmpeg_enc_data {
	const AVCodec *codec;
	AVCodecContext *avctx;
	AVPacket *pkt;
	AVFrame *frame;
	int frame_bytes;	/* SOF S32 input frame size (all channels) */
	int nb;			/* encoder frame size in samples */
	size_t need;		/* source bytes for one encoder frame (nb*frame_bytes) */
	uint8_t *pcm_in;	/* linear S32 interleaved, one encoder frame */
	size_t pcm_fill;	/* bytes accumulated in pcm_in so far (< need) */
	uint8_t *mp3_out;	/* linear compressed staging (see FFMPEG_ENC_MP3_STAGE) */
	size_t mp3_cap;		/* capacity of mp3_out */
	size_t mp3_avail;	/* staged compressed bytes not yet drained to sink */
	size_t mp3_rd;		/* read offset of next byte to drain from mp3_out */
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
static void ffmpeg_enc_drain_mp3(struct ffmpeg_enc_data *e, struct sof_sink *sink)
{
	void *dst, *buf_start;
	size_t buf_size, n;
	int ret;

	n = ffmpeg_enc_min(e->mp3_avail, sink_get_free_size(sink));
	if (!n)
		return;

	ret = sink_get_buffer(sink, n, &dst, &buf_start, &buf_size);
	if (ret)
		return;

	ffmpeg_enc_copy_to_circular(dst, buf_start, buf_size,
				    e->mp3_out + e->mp3_rd, n);
	sink_commit_buffer(sink, n);
	e->mp3_rd += n;
	e->mp3_avail -= n;
	if (!e->mp3_avail)
		e->mp3_rd = 0;
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

	/* Size the DP output ring so it holds several compressed MP3 frames
	 * (a 1152-sample frame is at most ~1.5 KiB): bind sizes the ring
	 * between this DP module and the downstream host copier as 3x this.
	 */
	md->mpd.out_buff_size = FFMPEG_ENC_MP3_STAGE;

	e = mod_zalloc(mod, sizeof(*e));
	if (!e) {
		mod_free(mod, cd);
		return -ENOMEM;
	}

	/* libshine is the fixed-point MP3 encoder built into the archive. */
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

	/*
	 * Defer avcodec_open2 to the first process() on the module's own DP
	 * thread (see ffmpeg_enc_mod_process): prepare may run on the core's IDC
	 * worker, and the open must not stall that cross-core critical path.
	 * Keep the encoder open across reset()/re-prepare() (cd->configured),
	 * just drop any staged output from a previous run.
	 */
	if (e) {
		e->mp3_avail = 0;
		e->mp3_rd = 0;
		e->pcm_fill = 0;
	}

	/*
	 * Variable-rate MPEG encoder: one process() call encodes a whole MP3
	 * frame (~46 ms of soft-float MDCT + rate loop on this DSP), far longer
	 * than the OBS-derived auto period (~42 ms). Exceeding the DP deadline
	 * tears the pipeline down, so set an explicit period with headroom for
	 * the worst-case single-frame encode (see module_adapter_prepare, which
	 * skips the auto-calc when the module sets its own period).
	 */
	dev->period = 96000; /* us */

	comp_info(dev, "ffmpeg_enc prepare: %d src / %d sink (open deferred to DP)",
		  num_of_sources, num_of_sinks);

	return 0;
}

/* Open libshine once the source PCM format is known. Runs on the DP thread. */
static int ffmpeg_enc_configure(struct processing_module *mod, struct sof_source *src)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_enc_data *e = cd->backend_data;
	struct comp_dev *dev = mod->dev;
	int ret;

	cd->out_channels = source_get_channels(src);
	cd->out_rate = source_get_rate(src);
	e->frame_bytes = source_get_frame_bytes(src);

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

	e->nb = e->avctx->frame_size ? e->avctx->frame_size : 1152;
	e->need = (size_t)e->nb * e->frame_bytes;

	e->pcm_in = mod_alloc(mod, e->need);
	e->mp3_out = mod_alloc(mod, FFMPEG_ENC_MP3_STAGE);
	if (!e->pcm_in || !e->mp3_out) {
		comp_err(dev, "ffmpeg_enc: staging alloc failed");
		return -ENOMEM;
	}
	e->mp3_cap = FFMPEG_ENC_MP3_STAGE;
	e->mp3_avail = 0;
	e->mp3_rd = 0;
	e->pcm_fill = 0;

	comp_info(dev, "ffmpeg_enc: libshine MP3 open OK, rate %u ch %u frame_size %d need %zu",
		  cd->out_rate, cd->out_channels, e->nb, e->need);
	return 0;
}

/*
 * Gate the DP process() call. Crucially, input consumption is NOT blocked on a
 * full output sink. This encoder sits behind a SHARED HDA capture BE fan-out
 * (module-copier feeds both the normal PCM capture FE and this compress FE); if
 * we stopped draining the LL->DP input ring whenever the host sink was momentarily
 * full, the fan-out copier would back up and stall the entire capture DAI --
 * killing normal capture too. So:
 *   - one-time open (avcodec_open2) needs one cycle, touches no sink;
 *   - drain staged MP3 only when the host sink has room;
 *   - otherwise consume/accumulate input whenever any source PCM is available,
 *     independent of sink backpressure (one encoder frame e->need spans many DP
 *     cycles; the MP3 staging buffer absorbs output while the host catches up).
 */
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

	if (e->mp3_avail)
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
	if (e->mp3_avail) {
		ffmpeg_enc_drain_mp3(e, sink);
		return 0;
	}

	/* Accumulate source PCM into pcm_in until a full encoder frame is
	 * buffered. The LL->DP input ring holds only a few LL periods (far less
	 * than one 1152-sample encoder frame), so a frame must be gathered over
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

	e->frame->nb_samples = e->nb;
	e->frame->format = e->avctx->sample_fmt;
	e->frame->sample_rate = cd->out_rate;
	av_channel_layout_copy(&e->frame->ch_layout, &e->avctx->ch_layout);
	if (av_frame_get_buffer(e->frame, 0) < 0)
		return -ENOMEM;

	/* S32 interleaved -> S16 (planar or packed per the encoder format). */
	in = (const int32_t *)e->pcm_in;
	planar = av_sample_fmt_is_planar(e->frame->format);
	for (i = 0; i < e->nb; i++)
		for (c = 0; c < ch; c++) {
			int16_t s = (int16_t)(in[i * ch + c] >> 16);

			if (planar)
				((int16_t *)e->frame->data[c])[i] = s;
			else
				((int16_t *)e->frame->data[0])[i * ch + c] = s;
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
		if (e->mp3_avail + e->pkt->size <= e->mp3_cap) {
			memcpy_s(e->mp3_out + e->mp3_avail,
				 e->mp3_cap - e->mp3_avail,
				 e->pkt->data, e->pkt->size);
			e->mp3_avail += e->pkt->size;
		} else {
			comp_warn(dev, "mp3 stage full, packet dropped");
		}
		av_packet_unref(e->pkt);
		ret = 0;
	}

	/* Drain what fits now; the rest is carried to the next cycle. */
	ffmpeg_enc_drain_mp3(e, sink);
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
	if (e->pcm_in)
		mod_free(mod, e->pcm_in);
	if (e->mp3_out)
		mod_free(mod, e->mp3_out);
	mod_free(mod, e);
	cd->backend_data = NULL;
	mod_free(mod, cd);
	mod->priv.private = NULL;
	return 0;
}
