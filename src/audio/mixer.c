// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/mixer.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/base-config.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_mixer;

#if CONFIG_IPC_MAJOR_3
/* bc06c037-12aa-417c-9a97-89282e321a76 */
DECLARE_SOF_RT_UUID("mixer", mixer_uuid, 0xbc06c037, 0x12aa, 0x417c,
		 0x9a, 0x97, 0x89, 0x28, 0x2e, 0x32, 0x1a, 0x76);
#else
/* mixout 3c56505a-24d7-418f-bddc-c1f5a3ac2ae0 */
DECLARE_SOF_RT_UUID("mixer", mixer_uuid, 0x3c56505a, 0x24d7, 0x418f,
		    0xbd, 0xdc, 0xc1, 0xf5, 0xa3, 0xac, 0x2a, 0xe0);

/* mixin 39656eb2-3b71-4049-8d3f-f92cd5c43c09 */
DECLARE_SOF_RT_UUID("mix_in", mixin_uuid, 0x39656eb2, 0x3b71, 0x4049,
		    0x8d, 0x3f, 0xf9, 0x2c, 0xd5, 0xc4, 0x3c, 0x09);
DECLARE_TR_CTX(mixin_tr, SOF_UUID(mixin_uuid), LOG_LEVEL_INFO);
#endif

DECLARE_TR_CTX(mixer_tr, SOF_UUID(mixer_uuid), LOG_LEVEL_INFO);

/* mixer component private data */
struct mixer_data {
#if CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base_cfg;
#endif

	bool sources_inactive;

	void (*mix_func)(struct comp_dev *dev, struct audio_stream *sink,
			 const struct audio_stream **sources, uint32_t count,
			 uint32_t frames);
};

