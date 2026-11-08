// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// libavfilter graph backend for the ffmpeg_dec module: runs an FFmpeg audio
// filter (e.g. afftdn FFT noise reduction, alimiter lookahead peak limiter)
// over PCM. Unlike the decoder/encoder paths (send_packet/receive_frame or
// send_frame/receive_packet), filters use the avfilter graph API:
//
//     abuffer(src) -> <filter> -> abuffersink(sink)
//
// PCM AVFrames are pushed into the source and pulled filtered from the sink.
// Built when CONFIG_FFMPEG_FILTER is enabled (selected by any
// CONFIG_FFMPEG_FILTER_* option), which pulls libavfilter into the module.

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_api.h>
#include <sof/math/numbers.h>
#include <ipc/compress_params.h>
#include <rtos/string.h>
#include <errno.h>
#include <stdio.h>
#include "ffmpeg_dec.h"
#include "ffmpeg_dec-convert.h"

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>

LOG_MODULE_DECLARE(ffmpeg_dec, CONFIG_SOF_LOG_LEVEL);

#define FFMPEG_AF_MAX_NAME		32
#define FFMPEG_AF_MAX_ARGS		128
#define FFMPEG_AF_LAYOUT_STR_SIZE	64

enum ffmpeg_af_filter_id {
	FFMPEG_AF_FILTER_DEFAULT = 0,
	FFMPEG_AF_FILTER_AFFTDN = 1,
	FFMPEG_AF_FILTER_ALIMITER = 2,
};

struct ffmpeg_af_filter_desc {
	enum ffmpeg_af_filter_id id;
	const char *name;
	const char *default_args;
	enum AVSampleFormat sample_fmt;
};

static const struct ffmpeg_af_filter_desc ffmpeg_af_filters[] = {
#if defined(CONFIG_FFMPEG_FILTER_AFFTDN)
	{
		.id = FFMPEG_AF_FILTER_AFFTDN,
		.name = "afftdn",
		.default_args = "nr=28:nf=-20",
		.sample_fmt = AV_SAMPLE_FMT_FLTP,
	},
#endif
#if defined(CONFIG_FFMPEG_FILTER_ALIMITER)
	{
		.id = FFMPEG_AF_FILTER_ALIMITER,
		.name = "alimiter",
		.default_args = "limit=0.95:attack=5:release=50:level=true",
		.sample_fmt = AV_SAMPLE_FMT_DBL,
	},
#endif
};

static const struct ffmpeg_af_filter_desc *ffmpeg_af_default_filter(void)
{
#if defined(CONFIG_FFMPEG_FILTER_DEFAULT_ALIMITER)
	for (size_t i = 0; i < ARRAY_SIZE(ffmpeg_af_filters); i++) {
		if (ffmpeg_af_filters[i].id == FFMPEG_AF_FILTER_ALIMITER)
			return &ffmpeg_af_filters[i];
	}
#endif
	if (ARRAY_SIZE(ffmpeg_af_filters) > 0)
		return &ffmpeg_af_filters[0];
	return NULL;
}

static const struct ffmpeg_af_filter_desc *ffmpeg_af_find_by_id(uint32_t id)
{
	for (size_t i = 0; i < ARRAY_SIZE(ffmpeg_af_filters); i++) {
		if (ffmpeg_af_filters[i].id == id)
			return &ffmpeg_af_filters[i];
	}
	return NULL;
}

static const struct ffmpeg_af_filter_desc *ffmpeg_af_find_by_name(const char *name)
{
	for (size_t i = 0; i < ARRAY_SIZE(ffmpeg_af_filters); i++) {
		if (strcmp(ffmpeg_af_filters[i].name, name) == 0)
			return &ffmpeg_af_filters[i];
	}
	return NULL;
}

