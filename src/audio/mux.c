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

static struct comp_dev *mux_new(uint32_t uuid, int id)
{

	return 0;
}

static void mux_free(struct comp_dev *dev)
{

}

/* set component audio stream paramters */
static int mux_params(struct comp_dev *dev, struct stream_params *params)
{

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int mux_cmd(struct comp_dev *dev, int cmd, void *data)
{
	/* mux will use buffer "connected" status */
	return 0;
}

/* copy and process stream data from source to sink buffers */
static int mux_copy(struct comp_dev *dev)
{

	return 0;
}

struct comp_driver comp_mux = {
	.uuid	= COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_MUX),
	.ops	= {
		.new		= mux_new,
		.free		= mux_free,
		.params		= mux_params,
		.cmd		= mux_cmd,
		.copy		= mux_copy,
	},
};

void sys_comp_mux_init(void)
{
	comp_register(&comp_mux);
}