#if CONFIG_FORMAT_S16LE
/* Mix n 16 bit PCM source streams to one sink stream */
static void mix_n_s16(struct comp_dev *dev, struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t num_sources,
		      uint32_t frames)
{
	int16_t *src[PLATFORM_MAX_CHANNELS];
	int16_t *dest;
	int32_t val;
	int nmax;
	int i, j, n, ns;
	int processed = 0;
	int nch = sink->channels;
	int samples = frames * nch;

	dest = sink->w_ptr;
	for (j = 0; j < num_sources; j++)
		src[j] = sources[j]->r_ptr;

	while (processed < samples) {
		nmax = samples - processed;
		n = audio_stream_bytes_without_wrap(sink, dest) >> 1; /* divide 2 */
		n = MIN(n, nmax);
		for (i = 0; i < num_sources; i++) {
			ns = audio_stream_bytes_without_wrap(sources[i], src[i]) >> 1;
			n = MIN(n, ns);
		}
		for (i = 0; i < n; i++) {
			val = 0;
			for (j = 0; j < num_sources; j++) {
				val += *src[j];
				src[j]++;
			}

			/* Saturate to 16 bits */
			*dest = sat_int16(val);
			dest++;
		}
		processed += n;
		dest = audio_stream_wrap(sink, dest);
		for (i = 0; i < num_sources; i++)
			src[i] = audio_stream_wrap(sources[i], src[i]);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
/* Mix n 24 bit PCM source streams to one sink stream */
static void mix_n_s24(struct comp_dev *dev, struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t num_sources,
		      uint32_t frames)
{
	int32_t *src[PLATFORM_MAX_CHANNELS];
	int32_t *dest;
	int32_t val;
	int32_t x;
	int nmax;
	int i, j, n, ns;
	int processed = 0;
	int nch = sink->channels;
	int samples = frames * nch;

	dest = sink->w_ptr;
	for (j = 0; j < num_sources; j++)
		src[j] = sources[j]->r_ptr;

	while (processed < samples) {
		nmax = samples - processed;
		n = audio_stream_bytes_without_wrap(sink, dest) >> 2; /* divide 4 */
		n = MIN(n, nmax);
		for (i = 0; i < num_sources; i++) {
			ns = audio_stream_bytes_without_wrap(sources[i], src[i]) >> 2;
			n = MIN(n, ns);
		}
		for (i = 0; i < n; i++) {
			val = 0;
			for (j = 0; j < num_sources; j++) {
				x = *src[j] << 8;
				val += x >> 8; /* Sign extend */
				src[j]++;
			}

			/* Saturate to 24 bits */
			*dest = sat_int24(val);
			dest++;
		}
		processed += n;
		dest = audio_stream_wrap(sink, dest);
		for (i = 0; i < num_sources; i++)
			src[i] = audio_stream_wrap(sources[i], src[i]);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
/* Mix n 32 bit PCM source streams to one sink stream */
static void mix_n_s32(struct comp_dev *dev, struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t num_sources,
		      uint32_t frames)
{
	int32_t *src[PLATFORM_MAX_CHANNELS];
	int32_t *dest;
	int64_t val;
	int nmax;
	int i, j, n, ns;
	int processed = 0;
	int nch = sink->channels;
	int samples = frames * nch;

	dest = sink->w_ptr;
	for (j = 0; j < num_sources; j++)
		src[j] = sources[j]->r_ptr;

	while (processed < samples) {
		nmax = samples - processed;
		n = audio_stream_bytes_without_wrap(sink, dest) >> 2; /* divide 4 */
		n = MIN(n, nmax);
		for (i = 0; i < num_sources; i++) {
			ns = audio_stream_bytes_without_wrap(sources[i], src[i]) >> 2;
			n = MIN(n, ns);
		}
		for (i = 0; i < n; i++) {
			val = 0;
			for (j = 0; j < num_sources; j++) {
				val += *src[j];
				src[j]++;
			}

			/* Saturate to 32 bits */
			*dest = sat_int32(val);
			dest++;
		}
		processed += n;
		dest = audio_stream_wrap(sink, dest);
		for (i = 0; i < num_sources; i++)
			src[i] = audio_stream_wrap(sources[i], src[i]);
	}
}
#endif /* CONFIG_FORMAT_S32LE */

#if CONFIG_IPC_MAJOR_3
static struct comp_dev *mixer_new(const struct comp_driver *drv,
				  struct comp_ipc_config *config,
				  void *spec)
{
	struct comp_dev *dev;
	struct mixer_data *md;

	comp_cl_dbg(&comp_mixer, "mixer_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;
	dev->ipc_config = *config;

	md = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*md));
	if (!md) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, md);
	dev->state = COMP_STATE_READY;
	return dev;
}
#endif

static void mixer_free(struct comp_dev *dev)
{
	struct mixer_data *md = comp_get_drvdata(dev);

	comp_dbg(dev, "mixer_free()");

	rfree(md);
	rfree(dev);
}

static int mixer_verify_params(struct comp_dev *dev,
			       struct sof_ipc_stream_params *params)
{
	int ret;

	comp_dbg(dev, "mixer_verify_params()");

	ret = comp_verify_params(dev, BUFF_PARAMS_CHANNELS, params);
	if (ret < 0) {
		comp_err(dev, "mixer_verify_params(): comp_verify_params() failed.");
		return ret;
	}

	return 0;
}
/* set component audio stream parameters */
static int mixer_params(struct comp_dev *dev,
			struct sof_ipc_stream_params *params)
{
	struct comp_buffer *sinkb;
	uint32_t sink_period_bytes;
	int err;

	comp_dbg(dev, "mixer_params()");

	err = mixer_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "mixer_params(): pcm params verification failed.");
		return -EINVAL;
	}

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);

	/* calculate period size based on config */
	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      dev->frames);
	if (sink_period_bytes == 0) {
		comp_err(dev, "mixer_params(): period_bytes = 0");
		return -EINVAL;
	}

	if (sinkb->stream.size < sink_period_bytes) {
		comp_err(dev, "mixer_params(): sink buffer size %d is insufficient < %d",
			 sinkb->stream.size, sink_period_bytes);
		return -ENOMEM;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int mixer_trigger_common(struct comp_dev *dev, int cmd)
{
	int ret;

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return ret;
}

/*
 * Mix N source PCM streams to one sink PCM stream. Frames copied is constant.
 */
static int mixer_copy(struct comp_dev *dev)
{
	struct mixer_data *md = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	struct comp_buffer *sources[PLATFORM_MAX_STREAMS];
	const struct audio_stream *sources_stream[PLATFORM_MAX_STREAMS];
	struct comp_buffer *source;
	struct list_item *blist;
	int32_t i = 0;
	int32_t num_mix_sources = 0;
	uint32_t frames = INT32_MAX;
	uint32_t source_bytes;
	uint32_t sink_bytes;

	comp_dbg(dev, "mixer_copy()");

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* calculate the highest runtime component status
	 * between input streams
	 */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);

		/* only mix the sources with the same state with mixer */
		if (source->source->state == dev->state) {
			sources[num_mix_sources] = source;
			sources_stream[num_mix_sources] = &source->stream;
			num_mix_sources++;
		}

		/* too many sources ? */
		if (num_mix_sources == PLATFORM_MAX_STREAMS - 1)
			return 0;
	}

	sink = buffer_acquire(sink);

	/* check for underruns */
	for (i = 0; i < num_mix_sources; i++) {
		uint32_t avail_frames;
		sources[i] = buffer_acquire(sources[i]);
		avail_frames = audio_stream_avail_frames(sources_stream[i],
							 &sink->stream);
		frames = MIN(frames, avail_frames);
		sources[i] = buffer_release(sources[i]);
	}

	if (!num_mix_sources || (frames == 0 && md->sources_inactive)) {
		/*
		 * Generate silence when sources are inactive. When
		 * sources change to active, additionally keep
		 * generating silence until at least one of the
		 * sources start to have data available (frames!=0).
		 */
		frames = audio_stream_get_free_frames(&sink->stream);
		sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);
		sink = buffer_release(sink);
		if (!audio_stream_set_zero(&sink->stream, sink_bytes)) {
			buffer_stream_writeback(sink, sink_bytes);
			comp_update_buffer_produce(sink, sink_bytes);
		}

		md->sources_inactive = true;

		return 0;
	}

	if (md->sources_inactive) {
		md->sources_inactive = false;
		comp_dbg(dev, "mixer_copy exit sources_inactive state");
	}

	sink = buffer_release(sink);

	/* Every source has the same format, so calculate bytes based
	 * on the first one.
	 */
	source_bytes = frames * audio_stream_frame_bytes(sources_stream[0]);
	sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);

	comp_dbg(dev, "mixer_copy(), source_bytes = 0x%x, sink_bytes = 0x%x",
		 source_bytes, sink_bytes);

	/* mix streams */
	for (i = num_mix_sources - 1; i >= 0; i--)
		buffer_stream_invalidate(sources[i], source_bytes);
	md->mix_func(dev, &sink->stream, sources_stream, num_mix_sources,
		     frames);
	buffer_stream_writeback(sink, sink_bytes);

	/* update source buffer pointers */
	for (i = num_mix_sources - 1; i >= 0; i--)
		comp_update_buffer_consume(sources[i], source_bytes);

	/* update sink buffer pointer */
	comp_update_buffer_produce(sink, sink_bytes);

	return 0;
}

