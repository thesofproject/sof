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
DECLARE_SOF_UUID("hsdai", hsdai_uuid, 0x8f00c3bb, 0xe835, 0x4767,
		0x9a, 0x34, 0xb8, 0xec, 0x10, 0x41, 0xe5, 0x6b);
DECLARE_TR_CTX(hsdai_tr, SOF_UUID(hsdai_uuid), LOG_LEVEL_INFO);

static inline int hsdai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
				   void *spec_config)
{
	/* set master clk for hs dai */
	io_reg_write(PU_REGISTER_BASE + ACP_I2STDM2_MSTRCLKGEN, 0x40081);
	return 0;
}

static int hsdai_trigger(struct dai *dai, int cmd, int direction)
{
	/* nothing to do on rembrandt for HS dai */
	return 0;
}

static int hsdai_probe(struct dai *dai)
{
	/* TODO */
	return 0;
}

static int hsdai_remove(struct dai *dai)
{
	/* TODO */
	return 0;
}

static int hsdai_get_fifo(struct dai *dai, int direction, int stream_id)
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

static int hsdai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[direction].handshake;
}

static int hsdai_get_hw_params(struct dai *dai,
			      struct sof_ipc_stream_params *params,
			      int dir)
{
	/* HS DAI currently supports only these parameters */
	params->rate = ACP_DEFAULT_SAMPLE_RATE;
	params->channels = ACP_DEFAULT_NUM_CHANNELS;
	params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params->frame_fmt = SOF_IPC_FRAME_S16_LE;
	return 0;
}

const struct dai_driver acp_hsdai_driver = {
	.type = SOF_DAI_AMD_HS,
	.uid = SOF_UUID(hsdai_uuid),
	.tctx = &hsdai_tr,
	.dma_dev = DMA_DEV_SP,
	.dma_caps = DMA_CAP_SP,
	.ops = {
		.trigger		= hsdai_trigger,
		.set_config		= hsdai_set_config,
		.probe			= hsdai_probe,
		.remove			= hsdai_remove,
		.get_fifo		= hsdai_get_fifo,
		.get_handshake		= hsdai_get_handshake,
		.get_hw_params          = hsdai_get_hw_params,
	},
};
