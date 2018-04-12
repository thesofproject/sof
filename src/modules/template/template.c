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
#include <sof/module.h>
#include <sof/audio/component.h>

/* tracing */
#define trace_template(__e) trace_event(TRACE_CLASS_VOLUME, __e)
#define trace_template_error(__e)   trace_error(TRACE_CLASS_VOLUME, __e)
#define tracev_template(__e)        tracev_event(TRACE_CLASS_VOLUME, __e)

static struct comp_dev *template_new(struct sof_ipc_comp *comp)
{
	trace_template("new");

	return NULL;
}

static void template_free(struct comp_dev *dev)
{
	trace_template("fre");
}

/* set component audio stream parameters */
static int template_params(struct comp_dev *dev)
{

	return 0;
}

/* used to pass standard and bespoke commands (with data) to component */
static int template_cmd(struct comp_dev *dev, int cmd, void *data)
{
	/* template will use buffer "connected" status */
	return 0;
}

/* copy and process stream data from source to sink buffers */
static int template_copy(struct comp_dev *dev)
{

	return 0;
}

static int template_reset(struct comp_dev *dev)
{
	return 0;
}

static int template_prepare(struct comp_dev *dev)
{
	return 0;
}

struct comp_driver comp_template = {
	.type	= SOF_COMP_VOLUME,
	.ops	= {
		.new		= template_new,
		.free		= template_free,
		.params		= template_params,
		.cmd		= template_cmd,
		.copy		= template_copy,
		.prepare	= template_prepare,
		.reset		= template_reset,
	},
};

static int template_init(struct sof_module *mod)
{
	comp_register(&comp_template);
	return 0;
}

static int template_exit(struct sof_module *mod)
{
	comp_unregister(&comp_template);
	return 0;
}

SOF_MODULE(template, template_init, template_exit);
