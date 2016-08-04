/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/work.h>
#include <reef/clock.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>

/* this should ramp from 0dB to mute in 64ms.
 * i.e 2^16 -> 0 in 32 * 2048 steps each lasting 2ms
 */
#define VOL_RAMP_US	2000
#define VOL_RAMP_STEP	(1 << 11)
#define VOL_MAX		(1 << 16)

/*
 * Simple volume control
 *
 * Gain amplitude value is between 0 (mute) ... 2^16 (0dB) ... 2^24 (~+48dB)
 *
 * Currently we use 16 bit data for copies to/from DAIs and HOST PCM buffers,
 * 32 bit data is used in all other cases for overhead.
 * TODO: Add 24 bit (4 byte aligned) support using HiFi2 EP SIMD.
 */

/* volume component private data */
struct comp_data {
	uint32_t volume[STREAM_MAX_CHANNELS];	/* current volume */
	uint32_t tvolume[STREAM_MAX_CHANNELS];	/* target volume */
	uint32_t mvolume[STREAM_MAX_CHANNELS];	/* mute volume */
	void (*scale_vol)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer *source, uint32_t frames);
	struct work volwork;

	/* host volume readback */
	uint32_t *hvol[STREAM_MAX_CHANNELS];
};

struct comp_func_map {
	uint16_t source;	/* source format */
	uint16_t sink;		/* sink format */
	void (*func)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer *source, uint32_t frames);
};

/* copy and scale volume from 16 bit source buffer to 32 bit dest buffer */
static void vol_s16_to_s32(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t*) source->r_ptr;
	int32_t *dest = (int32_t*) sink->w_ptr;
	int i, j;

	/* buffer sizes are always divisable by period frames */
	for (i = 0; i < frames; i++) {
		for (j = 0; j < source->params.channels; j++) {
			int32_t val = (int32_t)*src;
			*dest = (val * cd->volume[j]) >> 16;
			dest++;
			src++;
		}
	}

	source->r_ptr = src;
	sink->w_ptr = dest;
}

/* copy and scale volume from 32 bit source buffer to 16 bit dest buffer */
static void vol_s32_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t*) source->r_ptr;
	int16_t *dest = (int16_t*) sink->w_ptr;
	int i, j;

	/* buffer sizes are always divisable by period frames */
	for (i = 0; i < frames; i++) {
		for (j = 0; j < source->params.channels; j++) {
			/* TODO: clamp when converting to int16_t */
			*dest = (int16_t)((*src * cd->volume[j]) >> 16);
			dest++;
			src++;
		}
	}

	source->r_ptr = src;
	sink->w_ptr = dest;
}

/* copy and scale volume from 32 bit source buffer to 32 bit dest buffer */
static void vol_s32_to_s32(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int32_t *src = (int32_t*) source->r_ptr;
	int32_t *dest = (int32_t*) sink->w_ptr;
	int i, j;

	/* buffer sizes are always divisable by period frames */
	for (i = 0; i < frames; i++) {
		for (j = 0; j < source->params.channels; j++) {
			*dest = (*src * cd->volume[j]) >> 16;
			dest++;
			src++;
		}
	}

	source->r_ptr = src;
	sink->w_ptr = dest;
}

/* copy and scale volume from 16 bit source buffer to 16 bit dest buffer */
static void vol_s16_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source, uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *src = (int16_t*) source->r_ptr;
	int16_t *dest = (int16_t*) sink->w_ptr;
	int i, j;

	/* buffer sizes are always divisable by period frames */
	for (i = 0; i < frames; i++) {
		for (j = 0; j < source->params.channels; j++) {
			int32_t val = (int32_t)*src;
			/* TODO: clamp when converting to int16_t */
			*dest = (int16_t)((val * cd->volume[j]) >> 16);

			dest++;
			src++;
		}
	}

	source->r_ptr = src;
	sink->w_ptr = dest;
}

