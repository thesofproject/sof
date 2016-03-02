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

static struct comp_dev *mixer_new(uint32_t type, uint32_t index,
	uint8_t direction)
{

	return 0;
}

static void mixer_free(struct comp_dev *dev)
{

}

/* set component audio stream paramters */
static int mixer_params(struct comp_dev *dev, struct stream_params *params)
{
	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int mixer_cmd(struct comp_dev *dev, int cmd, void *data)
{
	/* mixer will use buffer "connected" status */
	return 0;
}

/* copy and process stream data from source to sink buffers */
static int mixer_copy(struct comp_dev *dev)
{
	return 0;
}

static int mixer_reset(struct comp_dev *dev)
{
	return 0;
}

static int mixer_prepare(struct comp_dev *dev)
{
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

