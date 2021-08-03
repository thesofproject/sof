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
#endif

DECLARE_TR_CTX(mixer_tr, SOF_UUID(mixer_uuid), LOG_LEVEL_INFO);

/* mixer component private data */
struct mixer_data {
#if CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base_cfg;
#endif

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
	int16_t *src;
	int16_t *dest;
	int32_t val;
	int i;
	int j;
	int channel;
	uint32_t frag = 0;

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < sink->channels; channel++) {
			val = 0;

			for (j = 0; j < num_sources; j++) {
				src = audio_stream_read_frag_s16(sources[j],
								 frag);
				val += *src;
			}

			dest = audio_stream_write_frag_s16(sink, frag);

			/* Saturate to 16 bits */
			*dest = sat_int16(val);

			frag++;
		}
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE
/* Mix n 32 bit PCM source streams to one sink stream */
static void mix_n_s32(struct comp_dev *dev, struct audio_stream *sink,
		      const struct audio_stream **sources, uint32_t num_sources,
		      uint32_t frames)
{
	int32_t *src;
	int32_t *dest;
	int64_t val;
	int i;
	int j;
	int channel;
	uint32_t frag = 0;

	for (i = 0; i < frames; i++) {
		for (channel = 0; channel < sink->channels; channel++) {
			val = 0;

			for (j = 0; j < num_sources; j++) {
				src = audio_stream_read_frag_s32(sources[j],
								 frag);
				val += *src;
			}

			dest = audio_stream_write_frag_s32(sink, frag);

			/* Saturate to 32 bits */
			*dest = sat_int32(val);

			frag++;
		}
	}
}
#endif /* CONFIG_FORMAT_S24LE || CONFIG_FORMAT_S32LE */

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

/* used to pass standard and bespoke commands (with data) to component */
static int mixer_trigger(struct comp_dev *dev, int cmd)
{
	int dir = dev->pipeline->source_comp->direction;
	int ret;

	comp_dbg(dev, "mixer_trigger()");

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
		case COMP_TRIGGER_PRE_START:
			/* Mixer and downstream components might or might not be active */
			if (mixer_source_status_count(dev, COMP_STATE_ACTIVE) ||
			    mixer_source_status_count(dev, COMP_STATE_PAUSED))
				return PPL_STATUS_PATH_STOP;
		}

		comp_writeback(dev);
	}

	ret = comp_set_state(dev, cmd);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* nothing else to check for capture streams */
	if (dir == SOF_IPC_STREAM_CAPTURE)
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
	uint32_t sink_flags, source_flags;

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

	/* write zeros if all sources are inactive */
	if (num_mix_sources == 0) {
		uint32_t free_frames;
		buffer_lock(sink, &sink_flags);
		free_frames = audio_stream_get_free_frames(&sink->stream);
		frames = MIN(frames, free_frames);
		sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);
		buffer_unlock(sink, sink_flags);

		if (!audio_stream_set_zero(&sink->stream, sink_bytes)) {
			buffer_writeback(sink, sink_bytes);
			comp_update_buffer_produce(sink, sink_bytes);
		}

		return 0;
	}

	buffer_lock(sink, &sink_flags);

	/* check for underruns */
	for (i = 0; i < num_mix_sources; i++) {
		uint32_t avail_frames;
		buffer_lock(sources[i], &source_flags);
		avail_frames = audio_stream_avail_frames(sources_stream[i],
							 &sink->stream);
		frames = MIN(frames, avail_frames);
		buffer_unlock(sources[i], source_flags);
	}

	buffer_unlock(sink, sink_flags);

	/* Every source has the same format, so calculate bytes based
	 * on the first one.
	 */
	source_bytes = frames * audio_stream_frame_bytes(sources_stream[0]);
	sink_bytes = frames * audio_stream_frame_bytes(&sink->stream);

	comp_dbg(dev, "mixer_copy(), source_bytes = 0x%x, sink_bytes = 0x%x",
		 source_bytes, sink_bytes);

	/* mix streams */
	for (i = num_mix_sources - 1; i >= 0; i--)
		buffer_invalidate(sources[i], source_bytes);
	md->mix_func(dev, &sink->stream, sources_stream, num_mix_sources,
		     frames);
	buffer_writeback(sink, sink_bytes);

	/* update source buffer pointers */
	for (i = num_mix_sources - 1; i >= 0; i--)
		comp_update_buffer_consume(sources[i], source_bytes);

	/* update sink buffer pointer */
	comp_update_buffer_produce(sink, sink_bytes);

	return 0;
}

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
			if (source->source->state > COMP_STATE_READY)
				/* should not reset the downstream components */
				return 1;
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
static int mixer_prepare(struct comp_dev *dev)
{
	struct mixer_data *md = comp_get_drvdata(dev);
	struct list_item *blist;
	struct comp_buffer *source;
	struct comp_buffer *sink;
	int downstream = 0;
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
			md->mix_func = mix_n_s32;
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

	/* check each mixer source state */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);

		/* only prepare downstream if we have no active sources */
		if (source->source->state == COMP_STATE_PAUSED ||
		    source->source->state == COMP_STATE_ACTIVE) {
			downstream = 1;
		}
	}

	/* prepare downstream */
	return downstream;
}