/* map of source and sink buffer formats to volume function */
static const struct comp_func_map func_map[] = {
	{STREAM_FORMAT_S16_LE, STREAM_FORMAT_S16_LE, vol_s16_to_s16},
	{STREAM_FORMAT_S16_LE, STREAM_FORMAT_S32_LE, vol_s16_to_s32},
	{STREAM_FORMAT_S32_LE, STREAM_FORMAT_S16_LE, vol_s32_to_s16},
	{STREAM_FORMAT_S32_LE, STREAM_FORMAT_S32_LE, vol_s32_to_s32},
};

static void vol_update(struct comp_data *cd, uint32_t chan)
{
	cd->volume[chan] = cd->tvolume[chan];

	if (cd->hvol[chan])
		*cd->hvol[chan] = cd->volume[chan];
}

/* this ramps volume changes over time */
static uint32_t vol_work(void *data, uint32_t delay)
{
	struct comp_dev *dev = (struct comp_dev *)data;
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t vol;
	int i, again = 0;

	/* inc/dec each volume if it's not at target */ 
	for (i = 0; i < STREAM_MAX_CHANNELS; i++) {

		/* skip if target reached */
		if (cd->volume[i] == cd->tvolume[i]) {
			continue;
		}

		vol = cd->volume[i];

		if (cd->volume[i] < cd->tvolume[i]) {
			/* ramp up */
			vol += VOL_RAMP_STEP;

			/* ramp completed ? */
			if (vol >= cd->tvolume[i] || vol >= VOL_MAX)
				vol_update(cd, i);
			else {
				cd->volume[i] = vol;
				again = 1;
			}
		} else {
			/* ramp down */
			vol -= VOL_RAMP_STEP;

			/* ramp completed ? */
			if (vol <= cd->tvolume[i] || vol >= VOL_MAX)
				vol_update(cd, i);
			else {
				cd->volume[i] = vol;
				again = 1;
			}
		}
	}

	/* do we need to continue ramping */
	if (again)
		return VOL_RAMP_US;
	else
		return 0;
}

static struct comp_dev *volume_new(uint32_t type, uint32_t index,
	uint32_t direction)
{
	struct comp_dev *dev;
	struct comp_data *cd;
	int i;

	dev = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	cd = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*cd));
	if (cd == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	comp_clear_ep(dev);
	work_init(&cd->volwork, vol_work, dev, WORK_ASYNC);

	/* set the default volumes */
	for (i = 0; i < STREAM_MAX_CHANNELS; i++) {
		cd->volume[i] = VOL_MAX;
		cd->tvolume[i] = VOL_MAX;
		cd->hvol[i] = NULL;
	}

	return dev;
}

static void volume_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	rfree(RZONE_MODULE, RMOD_SYS, cd);
	rfree(RZONE_MODULE, RMOD_SYS, dev);
}

/* set component audio stream paramters */
static int volume_params(struct comp_dev *dev, struct stream_params *params)
{
	/* dont do any data transformation */
	comp_set_sink_params(dev, params);

	return 0;
}

static inline void volume_set_chan(struct comp_dev *dev, int chan, uint32_t vol)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	/* TODO: ignore vol of 0 atm - bad IPC */
	if (vol > 0 && vol <= VOL_MAX)
		cd->tvolume[chan] = vol;
}

static inline void volume_set_chan_mute(struct comp_dev *dev, int chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->mvolume[chan] = cd->volume[chan];
	cd->tvolume[chan] = 0;
}

static inline void volume_set_chan_unmute(struct comp_dev *dev, int chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->tvolume[chan] = cd->mvolume[chan];
}

