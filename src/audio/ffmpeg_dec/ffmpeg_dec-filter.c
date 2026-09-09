// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// libavfilter graph backend for the ffmpeg_dec module: runs an FFmpeg audio
// filter (e.g. afftdn FFT noise reduction) over PCM. Unlike the decoder path
// (send_packet/receive_frame), filters use the avfilter graph API:
//
//     abuffer(src) -> <filter> -> abuffersink(sink)
//
// PCM AVFrames are pushed into the source, pulled filtered from the sink. This
// is a PCM->PCM effect path, distinct from the compressed->PCM decode path; it
// is only built when a filter is selected (CONFIG_FFMPEG_FILTER_*), which pulls
// libavfilter into the module.

#include <sof/audio/module_adapter/module/generic.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdio.h>
#include "ffmpeg_dec.h"
#if CONFIG_IPC_MAJOR_4
#include <sof/ipc/msg.h>
#endif
#include <zephyr/sys/printk.h>

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#ifndef CONFIG_FFMPEG_AF_FILTER_NAME
#if defined(CONFIG_FFMPEG_FILTER_DEFAULT_ALIMITER) || (defined(CONFIG_FFMPEG_FILTER_ALIMITER) && !defined(CONFIG_FFMPEG_FILTER_AFFTDN))
#define CONFIG_FFMPEG_AF_FILTER_NAME	"alimiter"
#else
#define CONFIG_FFMPEG_AF_FILTER_NAME	"afftdn"
#endif
#endif

#ifndef CONFIG_FFMPEG_AF_FILTER_ARGS
#if defined(CONFIG_FFMPEG_FILTER_DEFAULT_ALIMITER) || (defined(CONFIG_FFMPEG_FILTER_ALIMITER) && !defined(CONFIG_FFMPEG_FILTER_AFFTDN))
#define CONFIG_FFMPEG_AF_FILTER_ARGS	"limit=0.95:attack=5:release=50:level=1"
#else
#define CONFIG_FFMPEG_AF_FILTER_ARGS	"nr=28:nf=-20"
#endif
#endif


/**
 * struct ffmpeg_af_graph - one audio filter graph instance.
 * @graph: The avfilter graph.
 * @src:   abuffer source context (frames pushed here).
 * @sink:  abuffersink context (filtered frames pulled here).
 * @in_frame: Persistent pre-allocated input AVFrame.
 * @out_frame: Persistent pre-allocated output AVFrame.
 * @frame_size: Number of samples per filter advance block (rate / 80).
 * @channels: Channel count.
 * @rate: Sample rate (Hz).
 * @frame_bytes: Bytes per multichannel audio frame (channels * 4).
 * @pcm_in: Linear staging buffer for input interleaved S32 PCM.
 * @pcm_in_count: Current number of audio frames in pcm_in.
 * @pcm_in_cap: Maximum capacity of pcm_in in audio frames.
 * @pcm_out: Linear staging FIFO for filtered interleaved S32 PCM.
 * @pcm_out_count: Current number of audio frames in pcm_out.
 * @pcm_out_rd: Read offset in audio frames for draining pcm_out.
 * @pcm_out_cap: Maximum capacity of pcm_out in audio frames.
 */
struct ffmpeg_af_graph {
	AVFilterGraph *graph;
	AVFilterContext *src;
	AVFilterContext *sink;
	AVFrame *in_frame;
	AVFrame *out_frame;
	int frame_size;
	int channels;
	int rate;
	int in_frame_bytes;
	int out_frame_bytes;

	int16_t *pcm_in;
	size_t pcm_in_count;
	size_t pcm_in_cap;

	uint8_t *pcm_out;
	size_t pcm_out_count;
	size_t pcm_out_rd;
	size_t pcm_out_cap;
};

void ffmpeg_af_close(struct ffmpeg_af_graph *g)
{
	if (g->in_frame)
		av_frame_free(&g->in_frame);
	if (g->out_frame)
		av_frame_free(&g->out_frame);
	if (g->pcm_in) {
		av_freep(&g->pcm_in);
	}
	if (g->pcm_out) {
		av_freep(&g->pcm_out);
	}
	if (g->graph)
		avfilter_graph_free(&g->graph);
}

static void ffmpeg_af_quiet_log(void *avcl, int level, const char *fmt, va_list vl)
{
	(void)avcl;
	(void)level;
	vprintk(fmt, vl);
}

/**
 * ffmpeg_af_open() - Build a src -> <filter> -> sink graph for the given PCM
 * format.
 * @g:        Graph to initialise.
 * @filter:   Filter name, e.g. "afftdn".
 * @rate:     Sample rate (Hz).
 * @channels: Channel count.
 * @fmt:      Interleaved AV sample format (e.g. AV_SAMPLE_FMT_FLTP).
 *
 * Return: Zero on success, negative errno otherwise.
 */
int ffmpeg_af_open(struct ffmpeg_af_graph *g, const char *filter,
		   enum AVSampleFormat fmt)
{
	const AVFilter *abuffer = avfilter_get_by_name("abuffer");
	const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
	const AVFilter *filt = avfilter_get_by_name(filter);
	AVFilterContext *filt_ctx = NULL;
	AVChannelLayout ch_layout;
	char layout_str[64];
	AVRational tb;
	int ret;

