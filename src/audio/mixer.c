/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/audio/component.h>

#define trace_mixer(__e)	trace_event(TRACE_CLASS_MIXER, __e)
#define tracev_mixer(__e)	tracev_event(TRACE_CLASS_MIXER, __e)
#define trace_mixer_error(__e)	trace_error(TRACE_CLASS_MIXER, __e)

/* mixer component private data */
struct mixer_data {
	void (*mix_func)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer **sources, uint32_t count, uint32_t frames);
};

/* mix N PCM source streams to one sink stream */
static void mix_n(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer **sources, uint32_t num_sources, uint32_t frames)
{
	int32_t *src, *dest = sink->w_ptr, count;
	int64_t val[2];
	int i, j;

	count = frames * sink->params.pcm->channels;

	for (i = 0; i < count; i += 2) {
		val[0] = 0;
		val[1] = 0;
		for (j = 0; j < num_sources; j++) {
			src = sources[j]->r_ptr;

			/* TODO: clamp */
			val[0] += src[i];
			val[1] += src[i + 1];
		}

		/* TODO: best place for attenuation ? */
		dest[i] = (val[0] >> (num_sources >> 1));
		dest[i + 1] = (val[1] >> (num_sources >> 1));
	}

	/* update R/W pointers */
	sink->w_ptr = dest + count;
	for (j = 0; j < num_sources; j++) {
		src = sources[j]->r_ptr;
		sources[j]->r_ptr = src + count;
	}
}

static struct comp_dev *mixer_new(struct sof_ipc_comp *comp)
{
	struct comp_dev *dev;
	struct sof_ipc_comp_mixer *mixer;
	struct sof_ipc_comp_mixer *ipc_mixer = (struct sof_ipc_comp_mixer *)comp;
	struct mixer_data *md;

	trace_mixer("new");
	dev = rzalloc(RZONE_RUNTIME, RFLAGS_NONE,
		COMP_SIZE(struct sof_ipc_comp_mixer));
	if (dev == NULL)
		return NULL;

	mixer = (struct sof_ipc_comp_mixer *)&dev->comp;
	memcpy(mixer, ipc_mixer, sizeof(struct sof_ipc_comp_mixer));

	md = rzalloc(RZONE_RUNTIME, RFLAGS_NONE, sizeof(*md));
	if (md == NULL) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, md);

	return dev;
}

static void mixer_free(struct comp_dev *dev)
{
	struct mixer_data *md = comp_get_drvdata(dev);

	rfree(md);
	rfree(dev);
}

/* set component audio stream parameters */
static int mixer_params(struct comp_dev *dev, struct stream_params *params)
{
	struct stream_params sink_params = *params;

	/* dont do any params downstream setting for running mixer stream */
	if (dev->state == COMP_STATE_RUNNING)
		return 1;

	/* suppose sink component won't be host/dai, so hard code it */
	sink_params.pcm->frame_fmt = SOF_IPC_FRAME_S32_LE;
	sink_params.pcm->frame_size = 4 * params->pcm->channels; /* 32bit container */

	/* dont do any data transformation */
	comp_buffer_sink_params(dev, &sink_params);

	return 0;
}

static int mixer_status_change(struct comp_dev *dev/* , uint32_t target_state */)
{
	int finish = 0;
	struct comp_buffer *source;
	struct list_item * blist;
	uint32_t stream_target = COMP_STATE_INIT;

	/* calculate the highest status between input streams */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		if (source->source->state > stream_target)
			stream_target = source->source->state;
	}

	if (dev->state == stream_target)
		finish = 1;
	else
		dev->state = stream_target;

	return finish;
}

