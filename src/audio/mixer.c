/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
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

/* mix n stream datas to 1 sink buffer */
static void mix_n(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer **sources, uint32_t count, uint32_t frames)
{
	int16_t *src;
	int16_t *dest = (int16_t*) sink->w_ptr;
	int i, j, k;
	int32_t val = 0;

	trace_mixer("MxN");
	trace_value(count);
	trace_value((uint32_t)sink->w_ptr);

	/* buffer sizes are always divisable by period frames */
	for (i = 0; i < frames; i++) {
		for (j = 0; j < sink->params.channels; j++) {
			val = 0;
			for (k = 0; k < count; k++) {
				src = (int16_t*)sources[k]->r_ptr;
				val += *src;
				/* TODO: clamp when converting to int16_t */
				src++;
				sources[k]->r_ptr = src;
			}
			*dest = (int16_t)(val >> (count >> 1)); /* average level */
			dest++;
		}
	}

	sink->w_ptr = dest;
}

static struct comp_dev *mixer_new(uint32_t type, uint32_t index,
	uint32_t direction)
{
	struct comp_dev *dev;
	struct mixer_data *md;

	trace_mixer("MNw");
	dev = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	md = rzalloc(RZONE_MODULE, RMOD_SYS, sizeof(*md));
	if (md == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		return NULL;
	}

	comp_set_drvdata(dev, md);
	comp_clear_ep(dev);
	comp_set_mixer(dev);

	return dev;
}

static void mixer_free(struct comp_dev *dev)
{
	struct mixer_data *md = comp_get_drvdata(dev);

	rfree(RZONE_MODULE, RMOD_SYS, md);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
}

/* set component audio stream paramters */
static int mixer_params(struct comp_dev *dev, struct stream_params *params)
{
	/* dont do any params downstream setting for running mixer stream */
	if (dev->state == COMP_STATE_RUNNING)
		return 1;

	/* dont do any data transformation */
	comp_set_sink_params(dev, params);

	return 0;
}

static int mixer_status_change(struct comp_dev *dev/* , uint32_t target_state */)
{
	int finish = 0;
	struct comp_buffer *source;
	struct list_head * blist;
	uint32_t stream_target = COMP_STATE_INIT;

	/* calculate the highest status between input streams */
	list_for_each(blist, &dev->bsource_list) {
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


static struct comp_dev* mixer_volume_component(struct comp_dev *mixer)
{
	struct comp_dev *comp_dev = NULL;
	struct list_head *clist;

	list_for_each(clist, &mixer->bsink_list) {
		struct comp_buffer *buffer;

		buffer = container_of(clist, struct comp_buffer,
			source_list);

		if (buffer->sink->drv->type == COMP_TYPE_VOLUME) {
			comp_dev = buffer->sink;
			break;
		}
	}

	return comp_dev;
}

/* used to pass standard and bespoke commands (with data) to component */
static int mixer_cmd(struct comp_dev *dev, int cmd, void *data)
{
	int finish = 0;
	struct comp_dev *vol_dev = NULL;

	switch(cmd) {
	case COMP_CMD_START:
		trace_mixer("MSa");
	case COMP_CMD_STOP:
	case COMP_CMD_PAUSE:
	case COMP_CMD_RELEASE:
	case COMP_CMD_DRAIN:
	case COMP_CMD_SUSPEND:
	case COMP_CMD_RESUME:
		finish = mixer_status_change(dev);
		break;
	case COMP_CMD_VOLUME:
		vol_dev = mixer_volume_component(dev);
		if (vol_dev != NULL)
			finish = comp_cmd(vol_dev, COMP_CMD_VOLUME, data);
		break;
	case COMP_CMD_IPC_MMAP_VOL(0) ... COMP_CMD_IPC_MMAP_VOL(STREAM_MAX_CHANNELS - 1):
		vol_dev = mixer_volume_component(dev);
		if (vol_dev != NULL)
			finish = comp_cmd(vol_dev, cmd, data);
		break;
	default:
		break;
	}

	return finish;
}

/* mix N stream datas to 1 sink buffer */
static int mixer_copy(struct comp_dev *dev)
{
	struct mixer_data *md = comp_get_drvdata(dev);
	struct comp_buffer *sink, *sources[5], *source;
	uint32_t i = 0, cframes = 64;
	struct list_head * blist;

	trace_mixer("MCy");

	/* calculate the highest status between input streams */
	list_for_each(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		/* only mix the sources with the same state with mixer*/
		if (source->source->state == dev->state)
			sources[i++] = source;
	}

	sink = list_first_entry(&dev->bsink_list, struct comp_buffer, source_list);

	/* mix streams */
	md->mix_func(dev, sink, sources, i, cframes);

	/* update buffer pointers for overflow */
	for(; i>0; i--) {
		if (sources[i-1]->r_ptr >= sources[i-1]->end_addr)
			sources[i-1]->r_ptr = sources[i-1]->addr;
		comp_update_buffer(sources[i-1]);
	}
	if (sink->w_ptr >= sink->end_addr)
		sink->w_ptr = sink->addr;

	/* calc new free and available */
	comp_update_buffer(sink);

	/* number of frames sent downstream */
	return cframes;
}

static int mixer_reset(struct comp_dev *dev)
{
	struct list_head * blist;
	struct comp_buffer *source;

	list_for_each(blist, &dev->bsource_list) {
		source = container_of(blist, struct comp_buffer, sink_list);
		/* only mix the sources with the same state with mixer*/
		if (source->source->state > COMP_STATE_STOPPED)
			return 1; /* should not reset the downstream components */
	}

	return 0;
}

static int mixer_prepare(struct comp_dev *dev)
{
	struct mixer_data *md = comp_get_drvdata(dev);
	int i;

	trace_mixer("MPp");
	md->mix_func = mix_n;

	dev->state = COMP_STATE_PREPARE;

	/* mix periods */
	for (i = 0; i < PLAT_HOST_PERIODS; i++)
		mixer_copy(dev);

	return 0;
}

struct comp_driver comp_mixer = {
	.type	= COMP_TYPE_MIXER,
	.ops	= {
		.new		= mixer_new,
		.free		= mixer_free,
		.params		= mixer_params,
		.prepare	= mixer_prepare,
		.cmd		= mixer_cmd,
		.copy		= mixer_copy,
		.reset		= mixer_reset,
	},
};

void sys_comp_mixer_init(void)
{
	comp_register(&comp_mixer);
}

