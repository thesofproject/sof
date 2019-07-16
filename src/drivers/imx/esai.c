// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/drivers/esai.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <ipc/dai.h>

static int esai_context_store(struct dai *dai)
{
	return 0;
}

static int esai_context_restore(struct dai *dai)
{
	return 0;
}

static inline int esai_set_config(struct dai *dai,
				 struct sof_ipc_dai_config *config)
{
	return 0;
}

static int esai_trigger(struct dai *dai, int cmd, int direction)
{
	return 0;
}

static int esai_probe(struct dai *dai)
{
	return 0;
}

const struct dai_driver esai_driver = {
	.type = SOF_DAI_IMX_ESAI,
	.dma_dev = DMA_DEV_ESAI,
	.ops = {
		.trigger		= esai_trigger,
		.set_config		= esai_set_config,
		.pm_context_store	= esai_context_store,
		.pm_context_restore	= esai_context_restore,
		.probe			= esai_probe,
	},
};