/* used to pass standard and bespoke commands (with data) to component */
static int mixer_cmd(struct comp_dev *dev, int cmd, void *data)
{
	int finish = 0;

	switch(cmd) {
	case COMP_CMD_START:
		trace_mixer("MSa");
	case COMP_CMD_PAUSE:
	case COMP_CMD_RELEASE:
	case COMP_CMD_DRAIN:
	case COMP_CMD_SUSPEND:
	case COMP_CMD_RESUME:
		finish = mixer_status_change(dev);
		break;
	case COMP_CMD_STOP:
		finish = mixer_status_change(dev);
		if (finish == 0)
			comp_buffer_reset(dev);
		break;
	default:
		break;
	}

	return finish;
}

/* mix N source PCM streams to one sink stream */
static int mixer_copy(struct comp_dev *dev)
{
	struct mixer_data *md = comp_get_drvdata(dev);
	struct comp_buffer *sink, *sources[5], *source;
	uint32_t i = 0, num_mix_sources, cframes = PLAT_INT_PERIOD_FRAMES;
	struct list_item * blist;

	trace_mixer("Mix");

	/* calculate the highest status between input streams */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		/* only mix the sources with the same state with mixer*/
		if (source->source->state == dev->state)
			sources[i++] = source;
	}

	num_mix_sources = i;

	sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

	for(i = 0; i < num_mix_sources; i++) {
		if (sources[i]->avail < cframes * sources[i]->params.pcm->frame_size)
			cframes = sources[i]->avail /sources[i]->params.pcm->frame_size;
	}
	if (sink->free < cframes * sink->params.pcm->frame_size)
		cframes = sink->free /sink->params.pcm->frame_size;

	if (num_mix_sources == 0)
		cframes = 0;

	/* no frames to mix */
	if (cframes == 0) {
		trace_value(cframes);
		return 0;
	}

	/* mix streams */
	md->mix_func(dev, sink, sources, i, cframes);

	/* update buffer pointers for overflow */
	for(i = num_mix_sources; i > 0; i--) {
		if (sources[i-1]->r_ptr >= sources[i-1]->end_addr)
			sources[i-1]->r_ptr = sources[i-1]->addr;
		comp_update_buffer_consume(sources[i-1]);
	}
	if (sink->w_ptr >= sink->end_addr)
		sink->w_ptr = sink->addr;

	/* calc new free and available */
	comp_update_buffer_produce(sink);

	/* number of frames sent downstream */
	return cframes;
}

static int mixer_reset(struct comp_dev *dev)
{
	struct list_item * blist;
	struct comp_buffer *source;

	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		/* only mix the sources with the same state with mixer*/
		if (source->source->state > COMP_STATE_SETUP)
			return 1; /* should not reset the downstream components */
	}

	dev->state = COMP_STATE_INIT;
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
	struct list_item * blist;
	struct comp_buffer *source;
	int downstream = 0;

	trace_mixer("MPp");

	if (dev->state != COMP_STATE_RUNNING) {
		md->mix_func = mix_n;
		dev->state = COMP_STATE_PREPARE;
		//dev->preload = PLAT_INT_PERIODS;
	}

	/* check each mixer source state */
	list_for_item(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);

		/* only prepare downstream if we have no active sources */
		if (source->source->state == COMP_STATE_PAUSED ||
				source->source->state == COMP_STATE_RUNNING) {
			downstream = 1;
		}
	}

	/* prepare downstream */
	return downstream;
}

static int mixer_preload(struct comp_dev *dev)
{
	//int i;

	if (dev->state != COMP_STATE_PREPARE)
		return 1;

	/* preload and mix periods if inactive */
	//for (i = 0; i < dev->preload; i++)
	//	mixer_copy(dev);

	return 0;
}

struct comp_driver comp_mixer = {
	.type	= SOF_COMP_MIXER,
	.ops	= {
		.new		= mixer_new,
		.free		= mixer_free,
		.params		= mixer_params,
		.prepare	= mixer_prepare,
		.preload	= mixer_preload,
		.cmd		= mixer_cmd,
		.copy		= mixer_copy,
		.reset		= mixer_reset,
	},
};

void sys_comp_mixer_init(void)
{
	comp_register(&comp_mixer);
}
