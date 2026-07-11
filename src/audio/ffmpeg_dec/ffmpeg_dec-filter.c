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

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

/**
 * struct ffmpeg_af_graph - one audio filter graph instance.
 * @graph: The avfilter graph.
 * @src:   abuffer source context (frames pushed here).
 * @sink:  abuffersink context (filtered frames pulled here).
 */
struct ffmpeg_af_graph {
	AVFilterGraph *graph;
	AVFilterContext *src;
	AVFilterContext *sink;
};

/**
 * ffmpeg_af_open() - Build a src -> <filter> -> sink graph for the given PCM
 * format.
 * @g:        Graph to initialise.
 * @filter:   Filter name, e.g. "afftdn".
 * @rate:     Sample rate (Hz).
 * @channels: Channel count.
 * @fmt:      Interleaved AV sample format (e.g. AV_SAMPLE_FMT_S32).
 *
 * Return: Zero on success, negative errno otherwise.
 */
int ffmpeg_af_open(struct ffmpeg_af_graph *g, const char *filter,
		   int rate, int channels, enum AVSampleFormat fmt)
{
	const AVFilter *abuffer = avfilter_get_by_name("abuffer");
	const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
	const AVFilter *filt = avfilter_get_by_name(filter);
	AVFilterContext *filt_ctx = NULL;
	AVChannelLayout layout;
	char chbuf[64];
	char args[256];
	int ret;

	memset(g, 0, sizeof(*g));
	if (!abuffer || !abuffersink || !filt)
		return -ENODEV;

	g->graph = avfilter_graph_alloc();
	if (!g->graph)
		return -ENOMEM;

	av_channel_layout_default(&layout, channels);
	av_channel_layout_describe(&layout, chbuf, sizeof(chbuf));

	snprintf(args, sizeof(args),
		 "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
		 rate, rate, av_get_sample_fmt_name(fmt), chbuf);

	ret = avfilter_graph_create_filter(&g->src, abuffer, "in", args, NULL, g->graph);
	if (ret < 0)
		goto err;
	ret = avfilter_graph_create_filter(&filt_ctx, filt, "filter", NULL, NULL, g->graph);
	if (ret < 0)
		goto err;
	ret = avfilter_graph_create_filter(&g->sink, abuffersink, "out", NULL, NULL, g->graph);
	if (ret < 0)
		goto err;

	ret = avfilter_link(g->src, 0, filt_ctx, 0);
	if (!ret)
		ret = avfilter_link(filt_ctx, 0, g->sink, 0);
	if (ret < 0)
		goto err;

	ret = avfilter_graph_config(g->graph, NULL);
	if (ret < 0)
		goto err;

	return 0;

err:
	avfilter_graph_free(&g->graph);
	return -EIO;
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
	int ret = av_buffersrc_add_frame(g->src, in);

	if (ret < 0)
		return -EIO;

	ret = av_buffersink_get_frame(g->sink, out);
	if (ret == AVERROR(EAGAIN))
		return -EAGAIN;
	if (ret < 0)
		return -EIO;

	return 0;
}

void ffmpeg_af_close(struct ffmpeg_af_graph *g)
{
	if (g->graph)
		avfilter_graph_free(&g->graph);
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

#define FFMPEG_AF_MAX_CHUNK	4096
#define FFMPEG_AF_S32_SCALE	2147483648.0f	/* 2^31 */
#ifndef CONFIG_FFMPEG_AF_FILTER_NAME
#define CONFIG_FFMPEG_AF_FILTER_NAME	"afftdn"
#endif

int ffmpeg_af_mod_init(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	ffmpeg_dec_libc_bind(mod);
	cd->af_graph = mod_zalloc(mod, sizeof(struct ffmpeg_af_graph));
	return cd->af_graph ? 0 : -ENOMEM;
}

int ffmpeg_af_mod_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	ffmpeg_dec_libc_bind(mod);
	cd->out_rate = source_get_rate(sources[0]);
	cd->out_channels = source_get_channels(sources[0]);
	cd->out_frame_bytes = source_get_frame_bytes(sources[0]);

	comp_info(dev, "ffmpeg_af: %s, rate %u ch %u",
		  CONFIG_FFMPEG_AF_FILTER_NAME, cd->out_rate, cd->out_channels);

	/* Graph runs in float planar; the module converts to/from S32. */
	return ffmpeg_af_open(cd->af_graph, CONFIG_FFMPEG_AF_FILTER_NAME,
			      cd->out_rate, cd->out_channels, AV_SAMPLE_FMT_FLTP);
}

int ffmpeg_af_mod_process(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct sof_source *source = sources[0];
	struct sof_sink *sink = sinks[0];
	int ch = cd->out_channels;
	int in_frames = source_get_data_frames_available(source);
	int n = MIN(in_frames, FFMPEG_AF_MAX_CHUNK);
	AVFrame *in, *out;
	const int32_t *src;
	int32_t *dst;
	int i, c, m, ret;

	ffmpeg_dec_libc_bind(mod);
	if (n <= 0)
		return 0;

	/* Build the FLTP input frame (deinterleave + normalize S32 -> float). */
	in = av_frame_alloc();
	if (!in)
		return -ENOMEM;
	in->format = AV_SAMPLE_FMT_FLTP;
	in->nb_samples = n;
	in->sample_rate = cd->out_rate;
	av_channel_layout_default(&in->ch_layout, ch);
	if (av_frame_get_buffer(in, 0) < 0) {
		av_frame_free(&in);
		return -ENOMEM;
	}

	ret = source_get_data_s32(source, n * cd->out_frame_bytes, &src, NULL, NULL);
	if (ret) {
		av_frame_free(&in);
		return ret;
	}
	for (i = 0; i < n; i++)
		for (c = 0; c < ch; c++)
			((float *)in->data[c])[i] = (float)src[i * ch + c] / FFMPEG_AF_S32_SCALE;
	source_release_data(source, n * cd->out_frame_bytes);

	/* Run the graph. */
	out = av_frame_alloc();
	if (!out) {
		av_frame_free(&in);
		return -ENOMEM;
	}
	ret = ffmpeg_af_filter(cd->af_graph, in, out);
	av_frame_free(&in);
	if (ret == -EAGAIN) {		/* filter still priming, no output yet */
		av_frame_free(&out);
		return 0;
	}
	if (ret) {
		av_frame_free(&out);
		return ret;
	}

	/* Interleave + denormalize float -> S32 into the sink. */
	m = MIN(out->nb_samples, sink_get_free_frames(sink));
	if (m > 0 && sink_get_buffer_s32(sink, m * cd->out_frame_bytes, &dst, NULL, NULL) == 0) {
		for (i = 0; i < m; i++)
			for (c = 0; c < ch; c++) {
				float f = ((const float *)out->data[c])[i] * FFMPEG_AF_S32_SCALE;

				f = f > 2147483647.0f ? 2147483647.0f :
				    (f < -2147483648.0f ? -2147483648.0f : f);
				dst[i * ch + c] = (int32_t)f;
			}
		sink_commit_buffer(sink, m * cd->out_frame_bytes);
	}
	av_frame_free(&out);
	return 0;
}

int ffmpeg_af_mod_free(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);

	ffmpeg_dec_libc_bind(mod);
	if (cd->af_graph) {
		ffmpeg_af_close(cd->af_graph);
		mod_free(mod, cd->af_graph);
		cd->af_graph = NULL;
	}
	return 0;
}
#endif /* CONFIG_FFMPEG_DEC_FILTER_MODE */
