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
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/alloc.h>
#include <reef/audio/component.h>

struct comp_data {
	uint16_t volume[STREAM_MAX_CHANNELS];
	uint16_t mvolume[STREAM_MAX_CHANNELS]; /* mute volume */
	void (*scale_vol)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer *source);
};

/* copy and scale volume from 16 bit source buffer to 24 bit dest buffer */
static void vol_s16_to_s24_4(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint16_t *src = (uint16_t*) source->r_ptr;
	uint32_t *dest = (uint32_t*) sink->w_ptr;
	int i, j;

	/* buffer sizes are always divisable by period frames */
	for (i = 0; i < dev->params.pcm.channels; i++) {
		for (j = 0; j < dev->params.pcm.period_frames; j++) {
			uint32_t val = (uint32_t)*src;
			*dest = (val * cd->volume[i]) >> 8;
			dest++;
			src++;
		}
	}

	source->r_ptr = (uint8_t*)src;
	sink->w_ptr = (uint8_t*)dest;
}

/* copy and scale volume from 24 bit source buffer to 16 bit dest buffer */
static void vol_s24_4_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t *src = (uint32_t*) source->r_ptr;
	uint16_t *dest = (uint16_t*) sink->w_ptr;
	int i, j;

	/* buffer sizes are always divisable by period frames */
	for (i = 0; i < dev->params.pcm.channels; i++) {
		for (j = 0; j < dev->params.pcm.period_frames; j++) {
			/* TODO: clamp when converting to uint16_t */
			*dest = (uint16_t)((*src * cd->volume[i]) >> 8);
			dest++;
			src++;
		}
	}

	source->r_ptr = (uint8_t*)src;
	sink->w_ptr = (uint8_t*)dest;
}

/* copy and scale volume from 24 bit source buffer to 24 bit dest buffer */
static void vol_s24_4_to_s24_4(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint32_t *src = (uint32_t*) source->r_ptr;
	uint32_t *dest = (uint32_t*) sink->w_ptr;
	int i, j;

	/* buffer sizes are always divisable by period frames */
	for (i = 0; i < dev->params.pcm.channels; i++) {
		for (j = 0; j < dev->params.pcm.period_frames; j++) {
			*dest = (*src * cd->volume[i]) >> 8;
			dest++;
			src++;
		}
	}

	source->r_ptr = (uint8_t*)src;
	sink->w_ptr = (uint8_t*)dest;
}

/* copy and scale volume from 16 bit source buffer to 16 bit dest buffer */
static void vol_s16_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	uint16_t *src = (uint16_t*) source->r_ptr;
	uint16_t *dest = (uint16_t*) sink->w_ptr;
	int i, j;

	/* buffer sizes are always divisable by period frames */
	for (i = 0; i < dev->params.pcm.channels; i++) {
		for (j = 0; j < dev->params.pcm.period_frames; j++) {
			uint32_t val = (uint32_t)*src;
			/* TODO: clamp when converting to uint16_t */
			*dest = (uint16_t)((val * cd->volume[i]) >> 8);
			dest++;
			src++;
		}
	}

	source->r_ptr = (uint8_t*)src;
	sink->w_ptr = (uint8_t*)dest;
}

static struct comp_dev *volume_new(uint32_t uuid, int id)
{
	struct comp_dev *dev;
	struct comp_data *cd;

	dev = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*dev));
	if (dev == NULL)
		return NULL;

	cd = rmalloc(RZONE_MODULE, RMOD_SYS, sizeof(*cd));
	if (cd == NULL) {
		rfree(RZONE_MODULE, RMOD_SYS, dev);
		return NULL;
	}

	comp_set_drvdata(dev, cd);
	dev->id = id;
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
	/* just copy params atm */
	dev->params = *params;
	return 0;
}

static int volume_prepare(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink, *source;
	struct comp_dev *sink_dev, *source_dev;

	/* volume components will only ever have 1 source and 1 sink buffer */
	source = list_entry(&dev->bsource_list, struct comp_buffer, sink_list);
	sink = list_entry(&dev->bsink_list, struct comp_buffer, source_list);
	sink_dev = sink->sink;
	source_dev = source->source;

	/* do we have 16 bit sink ? */
	if (sink_dev->params.pcm.format == STREAM_FORMAT_S16_LE) {

		/* 16 bit source ? */
		if (source_dev->params.pcm.format == STREAM_FORMAT_S16_LE)
			cd->scale_vol = vol_s16_to_s16;
		else
			cd->scale_vol = vol_s16_to_s24_4;

	} else {

		/* 24 bit sink */
		if (sink_dev->params.pcm.format == STREAM_FORMAT_S24_4LE)
			cd->scale_vol = vol_s24_4_to_s24_4;
		else
			cd->scale_vol = vol_s24_4_to_s16;
	}

	return 0;
}

static inline void volume_set_chan(struct comp_dev *dev, int chan, uint16_t vol)
{
	/* TODO: clamp volume */
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->volume[chan] = vol;
}

static inline void volume_set_chan_mute(struct comp_dev *dev, int chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->mvolume[chan] = cd->volume[chan];
	cd->volume[chan] = 0;
}

static inline void volume_set_chan_unmute(struct comp_dev *dev, int chan)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	cd->volume[chan] = cd->mvolume[chan];
}

/* used to pass standard and bespoke commands (with data) to component */
static int volume_cmd(struct comp_dev *dev, int cmd, void *data)
{
	struct comp_volume *cv = (struct comp_volume*)data;
	int i;

	/* TODO - mute individual channels */
	switch (cmd) {
	case COMP_CMD_VOLUME:
		for (i = 0; i < dev->params.pcm.channels; i++)
			volume_set_chan(dev, i, cv->volume[i]);
		break;
	case COMP_CMD_MUTE:
		for (i = 0; i < dev->params.pcm.channels; i++)
			volume_set_chan_mute(dev, i);
		break;
	case COMP_CMD_UNMUTE:
		for (i = 0; i < dev->params.pcm.channels; i++)
			volume_set_chan_unmute(dev, i);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int volume_copy(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct comp_buffer *sink, *source;

	/* volume components will only ever have 1 source and 1 sink buffer */
	source = list_entry(&dev->bsource_list, struct comp_buffer, sink_list);
	sink = list_entry(&dev->bsink_list, struct comp_buffer, source_list);

	/* copy and scale volume */
	cd->scale_vol(dev, source, sink);

	/* update buffer pointers for overflow */
	if (source->r_ptr >= source->addr + source->size)
		source->r_ptr = source->addr;
	if (sink->w_ptr >= source->addr + sink->size)
		sink->w_ptr = sink->addr;

	return 0;
}

struct comp_driver comp_volume = {
	.uuid	= COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_VOLUME),
	.ops	= {
		.new		= volume_new,
		.free		= volume_free,
		.params		= volume_params,
		.cmd		= volume_cmd,
		.copy		= volume_copy,
		.prepare	= volume_prepare,
	},
	.caps	= {
		.source = {
			.formats	= STREAM_FORMAT_S16_LE | STREAM_FORMAT_S24_4LE,
			.min_rate	= 8000,
			.max_rate	= 192000,
			.min_channels	= 1,
			.max_channels	= STREAM_MAX_CHANNELS,
		},
		.sink = {
			.formats	= STREAM_FORMAT_S16_LE | STREAM_FORMAT_S24_4LE,
			.min_rate	= 8000,
			.max_rate	= 192000,
			.min_channels	= 1,
			.max_channels	= STREAM_MAX_CHANNELS,
		},
	},
};

void sys_comp_volume_init(void)
{
	comp_register(&comp_volume);
}