#if CONFIG_IPC_MAJOR_3
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

	dcache_invalidate_region(spec, sizeof(md->base_cfg));
	memcpy_s(&md->base_cfg, sizeof(md->base_cfg), spec, sizeof(md->base_cfg));
	comp_set_drvdata(dev, md);

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

/* params are derived from base config for ipc4 path */
static int mixout_params(struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	struct mixer_data *md = comp_get_drvdata(dev);

	memset(params, 0, sizeof(*params));
	params->channels = md->base_cfg.audio_fmt.channels_count;
	params->rate = md->base_cfg.audio_fmt.sampling_frequency;
	params->sample_container_bytes = md->base_cfg.audio_fmt.depth;
	params->sample_valid_bytes = md->base_cfg.audio_fmt.valid_bit_depth;
	params->frame_fmt = dev->ipc_config.frame_fmt;
	params->buffer_fmt = md->base_cfg.audio_fmt.interleaving_style;
	params->buffer.size = md->base_cfg.ibs;

	return mixer_params(dev, params);
}

/* the original pipeline is : buffer  -> mixin -> buffer -> mixout
 * now convert it to : buffer -> mixer
 */
static int mixin_bind(struct comp_dev *dev, void *data)
{
	struct ipc4_module_bind_unbind *bu;
	struct comp_buffer *source_buf;
	struct comp_dev *sink;
	int src_id, sink_id;

	bu = (struct ipc4_module_bind_unbind *)data;
	src_id = IPC4_COMP_ID(bu->header.r.module_id, bu->header.r.instance_id);
	sink_id = IPC4_COMP_ID(bu->data.r.dst_module_id, bu->data.r.dst_instance_id);

	/* mixin -> mixout */
	if (dev->ipc_config.id == src_id) {
		struct list_item *blist;

		sink = ipc4_get_comp_dev(sink_id);
		list_for_item(blist, &sink->bsource_list) {
			source_buf = container_of(blist, struct comp_buffer, sink_list);
			if (source_buf->source == dev) {
				pipeline_disconnect(sink, source_buf, PPL_CONN_DIR_BUFFER_TO_COMP);
				pipeline_disconnect(dev, source_buf, PPL_CONN_DIR_COMP_TO_BUFFER);
				buffer_free(source_buf);
				break;
			}
		}

		source_buf = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
		pipeline_disconnect(dev, source_buf, PPL_CONN_DIR_BUFFER_TO_COMP);
		pipeline_connect(sink, source_buf, PPL_CONN_DIR_BUFFER_TO_COMP);
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
		.prepare	= mixer_prepare,
		.trigger	= mixer_trigger,
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