	if (!g->rate)
		g->rate = 48000;
	if (!g->channels)
		g->channels = 2;
	if (g->in_frame_bytes <= 0 || g->in_frame_bytes > 32)
		g->in_frame_bytes = g->channels * sizeof(int16_t);
	if (g->out_frame_bytes <= 0 || g->out_frame_bytes > 32)
		g->out_frame_bytes = g->channels * sizeof(int16_t);
	g->frame_size = g->rate / 80;
	if (g->frame_size <= 0)
		g->frame_size = 600;

	printk("[ffmpeg_af] open: g=%p filt=%s rate=%d ch=%d in_bytes=%d out_bytes=%d frame_size=%d\n",
	       g, filter, g->rate, g->channels, g->in_frame_bytes, g->out_frame_bytes, g->frame_size);
	av_log_set_callback(ffmpeg_af_quiet_log);
	av_log_set_level(AV_LOG_DEBUG);

	ffmpeg_af_close(g);
	g->graph = avfilter_graph_alloc();
	if (!g->graph) {
		printk("[ffmpeg_af] open: alloc graph failed ENOMEM\n");
		ret = -ENOMEM;
		goto err;
	}
	g->src = NULL;
	g->sink = NULL;
	g->in_frame = NULL;
	g->out_frame = NULL;
	g->pcm_in = NULL;
	g->pcm_out = NULL;
	g->pcm_in_count = 0;
	g->pcm_out_count = 0;
	g->pcm_out_rd = 0;

	g->src = avfilter_graph_alloc_filter(g->graph, abuffer, "in");
	if (!g->src) {
		printk("[ffmpeg_af] open: alloc src filter failed ENOMEM\n");
		ret = -ENOMEM;
		goto err;
	}

