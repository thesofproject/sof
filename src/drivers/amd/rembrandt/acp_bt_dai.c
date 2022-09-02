// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2022 AMD. All rights reserved.
//
//Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		Bala Kishore <balakishore.pati@amd.com>

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

/* 4abd71ba-8619-458a-b33f-160fc0cf809b */
DECLARE_SOF_UUID("btdai", btdai_uuid, 0x4abd71ba, 0x8619, 0x458a,
		  0xb3, 0x3f, 0x16, 0x0f, 0xc0, 0xcf, 0x80, 0x9b);

DECLARE_TR_CTX(btdai_tr, SOF_UUID(btdai_uuid), LOG_LEVEL_INFO);

static inline int btdai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
				   void *spec_config)
{
	/* nothing to do on rembrandt */
	return 0;
}

static int btdai_trigger(struct dai *dai, int cmd, int direction)
{
	/* nothing to do on rembrandt */
	return 0;
}

static int btdai_probe(struct dai *dai)
{
	/* TODO */
	return 0;
}

static int btdai_remove(struct dai *dai)
{
	/* TODO */
	return 0;
}

static int btdai_get_fifo(struct dai *dai, int direction, int stream_id)
{
	switch (direction) {
	case DAI_DIR_PLAYBACK:
	case DAI_DIR_CAPTURE:
		return dai_fifo(dai, direction);
	default:
		dai_err(dai, "Not a valid direction");
		return -EINVAL;
	}
}

static int btdai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[direction].handshake;
}

static int btdai_get_hw_params(struct dai *dai,
			       struct sof_ipc_stream_params *params, int dir)
{
	/* supported parameters */
	params->rate = ACP_DEFAULT_SAMPLE_RATE;
	params->channels = ACP_DEFAULT_NUM_CHANNELS;
	params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params->frame_fmt = SOF_IPC_FRAME_S16_LE;
	return 0;
}

const struct dai_driver acp_btdai_driver = {
	.type	  = SOF_DAI_AMD_BT,
	.uid	  = SOF_UUID(btdai_uuid),
	.tctx	  = &btdai_tr,
	.dma_dev  = DMA_DEV_BT,
	.dma_caps = DMA_CAP_BT,
	.ops = {
		.trigger		= btdai_trigger,
		.set_config		= btdai_set_config,
		.probe			= btdai_probe,
		.remove			= btdai_remove,
		.get_fifo		= btdai_get_fifo,
		.get_handshake		= btdai_get_handshake,
		.get_hw_params		= btdai_get_hw_params,
	},
};