static inline bool mixer_stop_reset(struct comp_dev *dev, struct comp_dev *source);
static int mixer_reset(struct comp_dev *dev)
{
	int dir = dev->pipeline->source_comp->direction;
	struct list_item *blist;
	struct comp_buffer *source;

	comp_dbg(dev, "mixer_reset()");

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		list_for_item(blist, &dev->bsource_list) {
			source = container_of(blist, struct comp_buffer,
					      sink_list);
			/* only mix the sources with the same state with mixer*/
			if (mixer_stop_reset(dev, source->source))
				/* should not reset the downstream components */
				return PPL_STATUS_PATH_STOP;
		}
	}

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}

/*
 * Prepare the mixer. The mixer may already be running at this point with other
 * sources. Make sure we only prepare the "prepared" source streams and not
 * the active or inactive sources.
 *
 * We should also make sure that we propagate the prepare call to downstream
 * if downstream is not currently active.
 */
static int mixer_prepare_common(struct comp_dev *dev)
{
	struct mixer_data *md = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	int ret;

	comp_dbg(dev, "mixer_prepare()");

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);

	/* does mixer already have active source streams ? */
	if (dev->state != COMP_STATE_ACTIVE) {
		/* currently inactive so setup mixer */
		switch (sink->stream.frame_fmt) {
#if CONFIG_FORMAT_S16LE
		case SOF_IPC_FRAME_S16_LE:
			md->mix_func = mix_n_s16;
			break;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
		case SOF_IPC_FRAME_S24_4LE:
			md->mix_func = mix_n_s24;
			break;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
		case SOF_IPC_FRAME_S32_LE:
			md->mix_func = mix_n_s32;
			break;
#endif /* CONFIG_FORMAT_S32LE */
		default:
			comp_err(dev, "unsupported data format");
			return -EINVAL;
		}

		ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
		if (ret < 0)
			return ret;

		if (ret == COMP_STATUS_STATE_ALREADY_SET)
			return PPL_STATUS_PATH_STOP;
	}

	return 0;
}

