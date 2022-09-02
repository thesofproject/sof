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

/* 4abd71ba-8619-458a-b33f-160fc0cf809b */
DECLARE_SOF_UUID("spdai", spdai_uuid, 0x4abd71ba, 0x8619, 0x458a,
		0xb3, 0x3f, 0x16, 0x0f, 0xc0, 0xcf, 0x80, 0x9b);
DECLARE_TR_CTX(spdai_tr, SOF_UUID(spdai_uuid), LOG_LEVEL_INFO);

static inline int spdai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
				   void *spec_config)
{
	/* nothing to do on rembrandt for SP dai */
	return 0;
}

static int spdai_trigger(struct dai *dai, int cmd, int direction)
{
	/* nothing to do on rembrandt for SP dai */
	return 0;
}

static int spdai_probe(struct dai *dai)
{
	/* TODO */
	return 0;
}

static int spdai_remove(struct dai *dai)
{
	/* TODO */
	return 0;
}

static int spdai_get_fifo(struct dai *dai, int direction, int stream_id)
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

static int spdai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[direction].handshake;
}

static int spdai_get_hw_params(struct dai *dai,
			      struct sof_ipc_stream_params *params,
			      int dir)
{
	/* SP DAI currently supports only these parameters */
	params->rate = ACP_DEFAULT_SAMPLE_RATE;
	params->channels = ACP_DEFAULT_NUM_CHANNELS;
	params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params->frame_fmt = SOF_IPC_FRAME_S16_LE;
	return 0;
}

const struct dai_driver acp_spdai_driver = {
	.type = SOF_DAI_AMD_SP,
	.uid = SOF_UUID(spdai_uuid),
	.tctx = &spdai_tr,
	.dma_dev = DMA_DEV_SP,
	.dma_caps = DMA_CAP_SP,
	.ops = {
		.trigger		= spdai_trigger,
		.set_config		= spdai_set_config,
		.probe			= spdai_probe,
		.remove			= spdai_remove,
		.get_fifo		= spdai_get_fifo,
		.get_handshake		= spdai_get_handshake,
		.get_hw_params          = spdai_get_hw_params,
	},
};
