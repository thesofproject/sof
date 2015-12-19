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

static struct comp_dev *host_new(uint32_t uuid, int id)
{

	return 0;
}

static void host_free(struct comp_dev *dev)
{

}

/* set component audio COMP paramters */
static int host_params(struct comp_dev *dev, struct stream_params *params)
{

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int host_cmd(struct comp_dev *dev, int cmd, void *data)
{

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int host_copy(struct comp_dev *sink, struct comp_dev *source)
{

	return 0;
}

struct comp_driver comp_host = {
	.uuid	= COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_HOST),
	.ops	= {
		.new		= host_new,
		.free		= host_free,
		.params		= host_params,
		.cmd		= host_cmd,
		.copy		= host_copy,
	},
};

void sys_comp_host_init(void)
{
	comp_register(&comp_host);
}