/**
 * struct ffmpeg_af_graph - one audio filter graph instance.
 * @graph:           The avfilter graph.
 * @src:             abuffer source context (frames pushed here).
 * @sink:            abuffersink context (filtered frames pulled here).
 * @in_frame:        Persistent pre-allocated input AVFrame.
 * @out_frame:       Persistent pre-allocated output AVFrame.
 * @desc:            Selected filter descriptor.
 * @filter_name:     Filter name string (e.g. "afftdn", "alimiter").
 * @filter_args:     Filter argument string passed to avfilter_graph_create_filter().
 * @frame_size:      Number of samples per filter advance block (rate / 80).
 * @channels:        Channel count.
 * @rate:            Sample rate (Hz).
 * @in_frame_fmt:    SOF input valid sample format (enum sof_ipc_frame).
 * @out_frame_fmt:   SOF output valid sample format (enum sof_ipc_frame).
 * @in_frame_bytes:  Bytes per multichannel input audio frame.
 * @out_frame_bytes: Bytes per multichannel output audio frame.
 * @pcm_in:          Linear staging buffer for input interleaved PCM.
 * @pcm_in_count:    Current number of audio frames in @pcm_in.
 * @pcm_in_cap:      Maximum capacity of @pcm_in in audio frames.
 * @pcm_out:         Linear staging FIFO for filtered interleaved PCM.
 * @pcm_out_count:   Current number of unread audio frames in @pcm_out.
 * @pcm_out_rd:      Read offset in audio frames for draining @pcm_out.
 * @pcm_out_cap:     Maximum capacity of @pcm_out in audio frames.
 * @eof_sent:        True once NULL EOF frame has been submitted to @src.
 * @eof_flushed:     True once @sink has returned AVERROR_EOF.
 */
struct ffmpeg_af_graph {
	AVFilterGraph *graph;
	AVFilterContext *src;
	AVFilterContext *sink;
	AVFrame *in_frame;
	AVFrame *out_frame;
	const struct ffmpeg_af_filter_desc *desc;
	char filter_name[FFMPEG_AF_MAX_NAME];
	char filter_args[FFMPEG_AF_MAX_ARGS];
	int frame_size;
	int channels;
	int rate;
	int in_frame_fmt;
	int out_frame_fmt;
	int in_frame_bytes;
	int out_frame_bytes;

	uint8_t *pcm_in;
	size_t pcm_in_count;
	size_t pcm_in_cap;

	uint8_t *pcm_out;
	size_t pcm_out_count;
	size_t pcm_out_rd;
	size_t pcm_out_cap;

	bool eof_sent;
	bool eof_flushed;
};

static void ffmpeg_af_close(struct processing_module *mod, struct ffmpeg_af_graph *g)
{
	if (!g)
		return;

	if (g->in_frame)
		av_frame_free(&g->in_frame);
	if (g->out_frame)
		av_frame_free(&g->out_frame);
	if (g->pcm_in) {
		mod_free(mod, g->pcm_in);
		g->pcm_in = NULL;
	}
	if (g->pcm_out) {
		mod_free(mod, g->pcm_out);
		g->pcm_out = NULL;
	}
	if (g->graph)
		avfilter_graph_free(&g->graph);
	g->src = NULL;
	g->sink = NULL;
	g->pcm_in_count = 0;
	g->pcm_out_count = 0;
	g->pcm_out_rd = 0;
	g->eof_sent = false;
	g->eof_flushed = false;
}

static void ffmpeg_af_quiet_log(void *avcl, int level, const char *fmt, va_list vl)
{
	(void)avcl;
	(void)level;
	(void)fmt;
	(void)vl;
}

/**
 * ffmpeg_af_open() - Build a src -> <filter> -> sink graph for the given PCM
 * format.
 * @mod:    Pointer to module data.
 * @g:      Graph to initialise.
 * @filter: Filter name, e.g. "afftdn" or "alimiter".
 * @args:   Filter argument string, or NULL for filter defaults.
 * @fmt:    Filter sample format (e.g. AV_SAMPLE_FMT_FLTP or AV_SAMPLE_FMT_DBL).
 *
 * Return: Zero on success, negative errno otherwise.
 */