	tb.num = 1;
	tb.den = g->rate;
	av_opt_set_q(g->src, "time_base", tb, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(g->src, "sample_rate", g->rate, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_sample_fmt(g->src, "sample_fmt", fmt, AV_OPT_SEARCH_CHILDREN);
	av_channel_layout_default(&ch_layout, g->channels);
	av_channel_layout_describe(&ch_layout, layout_str, sizeof(layout_str));
	av_opt_set(g->src, "channel_layout", layout_str, AV_OPT_SEARCH_CHILDREN);

	ret = avfilter_init_dict(g->src, NULL);
	if (ret < 0) {
		printk("[ffmpeg_af] open: abuffer init failed %d\n", ret);
		LOG_ERR("ffmpeg_af_open: abuffer init failed %d", ret);
		av_channel_layout_uninit(&ch_layout);
		goto err;
	}

	printk("[ffmpeg_af] open: creating filt %s with args: %s\n",
	       filter, CONFIG_FFMPEG_AF_FILTER_ARGS);
	ret = avfilter_graph_create_filter(&filt_ctx, filt, "filter",
					   CONFIG_FFMPEG_AF_FILTER_ARGS, NULL, g->graph);
	if (ret < 0) {
		printk("[ffmpeg_af] open: create filt failed %d\n", ret);
		LOG_ERR("ffmpeg_af_open: create filt failed %d", ret);
		av_channel_layout_uninit(&ch_layout);
		goto err;
	}

	ret = avfilter_graph_create_filter(&g->sink, abuffersink, "out", NULL, NULL, g->graph);
	if (ret < 0) {
		printk("[ffmpeg_af] open: create sink failed %d\n", ret);
		LOG_ERR("ffmpeg_af_open: create sink failed %d", ret);
		av_channel_layout_uninit(&ch_layout);
		goto err;
	}
	av_opt_set(g->sink, "ch_layouts", layout_str, AV_OPT_SEARCH_CHILDREN);
	av_channel_layout_uninit(&ch_layout);

	ret = avfilter_link(g->src, 0, filt_ctx, 0);
	if (!ret)
		ret = avfilter_link(filt_ctx, 0, g->sink, 0);
	if (ret < 0) {
		printk("[ffmpeg_af] open: link failed %d\n", ret);
		LOG_ERR("ffmpeg_af_open: link failed %d", ret);
		goto err;
	}

	printk("[ffmpeg_af] open: calling avfilter_graph_config...\n");
	ret = avfilter_graph_config(g->graph, NULL);
	printk("[ffmpeg_af] open: avfilter_graph_config ret=%d\n", ret);
	if (ret < 0) {
		LOG_ERR("ffmpeg_af_open: graph_config failed %d", ret);
		goto err;
	}

	/* Allocate persistent input and output AVFrames once. */
	g->in_frame = av_frame_alloc();
	g->out_frame = av_frame_alloc();
	if (!g->in_frame || !g->out_frame) {
		printk("[ffmpeg_af] open: av_frame_alloc failed\n");
		ret = -ENOMEM;
		goto err;
	}

	g->in_frame->format = fmt;
	g->in_frame->nb_samples = g->frame_size;
	g->in_frame->sample_rate = g->rate;
	av_channel_layout_default(&g->in_frame->ch_layout, g->channels);
	ret = av_frame_get_buffer(g->in_frame, 0);
	printk("[ffmpeg_af] open: av_frame_get_buffer ret=%d\n", ret);
	if (ret < 0) {
		LOG_ERR("ffmpeg_af_open: get_buffer failed %d", ret);
		goto err;
	}

	/* Allocate persistent staging buffers for 4 blocks to buffer latency. */
	g->pcm_in_cap = g->frame_size * 4;
	g->pcm_out_cap = g->frame_size * 4;
	g->pcm_in = av_calloc(g->pcm_in_cap * g->channels, sizeof(int16_t));
	g->pcm_out = av_calloc(g->pcm_out_cap, g->out_frame_bytes);
	if (!g->pcm_in || !g->pcm_out) {
		printk("[ffmpeg_af] open: av_calloc pcm buffers failed ENOMEM\n");
		ret = -ENOMEM;
		goto err;
	}

	g->pcm_in_count = 0;
	g->pcm_out_count = 0;
	g->pcm_out_rd = 0;

	printk("[ffmpeg_af] open: success frame_size=%d in_cap=%zu out_cap=%zu\n",
	       g->frame_size, g->pcm_in_cap, g->pcm_out_cap);
	return 0;

err:
	ffmpeg_af_close(g);
	return ret;
}

/**
 * ffmpeg_af_filter() - Push one PCM frame through the graph, pull the result.
 * @g:   Graph.
 * @in:  Input PCM frame.
 * @out: Output frame to receive the filtered PCM.
 *
 * Return: 0 on a produced frame, -EAGAIN if the filter needs more input,
 *         negative errno on error.
 */
int ffmpeg_af_filter(struct ffmpeg_af_graph *g, AVFrame *in, AVFrame *out)
{
	int ret = av_buffersrc_add_frame_flags(g->src, in, AV_BUFFERSRC_FLAG_KEEP_REF);

	if (ret < 0)
		return -EIO;

	ret = av_buffersink_get_frame(g->sink, out);
	if (ret == AVERROR(EAGAIN))
		return -EAGAIN;
	if (ret < 0)
		return -EIO;

	return 0;
}

#if CONFIG_FFMPEG_DEC_FILTER_MODE
/*
 * Filter-mode SOF module: a PCM source->sink effect that runs the graph
 * (default afftdn). SOF PCM is interleaved S32; afftdn works on float planar
 * (FLTP), so we deinterleave+normalize S32 -> float on the way in and the
 * reverse on the way out. Bounded chunk per cycle.
 *
 * NOTE: afftdn has internal latency/framing, so produced samples per cycle may
 * differ from consumed; the structure below drives the graph correctly but
 * real-time latency/underrun tuning is left for on-hardware bring-up.
 */

#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <sof/math/numbers.h>

/*
 * On HiFi4/HiFi5 VFPU cores our LLVM Xtensa clang provides packed int<->float
 * conversion intrinsics (float.sx2 / trunc.sx2), each converting two lanes per
 * op. float.sx2(x, 31) yields (float)x * 2^-31 (fusing the /2^31 normalize) and
 * trunc.sx2(f, 31) yields (int32_t)(f * 2^31) with int32 saturation (replacing
 * the manual clamp). Fall back to scalar C on GCC and on non-VFPU cores.
 */
#if defined(__XTENSA__) && defined(__has_include)
#if __has_include(<xtensa/config/core-isa.h>)
#include <xtensa/config/core-isa.h>
#endif
#if defined(__has_builtin) && __has_builtin(__builtin_xtensa_float_sx2) && \
	defined(XCHAL_HAVE_HIFI4_VFPU) && XCHAL_HAVE_HIFI4_VFPU && \
	__has_include(<xtensahifiintrin.h>)
#include <xtensahifiintrin.h>
#define FFMPEG_AF_VFPU_CONV 1
#endif
#endif

#define FFMPEG_AF_MAX_CHUNK	4096
#define FFMPEG_AF_S32_SCALE	2147483648.0f	/* 2^31 */

static inline int32_t ffmpeg_af_flt_to_s32_sample(float val)
{
	float scaled = val * FFMPEG_AF_S32_SCALE;

	if (scaled >= 2147483647.0f)
		return 2147483647;
	if (scaled <= -2147483648.0f)
		return (int32_t)-2147483648LL;
	return (int32_t)scaled;
}

static void ffmpeg_af_s16_to_fltp(const int16_t *src, float *const *dst, int channels, int nb_samples)
{
	int i, c;

	if (channels == 2) {
		float *dst0 = dst[0];
		float *dst1 = dst[1];

		for (i = 0; i + 4 <= nb_samples; i += 4) {
			dst0[i + 0] = (float)src[(i + 0) * 2 + 0] * (1.0f / 32768.0f);
			dst1[i + 0] = (float)src[(i + 0) * 2 + 1] * (1.0f / 32768.0f);
			dst0[i + 1] = (float)src[(i + 1) * 2 + 0] * (1.0f / 32768.0f);
			dst1[i + 1] = (float)src[(i + 1) * 2 + 1] * (1.0f / 32768.0f);
			dst0[i + 2] = (float)src[(i + 2) * 2 + 0] * (1.0f / 32768.0f);
			dst1[i + 2] = (float)src[(i + 2) * 2 + 1] * (1.0f / 32768.0f);
			dst0[i + 3] = (float)src[(i + 3) * 2 + 0] * (1.0f / 32768.0f);
			dst1[i + 3] = (float)src[(i + 3) * 2 + 1] * (1.0f / 32768.0f);
		}
		for (; i < nb_samples; i++) {
			dst0[i] = (float)src[i * 2 + 0] * (1.0f / 32768.0f);
			dst1[i] = (float)src[i * 2 + 1] * (1.0f / 32768.0f);
		}
		return;
	}

	for (i = 0; i < nb_samples; i++) {
		for (c = 0; c < channels; c++) {
			dst[c][i] = (float)src[i * channels + c] * (1.0f / 32768.0f);
		}
	}
}

static inline void ffmpeg_af_s32_to_fltp(const int32_t *src, float *const *dst, int channels, int nb_samples)
{
#ifdef FFMPEG_AF_VFPU_CONV
	int i, c;

	for (c = 0; c < channels; c++) {
		float *pl = dst[c];
		const int32_t *sc = src + c;

		for (i = 0; i + 2 <= nb_samples; i += 2) {
			int32_t tmp[2] __attribute__((aligned(8)));
			ae_int32x2 p;
			ae_xtfloatx2 r;

			tmp[0] = sc[i * channels];
			tmp[1] = sc[(i + 1) * channels];
			p = AE_L32X2_I((const ae_int32x2 *)tmp, 0);
			r = XT_FLOAT_SX2(p, 31);
			AE_S32X2_I((ae_int32x2)r, (ae_int32x2 *)&pl[i], 0);
		}
		for (; i < nb_samples; i++)
			pl[i] = (float)sc[i * channels] / FFMPEG_AF_S32_SCALE;
	}
#else
	int i, c;

	if (channels == 2) {
		float *dst0 = dst[0];
		float *dst1 = dst[1];

		for (i = 0; i + 4 <= nb_samples; i += 4) {
			dst0[i + 0] = (float)src[(i + 0) * 2 + 0] * (1.0f / FFMPEG_AF_S32_SCALE);
			dst1[i + 0] = (float)src[(i + 0) * 2 + 1] * (1.0f / FFMPEG_AF_S32_SCALE);
			dst0[i + 1] = (float)src[(i + 1) * 2 + 0] * (1.0f / FFMPEG_AF_S32_SCALE);
			dst1[i + 1] = (float)src[(i + 1) * 2 + 1] * (1.0f / FFMPEG_AF_S32_SCALE);
			dst0[i + 2] = (float)src[(i + 2) * 2 + 0] * (1.0f / FFMPEG_AF_S32_SCALE);
			dst1[i + 2] = (float)src[(i + 2) * 2 + 1] * (1.0f / FFMPEG_AF_S32_SCALE);
			dst0[i + 3] = (float)src[(i + 3) * 2 + 0] * (1.0f / FFMPEG_AF_S32_SCALE);
			dst1[i + 3] = (float)src[(i + 3) * 2 + 1] * (1.0f / FFMPEG_AF_S32_SCALE);
		}
		for (; i < nb_samples; i++) {
			dst0[i] = (float)src[i * 2 + 0] * (1.0f / FFMPEG_AF_S32_SCALE);
			dst1[i] = (float)src[i * 2 + 1] * (1.0f / FFMPEG_AF_S32_SCALE);
		}
		return;
	}

	for (i = 0; i < nb_samples; i++) {
		for (c = 0; c < channels; c++) {
			dst[c][i] = (float)src[i * channels + c] * (1.0f / FFMPEG_AF_S32_SCALE);
		}
	}
#endif
}

static void ffmpeg_af_fltp_to_s32(const float *const *src, int32_t *dst, int channels, int nb_samples)
{
#ifdef FFMPEG_AF_VFPU_CONV
	int i, c;

	for (c = 0; c < channels; c++) {
		const float *pl = src[c];
		int32_t *dc = dst + c;

		for (i = 0; i + 2 <= nb_samples; i += 2) {
			int32_t tmp[2] __attribute__((aligned(8)));
			ae_int32x2 f, q;

			f = AE_L32X2_I((const ae_int32x2 *)&pl[i], 0);
			q = XT_TRUNC_SX2((ae_xtfloatx2)f, 31);
			AE_S32X2_I(q, tmp, 0);
			dc[i * channels] = tmp[0];
			dc[(i + 1) * channels] = tmp[1];
		}
		for (; i < nb_samples; i++) {
			float f = pl[i] * FFMPEG_AF_S32_SCALE;

			f = f > 2147483647.0f ? 2147483647.0f :
			    (f < -2147483648.0f ? -2147483648.0f : f);
			dc[i * channels] = (int32_t)f;
		}
	}
#else
	int i, c;

	if (channels == 2) {
		const float *src0 = src[0];
		const float *src1 = src[1];

		for (i = 0; i + 4 <= nb_samples; i += 4) {
			dst[(i + 0) * 2 + 0] = ffmpeg_af_flt_to_s32_sample(src0[i + 0]);
			dst[(i + 0) * 2 + 1] = ffmpeg_af_flt_to_s32_sample(src1[i + 0]);
			dst[(i + 1) * 2 + 0] = ffmpeg_af_flt_to_s32_sample(src0[i + 1]);
			dst[(i + 1) * 2 + 1] = ffmpeg_af_flt_to_s32_sample(src1[i + 1]);
			dst[(i + 2) * 2 + 0] = ffmpeg_af_flt_to_s32_sample(src0[i + 2]);
			dst[(i + 2) * 2 + 1] = ffmpeg_af_flt_to_s32_sample(src1[i + 2]);
			dst[(i + 3) * 2 + 0] = ffmpeg_af_flt_to_s32_sample(src0[i + 3]);
			dst[(i + 3) * 2 + 1] = ffmpeg_af_flt_to_s32_sample(src1[i + 3]);
		}
		for (; i < nb_samples; i++) {
			dst[i * 2 + 0] = ffmpeg_af_flt_to_s32_sample(src0[i]);
			dst[i * 2 + 1] = ffmpeg_af_flt_to_s32_sample(src1[i]);
		}
		return;
	}

	for (i = 0; i < nb_samples; i++) {
		for (c = 0; c < channels; c++) {
			dst[i * channels + c] = ffmpeg_af_flt_to_s32_sample(src[c][i]);
		}
	}
#endif
}

static inline int16_t ffmpeg_af_flt_to_s16_sample(float val)
{
	float scaled = val * 32768.0f;

	if (scaled >= 32767.0f)
		return 32767;
	if (scaled <= -32768.0f)
		return -32768;
	return (int16_t)scaled;
}

static void ffmpeg_af_fltp_to_s16(const float *const *src, int16_t *dst, int channels, int nb_samples)
{
	int i, c;

	if (channels == 2) {
		const float *src0 = src[0];
		const float *src1 = src[1];

		for (i = 0; i + 4 <= nb_samples; i += 4) {
			dst[(i + 0) * 2 + 0] = ffmpeg_af_flt_to_s16_sample(src0[i + 0]);
			dst[(i + 0) * 2 + 1] = ffmpeg_af_flt_to_s16_sample(src1[i + 0]);
			dst[(i + 1) * 2 + 0] = ffmpeg_af_flt_to_s16_sample(src0[i + 1]);
			dst[(i + 1) * 2 + 1] = ffmpeg_af_flt_to_s16_sample(src1[i + 1]);
			dst[(i + 2) * 2 + 0] = ffmpeg_af_flt_to_s16_sample(src0[i + 2]);
			dst[(i + 2) * 2 + 1] = ffmpeg_af_flt_to_s16_sample(src1[i + 2]);
			dst[(i + 3) * 2 + 0] = ffmpeg_af_flt_to_s16_sample(src0[i + 3]);
			dst[(i + 3) * 2 + 1] = ffmpeg_af_flt_to_s16_sample(src1[i + 3]);
		}
		for (; i < nb_samples; i++) {
			dst[i * 2 + 0] = ffmpeg_af_flt_to_s16_sample(src0[i]);
			dst[i * 2 + 1] = ffmpeg_af_flt_to_s16_sample(src1[i]);
		}
		return;
	}

	for (i = 0; i < nb_samples; i++) {
		for (c = 0; c < channels; c++) {
			dst[i * channels + c] = ffmpeg_af_flt_to_s16_sample(src[c][i]);
		}
	}
}

static void ffmpeg_af_store_out_frame(struct ffmpeg_af_graph *g, const AVFrame *frame, int nb_samples)
{
	uint8_t *dst = g->pcm_out + g->pcm_out_count * g->out_frame_bytes;

	if (frame->format == AV_SAMPLE_FMT_DBL) {
		const double *src = (const double *)frame->data[0];
		int total = nb_samples * g->channels;
		int i;

		if (g->out_frame_bytes == g->channels * sizeof(int16_t)) {
			int16_t *dst16 = (int16_t *)dst;
			for (i = 0; i < total; i++) {
				double v = src[i] * 32768.0;
				if (v >= 32767.0)
					dst16[i] = 32767;
				else if (v <= -32768.0)
					dst16[i] = -32768;
				else
					dst16[i] = (int16_t)v;
			}
		} else {
			int32_t *dst32 = (int32_t *)dst;
			for (i = 0; i < total; i++) {
				double v = src[i] * 2147483648.0;
				if (v >= 2147483647.0)
					dst32[i] = 2147483647;
				else if (v <= -2147483648.0)
					dst32[i] = (int32_t)-2147483648LL;
				else
					dst32[i] = (int32_t)v;
			}
		}
	} else {
		if (g->out_frame_bytes == g->channels * sizeof(int16_t)) {
			ffmpeg_af_fltp_to_s16((const float *const *)frame->data,
					      (int16_t *)dst, g->channels, nb_samples);
		} else {
			ffmpeg_af_fltp_to_s32((const float *const *)frame->data,
					      (int32_t *)dst, g->channels, nb_samples);
		}
	}
	g->pcm_out_count += nb_samples;
}

static void ffmpeg_af_copy_from_circular(void *dst, const void *src,
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

static void ffmpeg_af_copy_to_circular(void *dst, void *buf_start,
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

int ffmpeg_af_mod_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_dec_comp_data *cd;
	struct ffmpeg_af_graph *g;

	ffmpeg_dec_libc_bind(mod);
	comp_info(dev, "ffmpeg_af_mod_init entry");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;
	md->private = cd;

	md->mpd.in_buff_size = 9600;
	md->mpd.out_buff_size = 9600;

	g = mod_zalloc(mod, sizeof(struct ffmpeg_af_graph));
	if (!g) {
		mod_free(mod, cd);
		md->private = NULL;
		return -ENOMEM;
	}

	cd->af_graph = g;

#if CONFIG_IPC_MAJOR_4
	cd->eos_msg = ffmpeg_dec_eos_notification_init(mod);
	if (!cd->eos_msg) {
		comp_err(dev, "failed to allocate EOS notification");
		mod_free(mod, g);
		mod_free(mod, cd);
		md->private = NULL;
		return -ENOMEM;
	}
#endif

	return 0;
}

int ffmpeg_af_mod_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_af_graph *g;
	struct comp_buffer *sinkb;
	const struct audio_stream *stream;

	if (!cd || !cd->af_graph)
		return -EINVAL;

	ffmpeg_dec_libc_bind(mod);
	g = cd->af_graph;

	/* Determine PCM parameters from connected sink buffer, or fallback safely. */
	sinkb = comp_dev_get_first_data_consumer(dev);
	if (sinkb) {
		stream = &sinkb->stream;
		g->rate = audio_stream_get_rate(stream);
		g->channels = audio_stream_get_channels(stream);
		g->out_frame_bytes = audio_stream_frame_bytes(stream);
	} else if (sinks && sinks[0] && sinks[0]->audio_stream_params) {
		g->rate = sink_get_rate(sinks[0]);
		g->channels = sink_get_channels(sinks[0]);
		g->out_frame_bytes = sink_get_frame_bytes(sinks[0]);
	} else if (sources && sources[0] && sources[0]->audio_stream_params) {
		g->rate = source_get_rate(sources[0]);
		g->channels = source_get_channels(sources[0]);
		g->out_frame_bytes = source_get_frame_bytes(sources[0]);
	} else {
		g->rate = 48000;
		g->channels = 2;
		g->out_frame_bytes = 4;
	}

	if (!g->rate)
		g->rate = 48000;
	if (!g->channels)
		g->channels = 2;
	if (!g->out_frame_bytes)
		g->out_frame_bytes = g->channels * sizeof(int16_t);

	if (sources && sources[0] && sources[0]->audio_stream_params)
		g->in_frame_bytes = source_get_frame_bytes(sources[0]);
	else
		g->in_frame_bytes = g->channels * sizeof(int16_t);

	if (!g->in_frame_bytes)
		g->in_frame_bytes = g->channels * sizeof(int16_t);

	cd->out_rate = g->rate;
	cd->out_channels = g->channels;
	cd->out_frame_bytes = g->out_frame_bytes;

	comp_info(dev, "ffmpeg_af: afftdn, rate %u ch %u in_frame_bytes %u out_frame_bytes %u",
		  g->rate, g->channels, g->in_frame_bytes, g->out_frame_bytes);
	printk("[ffmpeg_af] prepare: g=%p rate=%d ch=%d in_b=%d out_b=%d\n",
	       g, g->rate, g->channels, g->in_frame_bytes, g->out_frame_bytes);

	/*
	 * NB: Lazy open deferred to DP thread.
	 * prepare() is dispatched across cores via IDC and runs on Core 1's
	 * single IDC worker thread with a tiny default stack (~2 KiB).
	 * Filter open (avfilter_graph_config / afftdn table building) takes
	 * tens of ms and requires a deep stack. Doing that here would overflow
	 * the IDC worker stack and stall Core 0, timing out the host IPC.
	 * Defer ffmpeg_af_open to the first process() call on the module's
	 * DP thread (256 KiB stack, off the IDC critical path).
	 */
	cd->configured = false;
	cd->eos_sent = false;

	return 0;
}

int ffmpeg_af_mod_process(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_source *source;
	struct sof_sink *sink;
	struct ffmpeg_af_graph *g;
	size_t avail_bytes, free_bytes, req_bytes;
	int avail_frames, space_frames, to_read;
	int free_frames, to_drain;
	const void *sp, *sstart;
	void *dst, *buf_start;
	size_t sbytes, buf_size;
	int ret, out_nb;
	enum sof_audio_buffer_state src_state;
	bool host_eos;
	static int proc_count = 0;
	uint32_t c0, c1;

	ffmpeg_dec_libc_bind(mod);
	if (!cd || !cd->af_graph)
		return -EINVAL;

	/* Lazy open: runs once on the module's DP thread with deep stack */
	if (!cd->configured) {
		enum AVSampleFormat fmt = AV_SAMPLE_FMT_FLTP;
		if (strcmp(CONFIG_FFMPEG_AF_FILTER_NAME, "alimiter") == 0)
			fmt = AV_SAMPLE_FMT_DBL;

		printk("[ffmpeg_af] lazy open entering on DP thread (filter=%s fmt=%d)...\n",
		       CONFIG_FFMPEG_AF_FILTER_NAME, fmt);
		ret = ffmpeg_af_open(cd->af_graph, CONFIG_FFMPEG_AF_FILTER_NAME, fmt);
		printk("[ffmpeg_af] lazy open done ret=%d\n", ret);
		if (ret < 0) {
			comp_err(dev, "ffmpeg_af_open failed %d", ret);
			return ret;
		}
		cd->configured = true;
		return 0;	/* proceed to process on next cycle */
	}

	if (num_of_sources < 1 || num_of_sinks < 1)
		return 0;

	source = sources[0];
	sink = sinks[0];
	g = cd->af_graph;
	src_state = source_get_state(source);
	host_eos = src_state == AUDIOBUF_STATE_END_OF_STREAM ||
		   src_state == AUDIOBUF_STATE_END_OF_STREAM_FLUSH;

	if ((proc_count++ % 20) == 0) {
		printk("[ffmpeg_af] mod_process #%d: in_cnt=%zu out_cnt=%zu avail_b=%zu free_b=%zu eos=%d\n",
		       proc_count, g->pcm_in_count, g->pcm_out_count,
		       source_get_data_available(source), sink_get_free_size(sink), host_eos);
	}

	/* 1. Drain previously staged filtered samples into the sink if space is available. */
	if (g->pcm_out_count > 0) {
		free_bytes = sink_get_free_size(sink);
		free_frames = (int)(free_bytes / g->out_frame_bytes);
		to_drain = MIN((int)g->pcm_out_count, free_frames);
		if (to_drain > 0) {
			req_bytes = to_drain * g->out_frame_bytes;
			ret = sink_get_buffer(sink, req_bytes, &dst, &buf_start, &buf_size);
			if (ret == 0) {
				ffmpeg_af_copy_to_circular(dst, buf_start, buf_size,
							   g->pcm_out, req_bytes);
				sink_commit_buffer(sink, req_bytes);
				if (g->pcm_out_count > (size_t)to_drain)
					memmove(g->pcm_out, g->pcm_out + to_drain * g->out_frame_bytes,
						(g->pcm_out_count - to_drain) * g->out_frame_bytes);
				g->pcm_out_count -= to_drain;
			}
		}
	}

	/* 2. Read new input samples from source into pcm_in staging buffer. */
	avail_bytes = source_get_data_available(source);
	space_frames = (int)(g->pcm_in_cap - g->pcm_in_count);
	avail_frames = (int)(avail_bytes / g->in_frame_bytes);
	to_read = MIN(avail_frames, space_frames);
	if (to_read > 0) {
		req_bytes = to_read * g->in_frame_bytes;
		ret = source_get_data(source, req_bytes, &sp, &sstart, &sbytes);
		if (ret == 0) {
			ffmpeg_af_copy_from_circular(&g->pcm_in[g->pcm_in_count * g->channels],
						     sp, sstart, sbytes, req_bytes);
			source_release_data(source, req_bytes);
			g->pcm_in_count += to_read;
		}
	}

	/* 3. Process at most one filter frame through filter to yield to scheduler and IDC. */
	if ((int)g->pcm_in_count >= g->frame_size &&
	    (int)(g->pcm_out_cap - g->pcm_out_count) >= g->frame_size) {
		if (g->in_frame->format == AV_SAMPLE_FMT_DBL) {
			double *dst = (double *)g->in_frame->data[0];
			int total = g->frame_size * g->channels;
			int i;

			for (i = 0; i < total; i++)
				dst[i] = (double)g->pcm_in[i] * (1.0 / 32768.0);
		} else {
			ffmpeg_af_s16_to_fltp(g->pcm_in, (float *const *)g->in_frame->data,
					      g->channels, g->frame_size);
		}

		if (g->pcm_in_count > (size_t)g->frame_size)
			memmove(g->pcm_in, &g->pcm_in[g->frame_size * g->channels],
				(g->pcm_in_count - g->frame_size) * g->in_frame_bytes);
		g->pcm_in_count -= g->frame_size;

		c0 = k_cycle_get_32();
		ret = av_buffersrc_add_frame_flags(g->src, g->in_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
		c1 = k_cycle_get_32();
		printk("[ffmpeg_af] filter cycles=%u (%u us) ret=%d\n", c1 - c0, (c1 - c0) / 400, ret);
		if (ret < 0) {
			printk("[ffmpeg_af] buffersrc_add_frame failed %d\n", ret);
		}

		/* Pull all filtered frames available from sink. */
		while ((int)(g->pcm_out_cap - g->pcm_out_count) >= g->frame_size) {
			ret = av_buffersink_get_frame(g->sink, g->out_frame);
			if (ret < 0)
				break;

			out_nb = g->out_frame->nb_samples;
			printk("[ffmpeg_af] filter produced %d samples\n", out_nb);
			if (g->pcm_out_count + out_nb <= g->pcm_out_cap)
				ffmpeg_af_store_out_frame(g, g->out_frame, out_nb);
			av_frame_unref(g->out_frame);
		}
	}

	/* Also pull any remaining filtered frames from sink. */
	while ((int)(g->pcm_out_cap - g->pcm_out_count) >= g->frame_size) {
		ret = av_buffersink_get_frame(g->sink, g->out_frame);
		if (ret < 0)
			break;

		out_nb = g->out_frame->nb_samples;
		printk("[ffmpeg_af] filter residual %d samples\n", out_nb);
		if (g->pcm_out_count + out_nb <= g->pcm_out_cap)
			ffmpeg_af_store_out_frame(g, g->out_frame, out_nb);
		av_frame_unref(g->out_frame);
	}

	/* 4. Drain any freshly produced samples to the sink. */
	if (g->pcm_out_count > 0) {
		free_bytes = sink_get_free_size(sink);
		free_frames = (int)(free_bytes / g->out_frame_bytes);
		to_drain = MIN((int)g->pcm_out_count, free_frames);
		if (to_drain > 0) {
			req_bytes = to_drain * g->out_frame_bytes;
			ret = sink_get_buffer(sink, req_bytes, &dst, &buf_start, &buf_size);
			if (ret == 0) {
				ffmpeg_af_copy_to_circular(dst, buf_start, buf_size,
							   g->pcm_out, req_bytes);
				sink_commit_buffer(sink, req_bytes);
				if (g->pcm_out_count > (size_t)to_drain)
					memmove(g->pcm_out, g->pcm_out + to_drain * g->out_frame_bytes,
						(g->pcm_out_count - to_drain) * g->out_frame_bytes);
				g->pcm_out_count -= to_drain;
				printk("[ffmpeg_af] drain 4: sent %d frames to sink, left=%zu free_b=%zu\n",
				       to_drain, g->pcm_out_count, sink_get_free_size(sink));
			}
		} else {
			printk("[ffmpeg_af] drain 4: sink full! free_frames=%d out_cnt=%zu\n",
			       free_frames, g->pcm_out_count);
		}
	}

#if CONFIG_IPC_MAJOR_4
	/* 5. Signal EOS when source is exhausted and all filtered audio has drained. */
	if (host_eos && g->pcm_in_count < (size_t)g->frame_size && g->pcm_out_count == 0) {
		if (!cd->eos_sent) {
			g->pcm_in_count = 0;
			printk("[ffmpeg_af] signaling EOS\n");
			ffmpeg_dec_signal_eos(mod, sink);
		}
	}
#endif

	return 0;
}

bool ffmpeg_af_is_ready_to_process(struct processing_module *mod,
				   struct sof_source **sources, int num_of_sources,
				   struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_af_graph *g;

	if (num_of_sources < 1 || num_of_sinks < 1)
		return false;

	if (!cd)
		return false;

	/* Run lazy open on DP thread if not configured yet */
	if (!cd->configured) {
		printk("[ffmpeg_af] is_ready: !configured -> true\n");
		return true;
	}

	g = cd->af_graph;
	if (!g)
		return false;

	if (cd->eos_sent && g->pcm_out_count == 0)
		return false;

	if (g->pcm_out_count > 0 && sink_get_free_size(sinks[0]) >= (size_t)g->out_frame_bytes)
		return true;

	if (!cd->eos_sent && source_get_data_available(sources[0]) >= (size_t)g->in_frame_bytes &&
	    g->pcm_in_count < g->pcm_in_cap)
		return true;

#if CONFIG_IPC_MAJOR_4
	if (!cd->eos_sent) {
		enum sof_audio_buffer_state src_state = source_get_state(sources[0]);
		if (src_state == AUDIOBUF_STATE_END_OF_STREAM ||
		    src_state == AUDIOBUF_STATE_END_OF_STREAM_FLUSH)
			return true;
	}
#endif

	return false;
}

int ffmpeg_af_mod_reset(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_af_graph *g;

	if (!cd || !cd->af_graph)
		return 0;

	g = cd->af_graph;
	g->pcm_in_count = 0;
	g->pcm_out_count = 0;
	g->pcm_out_rd = 0;
	cd->eos_sent = false;
	return 0;
}

int ffmpeg_af_mod_free(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	ffmpeg_dec_libc_bind(mod);
	if (!cd)
		return 0;

#if CONFIG_IPC_MAJOR_4
	if (cd->eos_msg) {
		ipc_msg_free(cd->eos_msg);
		cd->eos_msg = NULL;
	}
#endif

	if (cd->af_graph) {
		ffmpeg_af_close(cd->af_graph);
		mod_free(mod, cd->af_graph);
		cd->af_graph = NULL;
	}
	mod_free(mod, cd);
	md->private = NULL;
	return 0;
}
#endif /* CONFIG_FFMPEG_DEC_FILTER_MODE */
