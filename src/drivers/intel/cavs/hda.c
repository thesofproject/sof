// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <errno.h>
#include <sof/hda.h>

static int hda_trigger(struct dai *dai, int cmd, int direction)
{
	return 0;
}

static int hda_set_config(struct dai *dai,
			  struct sof_ipc_dai_config *config)
{
	return 0;
}

static int hda_dummy(struct dai *dai)
{
	return 0;
}

const struct dai_driver hda_driver = {
	.type = SOF_DAI_INTEL_HDA,
	.dma_caps = DMA_CAP_HDA,
	.dma_dev = DMA_DEV_HDA,
	.ops = {
		.trigger		= hda_trigger,
		.set_config		= hda_set_config,
		.pm_context_store	= hda_dummy,
		.pm_context_restore	= hda_dummy,
		.probe			= hda_dummy,
		.remove			= hda_dummy,
	},
};
