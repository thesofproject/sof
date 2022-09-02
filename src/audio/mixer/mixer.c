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
#include <rtos/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc4/base-config.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

static const struct comp_driver comp_mixer;

LOG_MODULE_REGISTER(mixer, CONFIG_SOF_LOG_LEVEL);

/* bc06c037-12aa-417c-9a97-89282e321a76 */
DECLARE_SOF_RT_UUID("mixer", mixer_uuid, 0xbc06c037, 0x12aa, 0x417c,
		 0x9a, 0x97, 0x89, 0x28, 0x2e, 0x32, 0x1a, 0x76);

DECLARE_TR_CTX(mixer_tr, SOF_UUID(mixer_uuid), LOG_LEVEL_INFO);

/* mixer component private data */
struct mixer_data {
	bool sources_inactive;

	void (*mix_func)(struct comp_dev *dev, struct audio_stream __sparse_cache *sink,
			 const struct audio_stream __sparse_cache **sources, uint32_t count,
			 uint32_t frames);
};

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
	struct comp_buffer __sparse_cache *sink_c;
	size_t sink_period_bytes, sink_stream_size;
	int err;

	comp_dbg(dev, "mixer_params()");

	err = mixer_verify_params(dev, params);
	if (err < 0) {
		comp_err(dev, "mixer_params(): pcm params verification failed.");
		return -EINVAL;
	}

	sinkb = list_first_item(&dev->bsink_list, struct comp_buffer,
				source_list);
	sink_c = buffer_acquire(sinkb);

	sink_stream_size = sink_c->stream.size;

	/* calculate period size based on config */
	sink_period_bytes = audio_stream_period_bytes(&sink_c->stream,
						      dev->frames);
	buffer_release(sink_c);

	if (sink_period_bytes == 0) {
		comp_err(dev, "mixer_params(): period_bytes = 0");
		return -EINVAL;
	}

	if (sink_stream_size < sink_period_bytes) {
		comp_err(dev, "mixer_params(): sink buffer size %d is insufficient < %d",
			 sink_stream_size, sink_period_bytes);
		return -ENOMEM;
	}

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int mixer_trigger_common(struct comp_dev *dev, int cmd)
{
	int ret;

	ret = comp_set_state(dev, cmd);
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
	struct comp_buffer *source, *sink;
	struct comp_buffer __sparse_cache *source_c, *sink_c;
	struct comp_buffer *sources[PLATFORM_MAX_STREAMS];
	struct comp_buffer __sparse_cache *sources_c[PLATFORM_MAX_STREAMS];
	const struct audio_stream __sparse_cache *sources_stream[PLATFORM_MAX_STREAMS];
	struct list_item *blist;
	int32_t i = 0;
	int32_t num_mix_sources = 0;
	uint32_t frames = INT32_MAX;
	/* Redundant, but helps the compiler */
	uint32_t source_bytes = 0;
	uint32_t sink_bytes;

	comp_dbg(dev, "mixer_copy()");

	/* calculate the highest runtime component status
	 * between input streams
	 */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		source_c = buffer_acquire(source);

		/* only mix the sources with the same state with mixer */
		if (source_c->source && source_c->source->state == dev->state)
			sources[num_mix_sources++] = source;

		buffer_release(source_c);

		/* too many sources ? */
		if (num_mix_sources == PLATFORM_MAX_STREAMS - 1)
			return 0;
	}

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);
	sink_c = buffer_acquire(sink);

	/* check for underruns */
	for (i = 0; i < num_mix_sources; i++) {
		uint32_t avail_frames;

		source_c = buffer_acquire(sources[i]);
		avail_frames = audio_stream_avail_frames(&source_c->stream,
							 &sink_c->stream);
		frames = MIN(frames, avail_frames);

		if (i == num_mix_sources - 1)
			/* Every source has the same format, so calculate bytes
			 * based on the last one - once we've got the final
			 * frames value.
			 */
			source_bytes = frames * audio_stream_frame_bytes(&source_c->stream);

		buffer_release(source_c);
	}

	if (!num_mix_sources || (frames == 0 && md->sources_inactive)) {
		/*
		 * Generate silence when sources are inactive. When
		 * sources change to active, additionally keep
		 * generating silence until at least one of the
		 * sources start to have data available (frames!=0).
		 */
		sink_bytes = dev->frames * audio_stream_frame_bytes(&sink_c->stream);
		if (!audio_stream_set_zero(&sink_c->stream, sink_bytes)) {
			buffer_stream_writeback(sink_c, sink_bytes);
			comp_update_buffer_produce(sink_c, sink_bytes);
		}

		buffer_release(sink_c);

		md->sources_inactive = true;

		return 0;
	}

	if (md->sources_inactive) {
		md->sources_inactive = false;
		comp_dbg(dev, "mixer_copy exit sources_inactive state");
	}

	sink_bytes = frames * audio_stream_frame_bytes(&sink_c->stream);

	comp_dbg(dev, "mixer_copy(), source_bytes = 0x%x, sink_bytes = 0x%x",
		 source_bytes, sink_bytes);

	/* mix streams */
	for (i = num_mix_sources - 1; i >= 0; i--) {
		sources_c[i] = buffer_acquire(sources[i]);
		sources_stream[i] = &sources_c[i]->stream;
		buffer_stream_invalidate(sources_c[i], source_bytes);
	}

	md->mix_func(dev, &sink_c->stream, sources_stream, num_mix_sources,
		     frames);
	buffer_stream_writeback(sink_c, sink_bytes);

	/* update source buffer pointers */
	for (i = num_mix_sources - 1; i >= 0; i--)
		comp_update_buffer_consume(sources_c[i], source_bytes);

	for (i = 0; i < num_mix_sources; i++)
		buffer_release(sources_c[i]);

	/* update sink buffer pointer */
	comp_update_buffer_produce(sink_c, sink_bytes);

	buffer_release(sink_c);

	return 0;
}