static int ffmpeg_af_open(struct processing_module *mod,
			  struct ffmpeg_af_graph *g, const char *filter,
			  const char *args, enum AVSampleFormat fmt)
{
	struct comp_dev *dev = mod->dev;
	const AVFilter *abuffer = avfilter_get_by_name("abuffer");
	const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
	const AVFilter *filt = avfilter_get_by_name(filter);
	AVFilterContext *filt_ctx = NULL;
	AVChannelLayout ch_layout = { 0 };
	char layout_str[FFMPEG_AF_LAYOUT_STR_SIZE];
	AVRational tb;
	int ret;

	/* Phase 1: Look up avfilter descriptors and validate stream parameters. */
	if (!abuffer || !abuffersink || !filt) {
		comp_err(dev, "avfilter '%s' or buffer endpoint not found", filter);
		return -ENODEV;
	}

	if (!g->rate) {
		comp_warn(dev, "rate unset, defaulting to 48000 Hz");
		g->rate = 48000;
	}
	if (!g->channels) {
		comp_warn(dev, "channels unset, defaulting to 2");
		g->channels = 2;
	}
	if (g->in_frame_bytes <= 0 || g->in_frame_bytes > 32) {
		comp_warn(dev, "in_frame_bytes %d invalid, defaulting to %zu",
			  g->in_frame_bytes, g->channels * sizeof(int16_t));
		g->in_frame_bytes = g->channels * sizeof(int16_t);
	}
	if (g->out_frame_bytes <= 0 || g->out_frame_bytes > 32) {
		comp_warn(dev, "out_frame_bytes %d invalid, defaulting to %zu",
			  g->out_frame_bytes, g->channels * sizeof(int16_t));
		g->out_frame_bytes = g->channels * sizeof(int16_t);
	}
	g->frame_size = g->rate / 80;
	if (g->frame_size <= 0) {
		comp_warn(dev, "frame_size %d invalid, defaulting to 600", g->frame_size);
		g->frame_size = 600;
	}

	av_log_set_callback(ffmpeg_af_quiet_log);
	av_log_set_level(AV_LOG_QUIET);

	/* Phase 2: Allocate the filter graph and configure the abuffer source node. */
	ffmpeg_af_close(mod, g);
	g->graph = avfilter_graph_alloc();
	if (!g->graph)
		return -ENOMEM;

	g->src = avfilter_graph_alloc_filter(g->graph, abuffer, "in");
	if (!g->src) {
		ret = -ENOMEM;
		goto err;
	}

	tb.num = 1;
	tb.den = g->rate;
	av_opt_set_q(g->src, "time_base", tb, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(g->src, "sample_rate", g->rate, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_sample_fmt(g->src, "sample_fmt", fmt, AV_OPT_SEARCH_CHILDREN);
	av_channel_layout_default(&ch_layout, g->channels);
	av_channel_layout_describe(&ch_layout, layout_str, FFMPEG_AF_LAYOUT_STR_SIZE);
	av_opt_set(g->src, "channel_layout", layout_str, AV_OPT_SEARCH_CHILDREN);

	ret = avfilter_init_dict(g->src, NULL);
	if (ret < 0) {
		comp_err(dev, "abuffer init failed %d", ret);
		av_channel_layout_uninit(&ch_layout);
		ret = -EIO;
		goto err;
	}

	/* Phase 3: Instantiate the processing filter node and abuffersink output node. */
	ret = avfilter_graph_create_filter(&filt_ctx, filt, "filter",
					   args, NULL, g->graph);
	if (ret < 0) {
		comp_err(dev, "create filter '%s' failed %d", filter, ret);
		av_channel_layout_uninit(&ch_layout);
		ret = -EIO;
		goto err;
	}

	ret = avfilter_graph_create_filter(&g->sink, abuffersink, "out", NULL, NULL, g->graph);
	if (ret < 0) {
		comp_err(dev, "create abuffersink failed %d", ret);
		av_channel_layout_uninit(&ch_layout);
		ret = -EIO;
		goto err;
	}
	av_opt_set(g->sink, "ch_layouts", layout_str, AV_OPT_SEARCH_CHILDREN);
	av_channel_layout_uninit(&ch_layout);

	/* Phase 4: Link abuffer -> filter -> abuffersink and negotiate graph formats. */
	ret = avfilter_link(g->src, 0, filt_ctx, 0);
	if (!ret)
		ret = avfilter_link(filt_ctx, 0, g->sink, 0);
	if (ret < 0) {
		comp_err(dev, "avfilter_link failed %d", ret);
		ret = -EIO;
		goto err;
	}

	ret = avfilter_graph_config(g->graph, NULL);
	if (ret < 0) {
		comp_err(dev, "avfilter_graph_config failed %d", ret);
		ret = -EIO;
		goto err;
	}

	/* Phase 5: Allocate persistent AVFrames and linear PCM staging buffers. */
	g->in_frame = av_frame_alloc();
	g->out_frame = av_frame_alloc();
	if (!g->in_frame || !g->out_frame) {
		ret = -ENOMEM;
		goto err;
	}

	g->in_frame->format = fmt;
	g->in_frame->nb_samples = g->frame_size;
	g->in_frame->sample_rate = g->rate;
	av_channel_layout_default(&g->in_frame->ch_layout, g->channels);
	ret = av_frame_get_buffer(g->in_frame, 0);
	if (ret < 0) {
		comp_err(dev, "av_frame_get_buffer failed %d", ret);
		ret = -ENOMEM;
		goto err;
	}

	/* Allocate persistent staging buffers for 4 blocks to absorb filter latency. */
	g->pcm_in_cap = (size_t)g->frame_size * 4;
	g->pcm_out_cap = (size_t)g->frame_size * 4;
	g->pcm_in = mod_alloc(mod, g->pcm_in_cap * (size_t)g->in_frame_bytes);
	g->pcm_out = mod_alloc(mod, g->pcm_out_cap * (size_t)g->out_frame_bytes);
	if (!g->pcm_in || !g->pcm_out) {
		ret = -ENOMEM;
		goto err;
	}

	g->pcm_in_count = 0;
	g->pcm_out_count = 0;
	g->pcm_out_rd = 0;
	g->eof_sent = false;
	g->eof_flushed = false;

	comp_info(dev, "ffmpeg_af: '%s' open OK, rate %d ch %d frame_size %d",
		  filter, g->rate, g->channels, g->frame_size);
	return 0;

err:
	ffmpeg_af_close(mod, g);
	return ret;
}

static void ffmpeg_af_store_out_frame(struct ffmpeg_af_graph *g,
				      const AVFrame *frame)
{
	size_t avail_bytes;
	uint8_t *dst;
	int ret;

	if (g->pcm_out_rd > 0) {
		if (g->pcm_out_count > 0) {
			memmove(g->pcm_out,
				g->pcm_out + g->pcm_out_rd * (size_t)g->out_frame_bytes,
				g->pcm_out_count * (size_t)g->out_frame_bytes);
		}
		g->pcm_out_rd = 0;
	}

	if (g->pcm_out_count >= g->pcm_out_cap)
		return;

	dst = g->pcm_out + g->pcm_out_count * (size_t)g->out_frame_bytes;
	avail_bytes = (g->pcm_out_cap - g->pcm_out_count) * (size_t)g->out_frame_bytes;
	ret = ffmpeg_dec_convert_frame(frame, dst, avail_bytes,
				       g->out_frame_fmt, g->channels);
	if (ret > 0)
		g->pcm_out_count += (size_t)ret / (size_t)g->out_frame_bytes;
}

static int ffmpeg_af_pull_output(struct processing_module *mod,
				 struct ffmpeg_af_graph *g)
{
	struct comp_dev *dev = mod->dev;
	int ret;

	while ((int)(g->pcm_out_cap - g->pcm_out_count) >= g->frame_size) {
		ret = av_buffersink_get_samples(g->sink, g->out_frame, g->frame_size);
		if (ret == AVERROR(EAGAIN))
			break;
		if (ret == AVERROR_EOF) {
			g->eof_flushed = true;
			break;
		}
		if (ret < 0) {
			comp_err(dev, "av_buffersink_get_samples failed %d", ret);
			return -EIO;
		}

		ffmpeg_af_store_out_frame(g, g->out_frame);
		av_frame_unref(g->out_frame);
	}

	return 0;
}

static int ffmpeg_af_submit_input(struct processing_module *mod,
				  struct ffmpeg_af_graph *g, int nb_samples)
{
	struct comp_dev *dev = mod->dev;
	int ret;

	ret = av_frame_make_writable(g->in_frame);
	if (ret < 0) {
		comp_err(dev, "av_frame_make_writable failed %d", ret);
		return -EIO;
	}

	g->in_frame->nb_samples = nb_samples;
	ret = ffmpeg_dec_pcm_to_frame(g->pcm_in, g->in_frame,
				      g->in_frame_fmt, g->channels, nb_samples);
	if (ret < 0) {
		g->in_frame->nb_samples = g->frame_size;
		comp_err(dev, "ffmpeg_dec_pcm_to_frame failed %d", ret);
		return ret;
	}

	if (g->pcm_in_count > (size_t)nb_samples) {
		memmove(g->pcm_in,
			g->pcm_in + (size_t)nb_samples * (size_t)g->in_frame_bytes,
			(g->pcm_in_count - (size_t)nb_samples) *
			(size_t)g->in_frame_bytes);
	}
	g->pcm_in_count -= (size_t)nb_samples;

	ret = av_buffersrc_add_frame_flags(g->src, g->in_frame,
					   AV_BUFFERSRC_FLAG_KEEP_REF);
	g->in_frame->nb_samples = g->frame_size;
	if (ret < 0) {
		comp_err(dev, "av_buffersrc_add_frame_flags failed %d", ret);
		return -EIO;
	}

	return ffmpeg_af_pull_output(mod, g);
}

static void ffmpeg_af_drain_sink(struct ffmpeg_af_graph *g, struct sof_sink *sink)
{
	size_t free_bytes, req_bytes;
	int free_frames, to_drain;
	void *dst, *buf_start;
	size_t buf_size;
	int ret;

	if (g->pcm_out_count == 0)
		return;

	free_bytes = sink_get_free_size(sink);
	free_frames = (int)(free_bytes / (size_t)g->out_frame_bytes);
	to_drain = MIN((int)g->pcm_out_count, free_frames);
	if (to_drain <= 0)
		return;

	req_bytes = (size_t)to_drain * (size_t)g->out_frame_bytes;
	ret = sink_get_buffer(sink, req_bytes, &dst, &buf_start, &buf_size);
	if (ret == 0) {
		ffmpeg_dec_copy_to_circular(dst, buf_start, buf_size,
					    g->pcm_out + g->pcm_out_rd * (size_t)g->out_frame_bytes,
					    req_bytes);
		sink_commit_buffer(sink, req_bytes);
		g->pcm_out_rd += (size_t)to_drain;
		g->pcm_out_count -= (size_t)to_drain;
		if (g->pcm_out_count == 0)
			g->pcm_out_rd = 0;
	}
}

int ffmpeg_af_mod_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_dec_comp_data *cd;
	struct ffmpeg_af_graph *g;
	const struct ffmpeg_af_filter_desc *desc;

	ffmpeg_dec_libc_bind(mod);
	comp_info(dev, "entry");

	if (dev->ipc_config.proc_domain != COMP_PROCESSING_DOMAIN_DP) {
		comp_err(dev, "ffmpeg_af requires DP processing domain");
		return -EINVAL;
	}

	desc = ffmpeg_af_default_filter();
	if (!desc) {
		comp_err(dev, "no FFmpeg audio filters enabled in build");
		return -ENOTSUP;
	}

	if (mod->priv.cfg.ext_data &&
	    mod->priv.cfg.ext_data->module_data &&
	    mod->priv.cfg.ext_data->module_data_size >= sizeof(struct snd_codec)) {
		const struct snd_codec *codec_params = mod->priv.cfg.ext_data->module_data;

		if (codec_params->profile) {
			const struct ffmpeg_af_filter_desc *req_desc =
				ffmpeg_af_find_by_id(codec_params->profile);

			if (!req_desc) {
				comp_err(dev, "unsupported or disabled filter profile %u",
					 codec_params->profile);
				return -ENOTSUP;
			}
			desc = req_desc;
		}
	}

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;
	cd->is_filter = true;
	md->private = cd;

	md->mpd.in_buff_size = 9600;
	md->mpd.out_buff_size = 9600;

	g = mod_zalloc(mod, sizeof(*g));
	if (!g) {
		mod_free(mod, cd);
		md->private = NULL;
		return -ENOMEM;
	}

	g->desc = desc;
	snprintf(g->filter_name, sizeof(g->filter_name), "%s", desc->name);
	snprintf(g->filter_args, sizeof(g->filter_args), "%s", desc->default_args);
	cd->backend_data = g;

	cd->eos_msg = ffmpeg_dec_eos_notification_init(mod);
	if (!cd->eos_msg) {
		comp_err(dev, "failed to allocate EOS notification");
		mod_free(mod, g);
		mod_free(mod, cd);
		md->private = NULL;
		return -ENOMEM;
	}

	return 0;
}

int ffmpeg_af_mod_set_config(struct processing_module *mod,
			     const uint8_t *data, size_t size)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_af_graph *g;
	char buf[FFMPEG_AF_MAX_NAME + FFMPEG_AF_MAX_ARGS];
	const struct ffmpeg_af_filter_desc *desc;
	char *colon;
	size_t len;

	if (!cd || !cd->backend_data || !data || !size)
		return 0;

	g = cd->backend_data;
	if (dev->state == COMP_STATE_ACTIVE || cd->configured || g->graph) {
		comp_err(dev, "cannot reconfigure filter while active (state %d)",
			 dev->state);
		return -EBUSY;
	}

	len = MIN(size, sizeof(buf) - 1);
	memcpy_s(buf, sizeof(buf) - 1, data, len);
	buf[len] = '\0';

	colon = strchr(buf, ':');
	if (colon)
		*colon = '\0';

	desc = ffmpeg_af_find_by_name(buf);
	if (!desc) {
		comp_err(dev, "unknown or disabled filter '%s'", buf);
		return -ENOTSUP;
	}

	g->desc = desc;
	snprintf(g->filter_name, sizeof(g->filter_name), "%s", desc->name);
	if (colon && colon[1] != '\0')
		snprintf(g->filter_args, sizeof(g->filter_args), "%s", colon + 1);
	else
		snprintf(g->filter_args, sizeof(g->filter_args), "%s", desc->default_args);

	comp_info(dev, "configured filter '%s' args '%s'", g->filter_name, g->filter_args);
	return 0;
}

