// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <sof/drivers/ers.h>

#include <sof/drivers/ipc.h>
#include <ipc/dai.h>

#define trace_ers(__e, ...) \
	trace_event(TRACE_CLASS_ERS, __e, ##__VA_ARGS__)
#define tracev_ers(__e, ...) \
	tracev_event(TRACE_CLASS_ERS, __e, ##__VA_ARGS__)
#define trace_ers_error(__e, ...) \
	trace_error(TRACE_CLASS_ERS, __e, ##__VA_ARGS__)

struct ers_pdata {
	struct sof_ipc_dai_config config;
	struct sof_ipc_dai_ers_params params;
};

static int ers_context_store(struct dai *dai)
{
	trace_ers("ers_context_store()");
	return 0;
}

static int ers_context_restore(struct dai *dai)
{
	trace_ers("ers_context_restore()");
	return 0;
}

static int ers_set_config(struct dai *dai,
			  struct sof_ipc_dai_config *config)
{
	struct ipc_comp_dev *comp = ipc_glb_get_comp(config->ers.source_buffer_id);

	trace_ers("ers_set_config()");

	if (!comp) {
		trace_ers_error("ers_set_config() error: missing buffer "
				"component to attach to, ID = %u",
				config->ers.source_buffer_id);

		return -ENODEV;
	}

	dai->plat_data.fifo[1].handshake = (uintptr_t)comp->cb;


	return 0;
}

static int ers_trigger(struct dai *dai, int cmd, int direction)
{
	trace_ers("ers_trigger()");
	return 0;
}

static int ers_probe(struct dai *dai)
{
	trace_ers("ers_probe()");
		return 0;
}

static int ers_remove(struct dai *dai)
{
	trace_ers("ers_remove()");
	return 0;
}

const struct dai_driver ers_driver = {
	.type = SOF_DAI_INTEL_ERS,
	.dma_caps = DMA_CAP_BUF_CP,
	.dma_dev = DMA_DEV_BUF,
	.ops = {
		.trigger = ers_trigger,
		.set_config = ers_set_config,
		.pm_context_store = ers_context_store,
		.pm_context_restore = ers_context_restore,
		.probe = ers_probe,
		.remove = ers_remove,
	},
};
