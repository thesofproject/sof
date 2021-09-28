// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
// Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		Anup Kulkarni <anup.kulkarni@amd.com>
//		Bala Kishore <balakishore.pati@amd.com>

#include <sof/audio/component.h>
#include <sof/drivers/acp_dai_dma.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/alloc.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/uuid.h>
#include <ipc/dai.h>
#include <ipc/topology.h>
#include <platform/fw_scratch_mem.h>
#include <sof/lib/io.h>
#include <platform/chip_offset_byte.h>

/*0ae40946-dfd2-4140-91-52-0d-d5-a3-ea-ae-81*/
DECLARE_SOF_UUID("acp_dmic_dai", acp_dmic_dai_uuid, 0x0ae40946, 0xdfd2, 0x4140,
		0x91, 0x52, 0x0d, 0xd5, 0xa3, 0xea, 0xae, 0x81);

DECLARE_TR_CTX(acp_dmic_dai_tr, SOF_UUID(acp_dmic_dai_uuid), LOG_LEVEL_INFO);

static inline int acp_dmic_dai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
					  void *spec_config)
{
	/* nothing to do on dmic dai */
	return 0;
}

static int acp_dmic_dai_trigger(struct dai *dai, int cmd, int direction)
{
	dai_dbg(dai, "acp_dmic_dai_trigger");
	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_STOP:
	default:
		break;
	}
	return 0;
}

static int acp_dmic_dai_probe(struct dai *dai)
{
	/* TODO */
	return 0;
}

static int acp_dmic_dai_get_fifo(struct dai *dai, int direction, int stream_id)
{
	switch (direction) {
	case DAI_DIR_PLAYBACK:
	case DAI_DIR_CAPTURE:
		return dai_fifo(dai, direction);
	default:
		dai_err(dai, "acp_dmic_dai_get_fifo(): Invalid direction");
		return -EINVAL;
	}
}

static int acp_dmic_dai_get_handshake(struct dai *dai, int direction,
					int stream_id)
{
	return dai->plat_data.fifo[direction].handshake;
}

static int acp_dmic_dai_get_hw_params(struct dai *dai,
			      struct sof_ipc_stream_params *params,
			      int dir)
{
	/* ACP only currently supports these parameters */
	params->rate = ACP_SAMPLE_RATE_16K;/* 16000 sample rate only for dmic */
	params->channels = ACP_DEFAULT_NUM_CHANNELS;
	params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params->frame_fmt = SOF_IPC_FRAME_S32_LE;
	return 0;
}

const struct dai_driver acp_dmic_dai_driver = {
	.type = SOF_DAI_AMD_DMIC,
	.uid = SOF_UUID(acp_dmic_dai_uuid),
	.tctx = &acp_dmic_dai_tr,
	.dma_dev = DMA_DEV_DMIC,
	.dma_caps = DMA_CAP_DMIC,
	.ops = {
		.trigger		= acp_dmic_dai_trigger,
		.set_config		= acp_dmic_dai_set_config,
		.probe			= acp_dmic_dai_probe,
		.get_fifo		= acp_dmic_dai_get_fifo,
		.get_handshake		= acp_dmic_dai_get_handshake,
		.get_hw_params		= acp_dmic_dai_get_hw_params,
	},
};
