// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <sof/drivers/hda.h>
#include <sof/drivers/ssp.h>
#include <sof/drivers/timestamp.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/uuid.h>
#include <ipc/dai.h>
#include <ipc/stream.h>
#include <ipc/topology.h>

/* bc9ebe20-4577-41bb-9eed-d0cb236328da */
DECLARE_SOF_UUID("hda-dai", hda_uuid, 0xbc9ebe20, 0x4577, 0x41bb,
		 0x9e, 0xed, 0xd0, 0xcb, 0x23, 0x63, 0x28, 0xda);

DECLARE_TR_CTX(hda_tr, SOF_UUID(hda_uuid), LOG_LEVEL_INFO);

static int hda_trigger(struct dai *dai, int cmd, int direction)
{
	return 0;
}

static int hda_set_config(struct dai *dai,
			  struct sof_ipc_dai_config *config)
{
	struct hda_pdata *hda = dai_get_drvdata(dai);

	if (config->hda.channels || config->hda.rate) {
		hda->params.channels = config->hda.channels;
		hda->params.rate = config->hda.rate;
	}


	return 0;
}

/* get HDA hw params */
static int hda_get_hw_params(struct dai *dai,
			     struct sof_ipc_stream_params  *params, int dir)
{
	struct hda_pdata *hda = dai_get_drvdata(dai);

	params->rate = hda->params.rate;
	params->channels = hda->params.channels;

	/* 0 means variable */
	params->buffer_fmt = 0;
	params->frame_fmt = 0;


	return 0;
}

static int hda_probe(struct dai *dai)
{
	struct hda_pdata *hda;

	dai_info(dai, "hda_probe()");

	if (dai_get_drvdata(dai))
		return -EEXIST;

	hda = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*hda));
	if (!hda) {
		dai_err(dai, "hda_probe() error: alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, hda);


	return 0;
}

static int hda_remove(struct dai *dai)
{
	dai_info(dai, "hda_remove()");

	rfree(dai_get_drvdata(dai));
	dai_set_drvdata(dai, NULL);

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
	.uid = SOF_UUID(hda_uuid),
	.tctx = &hda_tr,
	.dma_caps = DMA_CAP_HDA,
	.dma_dev = DMA_DEV_HDA,
	.ops = {
		.trigger		= hda_trigger,
		.set_config		= hda_set_config,
		.pm_context_store	= hda_dummy,
		.pm_context_restore	= hda_dummy,
		.get_hw_params		= hda_get_hw_params,
		.get_handshake		= hda_get_handshake,
		.get_fifo		= hda_get_fifo,
		.probe			= hda_probe,
		.remove			= hda_remove,
	},
	.ts_ops = {
		.ts_config		= timestamp_hda_config,
		.ts_start		= timestamp_hda_start,
		.ts_get			= timestamp_hda_get,
		.ts_stop		= timestamp_hda_stop,
	},
};
