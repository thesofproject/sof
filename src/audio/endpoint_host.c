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

static int endpoint_new(struct comp_dev *dev)
{

	return 0;
}

static void endpoint_free(struct comp_dev *dev)
{

}

/* set component audio COMP paramters */
static int endpoint_params(struct comp_dev *dev, struct stream_params *params)
{

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int endpoint_cmd(struct comp_dev *dev, int cmd, void *data)
{

	return 0;
}

/* copy and process stream data from source to sink buffers */
static int endpoint_copy(struct comp_dev *sink, struct comp_dev *source)
{

	return 0;
}

struct comp_driver comp_endpoint_host = {
	.uuid	= COMP_UUID(COMP_VENDOR_GENERIC, COMP_TYPE_EP_HOST),
	.ops	= {
		.new		= endpoint_new,
		.free		= endpoint_free,
		.params		= endpoint_params,
		.cmd		= endpoint_cmd,
		.copy		= endpoint_copy,
	},
};

