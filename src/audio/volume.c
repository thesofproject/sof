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
#include <reef/audio/component.h>

#define MAX_CHANNELS	8

struct comp_volume {
	uint8_t volume[MAX_CHANNELS];
};

/* copy and scale volume from 16 bit source buffer to 24 bit dest buffer */
 int vol_s16_to_s24_4(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source)
{
	struct comp_volume *cv = comp_get_drvdata(dev);
	uint16_t *src = (uint16_t*) source->r_ptr;
	uint32_t *dest = (uint32_t*) sink->w_ptr;
	int i, j;
	// TODO period_size to period_count
	// TODO unroll into single loop channels * period_count
	// buffer sizes are always divisable by period size
	for (i = 0; i < dev->params.channels; i++) {
		for (j = 0; j < dev->period_size; j++) {
			uint32_t val = (uint32_t)*src;
			*dest = (val * cv->volume[i]) >> 8;
			dest++;
			src++;
		}
	}

	// todo update r/w pointers/

	return 0;
}

 int vol_s24_4_to_s16(struct comp_dev *dev, struct comp_buffer *sink,
	struct comp_buffer *source)
{
	struct comp_volume *cv = comp_get_drvdata(dev);
	uint32_t *src = (uint32_t*) source->r_ptr;
	uint16_t *dest = (uint16_t*) sink->w_ptr;
	int i, j;

	for (i = 0; i < dev->params.channels; i++) {
		for (j = 0; j < dev->period_size; j++) {
			uint32_t val = (uint32_t)*src;
			*dest = (val * cv->volume[i]) >> 8;
		}
	}

	return 0;
}

static struct comp_dev *volume_new(uint32_t uuid, int id)
{

	return 0;
}

static void volume_free(struct comp_dev *dev)
{

}

/* set component audio stream paramters */
static int volume_params(struct comp_dev *dev, struct stream_params *params)
{

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int volume_cmd(struct comp_dev *dev, int cmd, void *data)
{

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int volume_copy(struct comp_dev *sink, struct comp_dev *source)
{

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
	},
	.caps	= {
		.source = {
			.formats	= STREAM_FORMAT_S16_LE | STREAM_FORMAT_S24_4LE,
			.min_rate	= 8000,
			.max_rate	= 192000,
			.min_channels	= 1,
			.max_channels	= MAX_CHANNELS,
		},
		.sink = {
			.formats	= STREAM_FORMAT_S16_LE | STREAM_FORMAT_S24_4LE,
			.min_rate	= 8000,
			.max_rate	= 192000,
			.min_channels	= 1,
			.max_channels	= MAX_CHANNELS,
		},
	},
};

void sys_comp_volume_init(void)
{
	comp_register(&comp_volume);
}

