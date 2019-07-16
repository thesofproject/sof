// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/common.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <stddef.h>

/* tracing */
#define trace_switch(__e) trace_event(TRACE_CLASS_SWITCH, __e)
#define trace_switch_error(__e)   trace_error(TRACE_CLASS_SWITCH, __e)
#define tracev_switch(__e)        tracev_event(TRACE_CLASS_SWITCH, __e)

static struct comp_dev *switch_new(struct sof_ipc_comp *comp)
{
	trace_switch("switch_new()");

	return NULL;
}

static void switch_free(struct comp_dev *dev)
{

}

/* set component audio stream parameters */
static int switch_params(struct comp_dev *dev)
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

struct comp_driver comp_switch = {
	.type	= SOF_COMP_SWITCH,
	.ops	= {
		.new		= switch_new,
		.free		= switch_free,
		.params		= switch_params,
		.cmd		= switch_cmd,
		.copy		= switch_copy,
		.prepare	= switch_prepare,
		.reset		= switch_reset,
	},
};

static void sys_comp_switch_init(void)
{
	comp_register(&comp_switch);
}

DECLARE_MODULE(sys_comp_switch_init);
