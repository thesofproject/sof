// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <sof/drivers/hda.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <ipc/dai.h>

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

static int hda_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return 0;
}

static int hda_get_fifo(struct dai *dai, int direction, int stream_id)
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
		.get_handshake		= hda_get_handshake,
		.get_fifo		= hda_get_fifo,
		.probe			= hda_dummy,
		.remove			= hda_dummy,
	},
};
