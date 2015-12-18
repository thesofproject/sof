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
};