static int ffmpeg_af_validate_pcm_fmt(struct comp_dev *dev, int container_fmt,
				      int valid_fmt, int channels,
				      int frame_bytes, const char *dir)
{
	int expected_bps;

	switch (container_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		expected_bps = 2;
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		expected_bps = 4;
		break;
	default:
		comp_err(dev, "unsupported %s container format %d", dir, container_fmt);
		return -EINVAL;
	}

	switch (valid_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		if (expected_bps != 2) {
			comp_err(dev, "unsupported %s S16 valid format in %d-byte container",
				 dir, expected_bps);
			return -EINVAL;
		}
		break;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		if (expected_bps != 4) {
			comp_err(dev, "unsupported %s valid format %d in %d-byte container",
				 dir, valid_fmt, expected_bps);
			return -EINVAL;
		}
		break;
	default:
		comp_err(dev, "unsupported %s valid sample format %d", dir, valid_fmt);
		return -EINVAL;
	}

	if (channels <= 0 || frame_bytes != channels * expected_bps) {
		comp_err(dev, "invalid %s frame_bytes %d for %d ch (%d B/sample)",
			 dir, frame_bytes, channels, expected_bps);
		return -EINVAL;
	}

	return 0;
}

int ffmpeg_af_mod_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct ffmpeg_af_graph *g;
	struct sof_source *source;
	struct sof_sink *sink;
	int in_container_fmt, out_container_fmt;
	int ret;

	if (!cd || !cd->backend_data)
		return -EINVAL;
	if (num_of_sources < 1 || !sources[0] || num_of_sinks < 1 || !sinks[0]) {
		comp_err(dev, "missing source or sink");
		return -EINVAL;
	}

	ffmpeg_dec_libc_bind(mod);
	g = cd->backend_data;
	source = sources[0];
	sink = sinks[0];

	g->rate = sink_get_rate(sink);
	g->channels = sink_get_channels(sink);
	out_container_fmt = sink_get_frm_fmt(sink);
	g->out_frame_fmt = sink_get_valid_fmt(sink);
	g->out_frame_bytes = sink_get_frame_bytes(sink);

	in_container_fmt = source_get_frm_fmt(source);
	g->in_frame_fmt = source_get_valid_fmt(source);
	g->in_frame_bytes = source_get_frame_bytes(source);

	if (source_get_rate(source) && g->rate &&
	    source_get_rate(source) != (uint32_t)g->rate) {
		comp_err(dev, "source rate %u != sink rate %d",
			 source_get_rate(source), g->rate);
		return -EINVAL;
	}
	if (source_get_channels(source) && g->channels &&
	    source_get_channels(source) != (uint32_t)g->channels) {
		comp_err(dev, "source channels %u != sink channels %d",
			 source_get_channels(source), g->channels);
		return -EINVAL;
	}

	ret = ffmpeg_af_validate_pcm_fmt(dev, in_container_fmt, g->in_frame_fmt,
					 g->channels, g->in_frame_bytes, "input");
	if (ret < 0)
		return ret;

	ret = ffmpeg_af_validate_pcm_fmt(dev, out_container_fmt, g->out_frame_fmt,
					 g->channels, g->out_frame_bytes, "output");
	if (ret < 0)
		return ret;

	cd->out_rate = g->rate;
	cd->out_channels = g->channels;
	cd->out_frame_fmt = g->out_frame_fmt;
	cd->out_frame_bytes = g->out_frame_bytes;

	comp_info(dev, "ffmpeg_af: %s, rate %d ch %d in_fmt %d (%d B) out_fmt %d (%d B)",
		  g->filter_name, g->rate, g->channels,
		  g->in_frame_fmt, g->in_frame_bytes,
		  g->out_frame_fmt, g->out_frame_bytes);

	/*
	 * NB: Lazy open deferred to DP thread.
	 * prepare() is dispatched across cores via IDC and runs on the IDC
	 * worker thread with a small stack. Filter open (avfilter_graph_config /
	 * afftdn table building) requires a deep stack and takes tens of ms.
	 * Defer ffmpeg_af_open() to the first process() call on the module's
	 * own DP thread.
	 */
	cd->configured = false;
	cd->eos_sent = false;
	g->eof_sent = false;
	g->eof_flushed = false;

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
	size_t avail_bytes, req_bytes;
	int avail_frames, space_frames, to_read;
	const void *sp, *sstart;
	size_t sbytes;
	int ret;
	enum sof_audio_buffer_state src_state;
	bool host_eos, src_exhausted;

	ffmpeg_dec_libc_bind(mod);
	if (!cd || !cd->backend_data)
		return -EINVAL;

	g = cd->backend_data;

	/* Lazy open: runs once on the module's DP thread with deep stack. */
	if (!cd->configured) {
		enum AVSampleFormat fmt = g->desc ? g->desc->sample_fmt : AV_SAMPLE_FMT_FLTP;

		ret = ffmpeg_af_open(mod, g, g->filter_name, g->filter_args, fmt);
		if (ret < 0) {
			comp_err(dev, "ffmpeg_af_open failed %d", ret);
			return ret;
		}
		cd->configured = true;
		return 0;	/* proceed to process on next cycle */
	}

	if (num_of_sources < 1 || num_of_sinks < 1)
		return -EINVAL;

	source = sources[0];
	sink = sinks[0];
	src_state = source_get_state(source);
	host_eos = src_state == AUDIOBUF_STATE_END_OF_STREAM ||
		   src_state == AUDIOBUF_STATE_END_OF_STREAM_FLUSH;

	/* 1. Drain previously staged filtered samples into the sink if space is available. */
	ffmpeg_af_drain_sink(g, sink);

	/* 2. Read new input samples from source into pcm_in staging buffer. */
	avail_bytes = source_get_data_available(source);
	space_frames = (int)(g->pcm_in_cap - g->pcm_in_count);
	avail_frames = (int)(avail_bytes / (size_t)g->in_frame_bytes);
	to_read = MIN(avail_frames, space_frames);
	if (to_read > 0) {
		req_bytes = (size_t)to_read * (size_t)g->in_frame_bytes;
		ret = source_get_data(source, req_bytes, &sp, &sstart, &sbytes);
		if (ret == 0) {
			ffmpeg_dec_copy_from_circular(g->pcm_in +
						      g->pcm_in_count * (size_t)g->in_frame_bytes,
						      sp, sstart, sbytes, req_bytes);
			source_release_data(source, req_bytes);
			g->pcm_in_count += (size_t)to_read;
		}
	}

	src_exhausted = host_eos &&
			source_get_data_available(source) < (size_t)g->in_frame_bytes;

	/* 3. Process at most one filter frame per cycle to yield to scheduler and IDC. */
	if ((int)g->pcm_in_count >= g->frame_size &&
	    (int)(g->pcm_out_cap - g->pcm_out_count) >= g->frame_size) {
		ret = ffmpeg_af_submit_input(mod, g, g->frame_size);
		if (ret < 0)
			return ret;
	} else if (src_exhausted && g->pcm_in_count > 0 &&
		   (g->pcm_out_cap - g->pcm_out_count) >= g->pcm_in_count) {
		/* Flush remaining partial input block (< frame_size) at EOS. */
		ret = ffmpeg_af_submit_input(mod, g, (int)g->pcm_in_count);
		if (ret < 0)
			return ret;
	}

	/* 4. At EOS after all input is submitted, send NULL EOF frame and drain graph. */
	if (src_exhausted && g->pcm_in_count == 0 && !g->eof_flushed &&
	    (int)(g->pcm_out_cap - g->pcm_out_count) >= g->frame_size) {
		if (!g->eof_sent) {
			ret = av_buffersrc_add_frame_flags(g->src, NULL, 0);
			if (ret < 0) {
				comp_err(dev, "av_buffersrc EOF flush failed %d", ret);
				return -EIO;
			}
			g->eof_sent = true;
		}
		ret = ffmpeg_af_pull_output(mod, g);
		if (ret < 0)
			return ret;
	}

	/* 5. Drain any freshly produced samples to the sink. */
	ffmpeg_af_drain_sink(g, sink);

	/* 6. Signal EOS once source, filter graph, and output staging are fully drained. */
	if (src_exhausted && g->pcm_in_count == 0 && g->eof_flushed &&
	    g->pcm_out_count == 0 && !cd->eos_sent)
		ffmpeg_dec_signal_eos(mod, sink);

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

	/* Run lazy open on DP thread if not configured yet. */
	if (!cd->configured)
		return true;

	g = cd->backend_data;
	if (!g)
		return false;

	if (cd->eos_sent && g->pcm_out_count == 0)
		return false;

	if (g->pcm_out_count > 0 &&
	    sink_get_free_size(sinks[0]) >= (size_t)g->out_frame_bytes)
		return true;

	if (source_get_data_available(sources[0]) >= (size_t)g->in_frame_bytes &&
	    g->pcm_in_count < g->pcm_in_cap)
		return true;

	if ((int)g->pcm_in_count >= g->frame_size &&
	    (int)(g->pcm_out_cap - g->pcm_out_count) >= g->frame_size)
		return true;

	if (!cd->eos_sent) {
		enum sof_audio_buffer_state src_state = source_get_state(sources[0]);
		bool host_eos = src_state == AUDIOBUF_STATE_END_OF_STREAM ||
				src_state == AUDIOBUF_STATE_END_OF_STREAM_FLUSH;

		if (host_eos &&
		    source_get_data_available(sources[0]) < (size_t)g->in_frame_bytes) {
			if (g->pcm_in_count > 0 &&
			    (g->pcm_out_cap - g->pcm_out_count) >= g->pcm_in_count)
				return true;
			if (g->pcm_in_count == 0 && !g->eof_flushed &&
			    (int)(g->pcm_out_cap - g->pcm_out_count) >= g->frame_size)
				return true;
			if (g->pcm_in_count == 0 && g->eof_flushed &&
			    g->pcm_out_count == 0)
				return true;
		}
	}

	return false;
}

int ffmpeg_af_mod_reset(struct processing_module *mod)
{
	struct ffmpeg_dec_comp_data *cd = module_get_private_data(mod);
	struct ffmpeg_af_graph *g;

	ffmpeg_dec_libc_bind(mod);
	if (!cd || !cd->backend_data)
		return 0;

	g = cd->backend_data;
	ffmpeg_af_close(mod, g);
	cd->configured = false;
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

	if (cd->eos_msg) {
		mod_ipc_msg_free(mod, cd->eos_msg);
		cd->eos_msg = NULL;
	}

	if (cd->backend_data) {
		ffmpeg_af_close(mod, cd->backend_data);
		mod_free(mod, cd->backend_data);
		cd->backend_data = NULL;
	}
	mod_free(mod, cd);
	md->private = NULL;
	return 0;
}
