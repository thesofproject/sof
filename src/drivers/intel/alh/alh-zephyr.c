// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

#include <sof/audio/component.h>
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

/* zephyr dai driver */
#include <drivers/dai.h>

/* a8e4218c-e863-4c93-84e7-5c27d2504501 */
DECLARE_SOF_UUID("alh-dai", alh_uuid, 0xa8e4218c, 0xe863, 0x4c93,
		 0x84, 0xe7, 0x5c, 0x27, 0xd2, 0x50, 0x45, 0x01);

DECLARE_TR_CTX(alh_tr, SOF_UUID(alh_uuid), LOG_LEVEL_INFO);

static int alh_trigger_zephyr(struct dai *dai, int cmd, int direction)
{
	enum dai_trigger_cmd cmd_z;

	switch(cmd) {
	case COMP_TRIGGER_STOP:
		cmd_z = DAI_TRIGGER_STOP;
		break;
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		cmd_z = DAI_TRIGGER_START;
		break;
	case COMP_TRIGGER_PAUSE:
		cmd_z = DAI_TRIGGER_PAUSE;
		break;
	case COMP_TRIGGER_PRE_START:
	case COMP_TRIGGER_PRE_RELEASE:
		cmd_z = DAI_TRIGGER_PRE_START;
		break;
	default:
	       return -EINVAL;
	}

	return dai_trigger(dai->z_drv, direction, cmd_z);
}

static int alh_set_config_zephyr(struct dai *dai, struct ipc_config_dai *common_config,
				 void *spec_config)
{
	struct sof_ipc_dai_config *sof_cfg;
	struct dai_config cfg;

	sof_cfg = (struct sof_ipc_dai_config *)spec_config;
	cfg.dai_index = common_config->dai_index;
	cfg.format = sof_cfg->format;
	cfg.options = sof_cfg->flags;

	if(common_config->is_config_blob) {
		cfg.type = DAI_INTEL_ALH_NHLT;
		return dai_config_set(dai->z_drv, &cfg, spec_config);
	} else {
		cfg.type = DAI_INTEL_ALH;
		return dai_config_set(dai->z_drv, &cfg, &sof_cfg->alh);
	}
}

static int alh_get_hw_params_zephyr(struct dai *dai, struct sof_ipc_stream_params  *params, int dir)
{
	const struct dai_config *cfg;

	cfg = dai_config_get(dai->z_drv, dir);

	params->rate = cfg->rate;
	params->buffer_fmt = 0;
	params->channels = cfg->channels;

	switch (cfg->word_size) {
        case 16:
                params->frame_fmt = SOF_IPC_FRAME_S16_LE;
                break;
        case 24:
                params->frame_fmt = SOF_IPC_FRAME_S24_4LE;
                break;
        case 32:
                params->frame_fmt = SOF_IPC_FRAME_S32_LE;
                break;
        default:
		dai_warn(dai,"%d not supported format", cfg->word_size);
        }

	return 0;
}

static int alh_get_handshake_zephyr(struct dai *dai, int direction, int stream_id)
{
	const struct dai_properties *props;

	props = dai_get_properties(dai->z_drv, direction, stream_id);

	return props->dma_hs_id;
}

static int alh_get_fifo_zephyr(struct dai *dai, int direction, int stream_id)
{
	const struct dai_properties *props;

	props = dai_get_properties(dai->z_drv, direction, stream_id);

	return props->fifo_address;
}

static int alh_get_stream_id_zephyr(struct dai *dai, int direction)
{
	const struct dai_properties *props;

	props = dai_get_properties(dai->z_drv, direction, 0);

	return props->stream_id;
}

static int alh_probe_zephyr(struct dai *dai)
{
	return dai_probe(dai->z_drv);
}

static int alh_remove_zephyr(struct dai *dai)
{
	return dai_remove(dai->z_drv);
}

const struct dai_driver alh_driver = {
	.type = SOF_DAI_INTEL_ALH,
	.uid = SOF_UUID(alh_uuid),
	.tctx = &alh_tr,
	.dma_caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP,
	.dma_dev = DMA_DEV_ALH,
	.ops = {
		.trigger		= alh_trigger_zephyr,
		.set_config		= alh_set_config_zephyr,
		.get_hw_params		= alh_get_hw_params_zephyr,
		.get_handshake		= alh_get_handshake_zephyr,
		.get_fifo		= alh_get_fifo_zephyr,
		.get_stream_id		= alh_get_stream_id_zephyr,
		.probe			= alh_probe_zephyr,
		.remove			= alh_remove_zephyr,
	},
};
