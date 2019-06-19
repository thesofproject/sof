// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <user/trace.h>

#define trace_alh(__e, ...) trace_event(TRACE_CLASS_ALH, __e, ##__VA_ARGS__)
#define trace_alh_error(__e, ...) \
	trace_error(TRACE_CLASS_ALH, __e, ##__VA_ARGS__)
#define tracev_alh(__e, ...) tracev_event(TRACE_CLASS_ALH, __e, ##__VA_ARGS__)

static int alh_trigger(struct dai *dai, int cmd, int direction)
{
	trace_alh("alh_trigger() cmd %d", cmd);

	return 0;
}

static int alh_set_config(struct dai *dai, struct sof_ipc_dai_config *config)
{
	trace_alh("alh_set_config() config->format = 0x%4x",
		  config->format);

	return 0;
}

static int alh_context_store(struct dai *dai)
{
	trace_alh("alh_context_store()");

	return 0;
}

static int alh_context_restore(struct dai *dai)
{
	trace_alh("alh_context_restore()");

	return 0;
}

static int alh_probe(struct dai *dai)
{
	trace_alh("alh_probe()");

	return 0;
}

static int alh_remove(struct dai *dai)
{
	trace_alh("alh_remove()");

	return 0;
}

const struct dai_driver alh_driver = {
	.type = SOF_DAI_INTEL_ALH,
	.dma_caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP,
	.dma_dev = DMA_DEV_ALH,
	.ops = {
		.trigger		= alh_trigger,
		.set_config		= alh_set_config,
		.pm_context_store	= alh_context_store,
		.pm_context_restore	= alh_context_restore,
		.probe			= alh_probe,
		.remove			= alh_remove,
	},
};
