// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/common.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <sof/ut.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <stddef.h>

static const struct comp_driver comp_switch;

LOG_MODULE_REGISTER(switch, CONFIG_SOF_LOG_LEVEL);

/* 385cc44b-f34e-4b9b-8be0-535c5f43a825 */
DECLARE_SOF_RT_UUID("switch", switch_uuid, 0x385cc44b, 0xf34e, 0x4b9b,
		 0x8b, 0xe0, 0x53, 0x5c, 0x5f, 0x43, 0xa8, 0x25);

DECLARE_TR_CTX(switch_tr, SOF_UUID(switch_uuid), LOG_LEVEL_INFO);

static struct comp_dev *switch_new(const struct comp_driver *drv,
				   struct comp_ipc_config *config,
				   void *spec)
{
	comp_cl_info(&comp_switch, "switch_new()");

	return NULL;
}

static void switch_free(struct comp_dev *dev)
{
}

/* set component audio stream parameters */
static int switch_params(struct comp_dev *dev,
			 struct sof_ipc_stream_params *params)
{
	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int switch_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	/* switch will use buffer "connected" status */
	return 0;
}

/* copy and process stream data from source to sink buffers */
static int switch_copy(struct comp_dev *dev)
{
	return 0;
}

static int switch_reset(struct comp_dev *dev)
{
	return 0;
}

static int switch_prepare(struct comp_dev *dev)
{
	return 0;
}

static const struct comp_driver comp_switch = {
	.type	= SOF_COMP_SWITCH,
	.uid	= SOF_RT_UUID(switch_uuid),
	.tctx	= &switch_tr,
	.ops	= {
		.create		= switch_new,
		.free		= switch_free,
		.params		= switch_params,
		.cmd		= switch_cmd,
		.copy		= switch_copy,
		.prepare	= switch_prepare,
		.reset		= switch_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_switch_info = {
	.drv = &comp_switch,
};

UT_STATIC void sys_comp_switch_init(void)
{
	comp_register(platform_shared_get(&comp_switch_info,
					  sizeof(comp_switch_info)));
}

DECLARE_MODULE(sys_comp_switch_init);