/* In IPC3 the simplest pipeline with mixer will like
 * host1 -> mixer -> volume -> dai, pipeline 1.
 *           |
 * host2 ---- pipeline 2
 *
 * For IPC4, it will be like
 * copier1(host) -> mixin1 ----> mixout(mixer) -> gain(volume) -> copier2(dai)
 *    pipeline 1                   |       pipline 2
 * copier3(host) -> mixin2 -------  pipeline 3
 *
 * mixin and mixout will be in different pipelines.
 * Now we combine mixin and mixout into mixer, but
 * the pipelines count is unchange. Ipc4 pipeline
 * can't be stopped at mixer component since pipeline
 * design is different, or gain and copier2 will not
 * be triggered. Since mixer will be in a different
 * pipeline other than host pipeline, it will not be
 * triggered second time.
 */
#if CONFIG_IPC_MAJOR_3
static int mixer_source_status_count(struct comp_dev *mixer, uint32_t status)
{
	struct comp_buffer *source;
	struct list_item *blist;
	int count = 0;

	/* count source with state == status */
	list_for_item(blist, &mixer->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		if (source->source->state == status)
			count++;
	}

	return count;
}

static int mixer_trigger(struct comp_dev *dev, int cmd)
{
	int dir = dev->pipeline->source_comp->direction;
	int ret;

	comp_dbg(dev, "mixer_trigger()");

	if (dir == SOF_IPC_STREAM_PLAYBACK && cmd == COMP_TRIGGER_PRE_START)
		/* Mixer and downstream components might or might not be active */
		if (mixer_source_status_count(dev, COMP_STATE_ACTIVE) ||
		    mixer_source_status_count(dev, COMP_STATE_PAUSED))
			return PPL_STATUS_PATH_STOP;

	/*
	 * This works around an unclear and apparently needlessly complicated
	 * mixer state machine.
	 */
	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		switch (cmd) {
		case COMP_TRIGGER_PRE_RELEASE:
			/* Mixer and everything downstream is active */
			dev->state = COMP_STATE_PRE_ACTIVE;
			break;
		case COMP_TRIGGER_RELEASE:
			/* Mixer and everything downstream is active */
			dev->state = COMP_STATE_ACTIVE;
			break;
		default:
			break;
		}

		comp_writeback(dev);
	}

	ret = mixer_trigger_common(dev, cmd);
	if (ret < 0)
		return ret;

	/* don't stop mixer on pause or if at least one source is active/paused */
	if (cmd == COMP_TRIGGER_PAUSE ||
	    (cmd == COMP_TRIGGER_STOP &&
	     (mixer_source_status_count(dev, COMP_STATE_ACTIVE) ||
	     mixer_source_status_count(dev, COMP_STATE_PAUSED)))) {
		dev->state = COMP_STATE_ACTIVE;
		comp_writeback(dev);
		ret = PPL_STATUS_PATH_STOP;
	}

	return ret;
}