static inline bool mixer_stop_reset(struct comp_dev *dev, struct comp_dev *source);
static int mixer_reset(struct comp_dev *dev)
{
	int dir = dev->pipeline->source_comp->direction;
	struct list_item *blist;
	struct mixer_data *md = comp_get_drvdata(dev);

	comp_dbg(dev, "mixer_reset()");

	if (dir == SOF_IPC_STREAM_PLAYBACK) {
		list_for_item(blist, &dev->bsource_list) {
			/* FIXME: this is racy and implicitly protected by serialised IPCs */
			struct comp_buffer *source = container_of(blist, struct comp_buffer,
								  sink_list);
			struct comp_buffer __sparse_cache *source_c = buffer_acquire(source);
			bool stop = mixer_stop_reset(dev, source_c->source);

			buffer_release(source_c);
			/* only mix the sources with the same state with mixer */
			if (stop)
				/* should not reset the downstream components */
				return PPL_STATUS_PATH_STOP;
		}
	}

	md->mix_func = NULL;

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
	struct comp_buffer __sparse_cache *sink_c;
	int ret;

	comp_dbg(dev, "mixer_prepare()");

	/* does mixer already have active source streams ? */
	if (dev->state == COMP_STATE_ACTIVE)
		return 0;

	sink = list_first_item(&dev->bsink_list, struct comp_buffer,
			       source_list);
	sink_c = buffer_acquire(sink);
	md->mix_func = mixer_get_processing_function(dev, sink_c);
	buffer_release(sink_c);

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	return 0;
}

static int mixer_source_status_count(struct comp_dev *mixer, uint32_t status)
{
	struct list_item *blist;
	int count = 0;

	/* count source with state == status */
	list_for_item(blist, &mixer->bsource_list) {
		/*
		 * FIXME: this is racy, state can be changed by another core.
		 * This is implicitly protected by serialised IPCs. Even when
		 * IPCs are processed in the pipeline thread, the next IPC will
		 * not be sent until the thread has processed and replied to the
		 * current one.
		 */
		struct comp_buffer *source = container_of(blist, struct comp_buffer,
							  sink_list);
		struct comp_buffer __sparse_cache *source_c = buffer_acquire(source);
		if (source_c->source && source_c->source->state == status)
			count++;
		buffer_release(source_c);
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
		ret = PPL_STATUS_PATH_STOP;
	}

	return ret;
}

static inline bool mixer_stop_reset(struct comp_dev *dev, struct comp_dev *source)
{
	return source && source->state > COMP_STATE_READY;
}

static int mixer_prepare(struct comp_dev *dev)
{
	struct list_item *blist;
	int ret;

	ret = mixer_prepare_common(dev);
	if (ret)
		return ret;

	/* check each mixer source state */
	list_for_item(blist, &dev->bsource_list) {
		struct comp_buffer *source;
		struct comp_buffer __sparse_cache *source_c;
		bool stop;

		/*
		 * FIXME: this is intrinsically racy. One of mixer sources can
		 * run on a different core and can enter PAUSED or ACTIVE right
		 * after we have checked it here. We should set a flag or a
		 * status to inform any other connected pipelines that we're
		 * preparing the mixer, so they shouldn't touch it until we're
		 * done.
		 */
		source = container_of(blist, struct comp_buffer, sink_list);
		source_c = buffer_acquire(source);
		stop = source_c->source && (source_c->source->state == COMP_STATE_PAUSED ||
					    source_c->source->state == COMP_STATE_ACTIVE);
		buffer_release(source_c);

		/* only prepare downstream if we have no active sources */
		if (stop)
			return PPL_STATUS_PATH_STOP;
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

static SHARED_DATA struct comp_driver_info comp_mixer_info = {
	.drv = &comp_mixer,
};

UT_STATIC void sys_comp_mixer_init(void)
{
	comp_register(platform_shared_get(&comp_mixer_info,
					  sizeof(comp_mixer_info)));
}

DECLARE_MODULE(sys_comp_mixer_init);
