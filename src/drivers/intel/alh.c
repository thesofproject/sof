// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>

#include <sof/drivers/alh.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <sof/common.h>
#include <ipc/dai.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <stdint.h>
#include <ipc4/alh.h>

LOG_MODULE_REGISTER(alh_dai, CONFIG_SOF_LOG_LEVEL);

/* a8e4218c-e863-4c93-84e7-5c27d2504501 */
DECLARE_SOF_UUID("alh-dai", alh_uuid, 0xa8e4218c, 0xe863, 0x4c93,
		 0x84, 0xe7, 0x5c, 0x27, 0xd2, 0x50, 0x45, 0x01);

DECLARE_TR_CTX(alh_tr, SOF_UUID(alh_uuid), LOG_LEVEL_INFO);

static int alh_trigger(struct dai *dai, int cmd, int direction)
{
	dai_info(dai, "alh_trigger() cmd %d", cmd);

	return 0;
}

static int alh_set_config_tplg(struct dai *dai, struct ipc_config_dai *common_config,
			       void *spec_config)
{
	struct alh_pdata *alh = dai_get_drvdata(dai);
	struct sof_ipc_dai_config *config = spec_config;

	dai_info(dai, "alh_set_config_tplg() config->format = 0x%4x", config->format);

	if (config->alh.channels || config->alh.rate) {
		alh->params.channels = config->alh.channels;
		alh->params.rate = config->alh.rate;
		dai_info(dai, "alh_set_config() channels %d rate %d",
			 config->alh.channels, config->alh.rate);
	}

	alh->params.stream_id = config->alh.stream_id;

	return 0;
}

static int alh_set_config_blob(struct dai *dai, struct ipc_config_dai *common_config,
			       void *spec_config)
{
	struct alh_pdata *alh = dai_get_drvdata(dai);
	struct sof_alh_configuration_blob *blob = spec_config;
	struct ipc4_alh_multi_gtw_cfg *alh_cfg = &blob->alh_cfg;
	int i;

	dai_info(dai, "alh_set_config_blob()");

	alh->params.rate = common_config->sampling_frequency;

	for (i = 0; i < alh_cfg->count; i++) {
		/* the LSB 8bits are for stream id */
		int alh_id = alh_cfg->mapping[i].alh_id & 0xff;

		if (IPC4_ALH_DAI_INDEX(alh_id) == dai->index) {
			alh->params.stream_id = alh_id;
			alh->params.channels = popcount(alh_cfg->mapping[i].channel_mask);
			break;
		}
	}

	return 0;
}

static int alh_set_config(struct dai *dai, struct ipc_config_dai *common_config,
			  void *spec_config)
{
	if (!common_config->is_config_blob)
		return alh_set_config_tplg(dai, common_config, spec_config);
	else
		return alh_set_config_blob(dai, common_config, spec_config);
}

/* get ALH hw params */
static int alh_get_hw_params(struct dai *dai,
			     struct sof_ipc_stream_params *params, int dir)
{
	struct alh_pdata *alh = dai_get_drvdata(dai);

	params->rate = alh->params.rate;
	params->channels = alh->params.channels;

	/* 0 means variable */
	params->buffer_fmt = 0;

	/* FIFO format is static */
	params->frame_fmt = SOF_IPC_FRAME_S32_LE;

	return 0;
}

static int alh_probe(struct dai *dai)
{
	struct alh_pdata *alh;

	dai_info(dai, "alh_probe()");

	if (dai_get_drvdata(dai))
		return -EEXIST;

	alh = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*alh));
	if (!alh) {
		dai_err(dai, "alh_probe() error: alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, alh);

	return 0;
}

static int alh_remove(struct dai *dai)
{
	dai_info(dai, "alh_remove()");

	rfree(dai_get_drvdata(dai));
	dai_set_drvdata(dai, NULL);

	return 0;
}

static int alh_get_handshake(struct dai *dai, int direction, int stream_id)
{
	if (stream_id >= ARRAY_SIZE(alh_handshake_map)) {
		dai_err(dai, "alh_get_handshake(): "
				"stream_id %d out of range", stream_id);

		return -1;
	}

	return alh_handshake_map[stream_id];
}

static int alh_get_fifo(struct dai *dai, int direction, int stream_id)
{
	uint32_t offset = direction == SOF_IPC_STREAM_PLAYBACK ?
		ALH_TXDA_OFFSET : ALH_RXDA_OFFSET;

	return ALH_BASE + offset + ALH_STREAM_OFFSET * stream_id;
}

static int alh_get_fifo_depth(struct dai *dai, int direction)
{
	return dai->plat_data.fifo[direction].depth;
}

const struct dai_driver alh_driver = {
	.type = SOF_DAI_INTEL_ALH,
	.uid = SOF_UUID(alh_uuid),
	.tctx = &alh_tr,
	.dma_caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP,
	.dma_dev = DMA_DEV_ALH,
	.ops = {
		.trigger		= alh_trigger,
		.set_config		= alh_set_config,
		.get_hw_params		= alh_get_hw_params,
		.get_handshake		= alh_get_handshake,
		.get_fifo		= alh_get_fifo,
		.get_fifo_depth		= alh_get_fifo_depth,
		.probe			= alh_probe,
		.remove			= alh_remove,
	},
};
