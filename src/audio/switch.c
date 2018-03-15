/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/audio/component.h>

/* tracing */
#define trace_switch(__e) trace_event(TRACE_CLASS_SWITCH, __e)
#define trace_switch_error(__e)   trace_error(TRACE_CLASS_SWITCH, __e)
#define tracev_switch(__e)        tracev_event(TRACE_CLASS_SWITCH, __e)

static struct comp_dev *switch_new(struct sof_ipc_comp *comp)
{
	trace_switch("new");

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
static int switch_cmd(struct comp_dev *dev, int cmd, void *data)
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

void sys_comp_switch_init(void)
{
	comp_register(&comp_switch);
}