static inline bool mixer_stop_reset(struct comp_dev *dev, struct comp_dev *source)
{
	return source->state > COMP_STATE_READY;
}

static int mixer_prepare(struct comp_dev *dev)
{
	struct comp_buffer *source;
	struct list_item *blist;
	int ret;

	ret = mixer_prepare_common(dev);
	if (ret)
		return ret;

	/* check each mixer source state */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);

		/* only prepare downstream if we have no active sources */
		if (source->source->state == COMP_STATE_PAUSED ||
		    source->source->state == COMP_STATE_ACTIVE) {
			return PPL_STATUS_PATH_STOP;
		}
	}

	/* prepare downstream */
	return 0;
}

static const struct comp_driver comp_mixer = {
	.type	= SOF_COMP_MIXER,
	.uid	= SOF_RT_UUID(mixer_uuid),
	.tctx	= &mixer_tr,
	.ops	= {
		.create		= mixer_new,
		.free		= mixer_free,
		.params		= mixer_params,
		.prepare	= mixer_prepare,
		.trigger	= mixer_trigger,
		.copy		= mixer_copy,
		.reset		= mixer_reset,
	},
};
#else
static struct comp_dev *mixinout_new(const struct comp_driver *drv,
				     struct comp_ipc_config *config,
				     void *spec)
{
	struct comp_dev *dev;
	struct mixer_data *md;
	enum sof_ipc_frame valid_fmt;

	comp_cl_dbg(&comp_mixer, "mixinout_new()");

	dev = comp_alloc(drv, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->ipc_config = *config;

	md = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*md));
	if (!md) {
		rfree(dev);
		return NULL;
	}

	memcpy_s(&md->base_cfg, sizeof(md->base_cfg), spec, sizeof(md->base_cfg));
	comp_set_drvdata(dev, md);

	audio_stream_fmt_conversion(md->base_cfg.audio_fmt.depth,
				    md->base_cfg.audio_fmt.valid_bit_depth,
				    &dev->ipc_config.frame_fmt, &valid_fmt,
				    md->base_cfg.audio_fmt.s_type);

	dev->state = COMP_STATE_READY;
	return dev;
}

static int mixin_trigger(struct comp_dev *dev, int cmd)
{
	int ret;

	comp_dbg(dev, "mixin_trigger()");

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return ret;
}

/* do copy in mixer component */
static int mixin_copy(struct comp_dev *dev)
{
	return 0;
}

static inline bool mixer_stop_reset(struct comp_dev *dev, struct comp_dev *source)
{
	return dev->pipeline == source->pipeline && source->state > COMP_STATE_PAUSED;
}

/* params are derived from base config for ipc4 path */
static int mixout_params(struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	struct mixer_data *md = comp_get_drvdata(dev);
	struct comp_buffer *sink;
	int i;

	memset(params, 0, sizeof(*params));
	params->channels = md->base_cfg.audio_fmt.channels_count;
	params->rate = md->base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = md->base_cfg.audio_fmt.depth;
	params->sample_valid_bytes = md->base_cfg.audio_fmt.valid_bit_depth;
	params->frame_fmt = dev->ipc_config.frame_fmt;
	params->buffer_fmt = md->base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = md->base_cfg.ibs;

	/* update each sink format based on base_cfg initialized by
	 * host driver. There is no hw_param ipc message for ipc4, instead
	 * all module params are built into module initialization data by
	 * host driver based on runtime hw_params and topology setting.
	 */
	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
	sink->stream.channels = md->base_cfg.audio_fmt.channels_count;
	sink->stream.rate = md->base_cfg.audio_fmt.sampling_frequency;
	audio_stream_fmt_conversion(md->base_cfg.audio_fmt.depth,
				    md->base_cfg.audio_fmt.valid_bit_depth,
				    &sink->stream.frame_fmt,
				    &sink->stream.valid_sample_fmt,
				    md->base_cfg.audio_fmt.s_type);

	sink->buffer_fmt = md->base_cfg.audio_fmt.interleaving_style;

	/* 8 ch stream is supported by ch_map and each channel
	 * is mapped by 4 bits. The first channel will be mapped
	 * by the bits of 0 ~ 3 and the 2th channel will be mapped
	 * by bits 4 ~ 7. The N channel will be mapped by bits
	 * N*4 ~ N*4 + 3
	 */
	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		sink->chmap[i] = (md->base_cfg.audio_fmt.ch_map >> i * 4) & 0xf;

	return mixer_params(dev, params);
}