/* used to pass standard and bespoke commands (with data) to component */
static int volume_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_volume *cv;
	int i;

	switch (cmd) {
	case COMP_CMD_VOLUME:
		cv = (struct comp_volume*)data;

		for (i = 0; i < STREAM_MAX_CHANNELS; i++) {
			if (cv->update_bits & (0x1 << i))
				volume_set_chan(dev, i, cv->volume);
		}

		work_schedule_default(&cd->volwork, VOL_RAMP_US);
		break;
	case COMP_CMD_MUTE:
		cv = (struct comp_volume*)data;

		for (i = 0; i < STREAM_MAX_CHANNELS; i++) {
			if (cv->update_bits & (0x1 << i))
				volume_set_chan_mute(dev, i);
		}
		work_schedule_default(&cd->volwork, VOL_RAMP_US);
		break;
	case COMP_CMD_UNMUTE:
		cv = (struct comp_volume*)data;

		for (i = 0; i < STREAM_MAX_CHANNELS; i++) {
			if (cv->update_bits & (0x1 << i))
				volume_set_chan_unmute(dev, i);
		}
		work_schedule_default(&cd->volwork, VOL_RAMP_US);
		break;
	case COMP_CMD_START:
		dev->state = COMP_STATE_RUNNING;
		break;
	case COMP_CMD_STOP:
		dev->state = COMP_STATE_STOPPED;
		break;
	case COMP_CMD_PAUSE:
		dev->state = COMP_STATE_PAUSED;
		break;
	case COMP_CMD_RELEASE:
		dev->state = COMP_STATE_RUNNING;
		break;
	case COMP_CMD_IPC_MMAP_VOL(0) ... COMP_CMD_IPC_MMAP_VOL(STREAM_MAX_CHANNELS - 1):
		i = cmd - COMP_CMD_IPC_MMAP_VOL(0);
		cd->hvol[i] = data;
		if (cd->hvol[i])
			*cd->hvol[i] = cd->volume[i];
		break;
	default:
		break;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int volume_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink, *source;
	uint32_t cframes = 64;

	/* volume components will only ever have 1 source and 1 sink buffer */
	source = list_first_entry(&dev->bsource_list, struct comp_buffer, sink_list);
	sink = list_first_entry(&dev->bsink_list, struct comp_buffer, source_list);

#if 0
	trace_value((uint32_t)(source->r_ptr - source->addr));
	trace_value((uint32_t)(sink->w_ptr - sink->addr));
#endif

	/* copy and scale volume */
	cd->scale_vol(dev, sink, source, cframes);

	/* update buffer pointers for overflow */
	if (source->r_ptr >= source->end_addr)
		source->r_ptr = source->addr;
	if (sink->w_ptr >= sink->end_addr)
		sink->w_ptr = sink->addr;

	/* calc new free and available */
	comp_update_buffer(sink);
	comp_update_buffer(source);

	/* number of frames sent downstream */
	return cframes;
}

static int volume_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink, *source;
	int i;

	/* volume components will only ever have 1 source and 1 sink buffer */
	source = list_first_entry(&dev->bsource_list, struct comp_buffer, sink_list);
	sink = list_first_entry(&dev->bsink_list, struct comp_buffer, source_list);

	/* map the volume function for source and sink buffers */
	for (i = 0; i < ARRAY_SIZE(func_map); i++) {

		if (source->params.pcm.format != func_map[i].source)
			continue;
		if (sink->params.pcm.format != func_map[i].sink)
			continue;

		cd->scale_vol = func_map[i].func;
		goto found;
	}

	return -EINVAL;

found:
	for (i = 0; i < STREAM_MAX_CHANNELS; i++) {
		if (cd->hvol[i])
			*cd->hvol[i] = cd->volume[i];
	}

	/* copy periods from host */
	if (source->params.direction == STREAM_DIRECTION_PLAYBACK) {
		for (i = 0; i < PLAT_HOST_PERIODS; i++)
			volume_copy(dev);
	}

	dev->state = COMP_STATE_PREPARE;
	return 0;
}

static int volume_reset(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int i;

	dev->state = COMP_STATE_INIT;

	for (i = 0; i < STREAM_MAX_CHANNELS; i++)
		cd->hvol[i] = NULL;

	return 0;
}

struct comp_driver comp_volume = {
	.type	= COMP_TYPE_VOLUME,
	.ops	= {
		.new		= volume_new,
		.free		= volume_free,
		.params		= volume_params,
		.cmd		= volume_cmd,
		.copy		= volume_copy,
		.prepare	= volume_prepare,
		.reset		= volume_reset,
	},
};

void sys_comp_volume_init(void)
{
	comp_register(&comp_volume);
}

