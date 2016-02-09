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

static struct comp_dev *switch_new(struct comp_desc *desc)
{

	return 0;
}

static void switch_free(struct comp_dev *dev)
{

}

/* set component audio stream paramters */
static int switch_params(struct comp_dev *dev, struct stream_params *params)
{

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int switch_cmd(struct comp_dev *dev, struct stream_params *params,
	int cmd, void *data)
{
	/* switch will use buffer "connected" status */
	return 0;
}

/* copy and process stream data from source to sink buffers */
static int switch_copy(struct comp_dev *dev, struct stream_params *params)
{

	return 0;
}

struct comp_driver comp_switch = {
	.uuid	= COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_SWITCH),
	.ops	= {
		.new		= switch_new,
		.free		= switch_free,
		.params		= switch_params,
		.cmd		= switch_cmd,
		.copy		= switch_copy,
	},
};

void sys_comp_switch_init(void)
{
	comp_register(&comp_switch);
}