/* the original pipeline is : buffer  -> mixin -> buffer -> mixout
 * now convert it to : buffer -> mixer
 */
static int mixin_bind(struct comp_dev *dev, void *data)
{
	struct ipc4_module_bind_unbind *bu;
	struct comp_buffer *source_buf;
	struct comp_buffer *sink_buf = NULL;
	struct comp_dev *sink;
	int src_id, sink_id;

	bu = (struct ipc4_module_bind_unbind *)data;
	src_id = IPC4_COMP_ID(bu->header.r.module_id, bu->header.r.instance_id);
	sink_id = IPC4_COMP_ID(bu->data.r.dst_module_id, bu->data.r.dst_instance_id);

	/* mixin -> mixout */
	if (dev->ipc_config.id == src_id) {
		struct list_item *blist;

		sink = ipc4_get_comp_dev(sink_id);
		if (!sink) {
			comp_err(dev, "mixin_bind: no sink with ID %d found", sink_id);
			return -EINVAL;
		}

		list_for_item(blist, &sink->bsource_list) {
			sink_buf = container_of(blist, struct comp_buffer, sink_list);
			if (sink_buf->source == dev) {
				pipeline_disconnect(sink, sink_buf, PPL_CONN_DIR_BUFFER_TO_COMP);
				break;
			}
		}

		if (!sink_buf) {
			comp_err(dev, "mixin_bind: no sink buffer found");
			return -EINVAL;
		}

		source_buf = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		pipeline_disconnect(dev, source_buf, PPL_CONN_DIR_BUFFER_TO_COMP);
		pipeline_connect(sink, source_buf, PPL_CONN_DIR_BUFFER_TO_COMP);
		pipeline_connect(dev, sink_buf, PPL_CONN_DIR_BUFFER_TO_COMP);
	}

	return 0;
}

static const struct comp_driver comp_mixin = {
	.uid	= SOF_RT_UUID(mixin_uuid),
	.tctx	= &mixer_tr,
	.ops	= {
		.create		= mixinout_new,
		.free		= mixer_free,
		.trigger	= mixin_trigger,
		.copy		= mixin_copy,
		.bind		= mixin_bind,
	},
};

static SHARED_DATA struct comp_driver_info comp_mixin_info = {
	.drv = &comp_mixin,
};

UT_STATIC void sys_comp_mixin_init(void)
{
	comp_register(platform_shared_get(&comp_mixin_info,
					  sizeof(comp_mixin_info)));
}

DECLARE_MODULE(sys_comp_mixin_init);

static const struct comp_driver comp_mixer = {
	.type	= SOF_COMP_MIXER,
	.uid	= SOF_RT_UUID(mixer_uuid),
	.tctx	= &mixer_tr,
	.ops	= {
		.create		= mixinout_new,
		.free		= mixer_free,
		.params		= mixout_params,
		.prepare	= mixer_prepare_common,
		.trigger	= mixer_trigger_common,
		.copy		= mixer_copy,
		.reset		= mixer_reset,
	},
};
#endif

static SHARED_DATA struct comp_driver_info comp_mixer_info = {
	.drv = &comp_mixer,
};

UT_STATIC void sys_comp_mixer_init(void)
{
	comp_register(platform_shared_get(&comp_mixer_info,
					  sizeof(comp_mixer_info)));
}

DECLARE_MODULE(sys_comp_mixer_init);
