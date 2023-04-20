// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2022 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Bala Kishore <balakishore.pati@amd.com>

#include <sof/audio/component.h>
#include <sof/drivers/acp_dai_dma.h>
#include <rtos/interrupt.h>
#include <rtos/alloc.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/uuid.h>
#include <ipc/dai.h>
#include <ipc/topology.h>
#include <platform/fw_scratch_mem.h>
#include <sof/lib/io.h>
#include <platform/chip_offset_byte.h>

/* 8f00c3bb-e835-4767-9a34-b8ec1041e56b */
DECLARE_SOF_UUID("sw0audiodai", sw0audiodai_uuid, 0x8f00c3bb, 0xe835, 0x4767,
		0x9a, 0x34, 0xb8, 0xec, 0x10, 0x41, 0xe5, 0x6b);
DECLARE_TR_CTX(sw0audiodai_tr, SOF_UUID(sw0audiodai_uuid), LOG_LEVEL_INFO);

static inline int sw0audiodai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
				   const void *spec_config)
{
	const struct sof_ipc_dai_config *config = spec_config;
	struct acp_pdata *acpdata = dai_get_drvdata(dai);

	//acp_p1_sw_bt_tx_frame_format sw1_tx_frameformat;
	//acp_p1_sw_bt_rx_frame_format sw1_rx_frameformat;
	acpdata->config = *config;
	acpdata->sdw_params = config->acpsdw;

	return 0;
}

static int sw0audiodai_trigger(struct dai *dai, int cmd, int direction)
{
	/* nothing to do on rembrandt for HS dai */
	return 0;
}

static int sw0audiodai_probe(struct dai *dai)
{
	struct acp_pdata *acp;

	dai_info(dai, "HS dai probe");
	/* allocate private data */
	acp = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*acp));
	if (!acp) {
		dai_err(dai, "HS dai probe alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, acp);
	return 0;
}

static int sw0audiodai_remove(struct dai *dai)
{
	struct acp_pdata *acp = dai_get_drvdata(dai);

	dai_info(dai, "sw0audiodai_remove");
	rfree(acp);
	dai_set_drvdata(dai, NULL);

	return 0;
}

static int sw0audiodai_get_fifo(struct dai *dai, int direction, int stream_id)
{
	switch (direction) {
	case DAI_DIR_PLAYBACK:
	case DAI_DIR_CAPTURE:
		return dai_fifo(dai, direction);
	default:
		dai_err(dai, "Invalid direction");
		return -EINVAL;
	}
}

static int sw0audiodai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[direction].handshake;
}

static int sw0audiodai_get_hw_params(struct dai *dai,
			      struct sof_ipc_stream_params *params,
			      int dir)
{
	struct acp_pdata *acpdata = dai_get_drvdata(dai);

	if (dir == DAI_DIR_PLAYBACK) {
		/* SP DAI currently supports only these parameters */
		params->rate = acpdata->sdw_params.rate;
		params->channels = acpdata->sdw_params.channels;
		params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
		params->frame_fmt = SOF_IPC_FRAME_S16_LE;
	} else if (dir == DAI_DIR_CAPTURE) {
		/* SP DAI currently supports only these parameters */
		params->rate = acpdata->sdw_params.rate;
		params->channels = acpdata->sdw_params.channels;
		params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
		params->frame_fmt = SOF_IPC_FRAME_S16_LE;
	}

	return 0;
}

const struct dai_driver acp_sw0audiodai_driver = {
	.type = SOF_DAI_AMD_SW0_AUDIO,
	.uid = SOF_UUID(sw0audiodai_uuid),
	.tctx = &sw0audiodai_tr,
	.dma_dev = DMA_DEV_SP,
	.dma_caps = DMA_CAP_SP,
	.ops = {
		.trigger		= sw0audiodai_trigger,
		.set_config		= sw0audiodai_set_config,
		.probe			= sw0audiodai_probe,
		.remove			= sw0audiodai_remove,
		.get_fifo		= sw0audiodai_get_fifo,
		.get_handshake		= sw0audiodai_get_handshake,
		.get_hw_params          = sw0audiodai_get_hw_params,
	},
};

