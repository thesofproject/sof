// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <sof/dai.h>
#include <sof/dma.h>
#include <sof/trace.h>
#include <ipc/dai.h>

#define trace_soundwire(__e, ...) \
	trace_event(TRACE_CLASS_SOUNDWIRE, __e, ##__VA_ARGS__)
#define trace_soundwire_error(__e, ...)	\
	trace_error(TRACE_CLASS_SOUNDWIRE, __e, ##__VA_ARGS__)
#define tracev_soundwire(__e, ...) \
	tracev_event(TRACE_CLASS_SOUNDWIRE, __e, ##__VA_ARGS__)

static int soundwire_trigger(struct dai *dai, int cmd, int direction)
{
	trace_soundwire("soundwire_trigger() cmd %d", cmd);

	return 0;
}

static int soundwire_set_config(struct dai *dai,
				struct sof_ipc_dai_config *config)
{
	trace_soundwire("soundwire_set_config() config->format = 0x%4x",
			config->format);

	return 0;
}

static int soundwire_context_store(struct dai *dai)
{
	trace_soundwire("soundwire_context_store()");

	return 0;
}

static int soundwire_context_restore(struct dai *dai)
{
	trace_soundwire("soundwire_context_restore()");

	return 0;
}

static int soundwire_probe(struct dai *dai)
{
	trace_soundwire("soundwire_probe()");

	return 0;
}

static int soundwire_remove(struct dai *dai)
{
	trace_soundwire("soundwire_remove()");

	return 0;
}

const struct dai_driver soundwire_driver = {
	.type = SOF_DAI_INTEL_SOUNDWIRE,
	.dma_caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP,
	.dma_dev = DMA_DEV_SOUNDWIRE,
	.ops = {
		.trigger		= soundwire_trigger,
		.set_config		= soundwire_set_config,
		.pm_context_store	= soundwire_context_store,
		.pm_context_restore	= soundwire_context_restore,
		.probe			= soundwire_probe,
		.remove			= soundwire_remove,
	},
};
