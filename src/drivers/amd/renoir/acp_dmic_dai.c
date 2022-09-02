// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
// Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		Anup Kulkarni <anup.kulkarni@amd.com>
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

/*0ae40946-dfd2-4140-91-52-0d-d5-a3-ea-ae-81*/
DECLARE_SOF_UUID("acp_dmic_dai", acp_dmic_dai_uuid, 0x0ae40946, 0xdfd2, 0x4140,
		0x91, 0x52, 0x0d, 0xd5, 0xa3, 0xea, 0xae, 0x81);

DECLARE_TR_CTX(acp_dmic_dai_tr, SOF_UUID(acp_dmic_dai_uuid), LOG_LEVEL_INFO);

static inline int acp_dmic_dai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
					  void *spec_config)
{
	dai_info(dai, "ACP: acp_dmic_set_config");
	struct sof_ipc_dai_config *config = spec_config;
	struct acp_pdata *acpdata = dai_get_drvdata(dai);
	acp_wov_clk_ctrl_t clk_ctrl;

	acpdata->config = *config;
	acpdata->dmic_params = config->acpdmic;
	clk_ctrl = (acp_wov_clk_ctrl_t)io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
	clk_ctrl.u32all = 0;
	switch (acpdata->dmic_params.pdm_rate) {
	case 48000:
		/* DMIC Clock for 48K sample rate */
		clk_ctrl.bits.brm_clk_ctrl = 7;
		break;
	case 16000:
		/* DMIC Clock for 16K sample rate */
		clk_ctrl.bits.brm_clk_ctrl = 1;
		break;
	default:
		dai_info(dai, "ACP:acp_dmic_set_config unsupported samplerate");
		return -EINVAL;
	}
	io_reg_write(PU_REGISTER_BASE + ACP_WOV_CLK_CTRL, clk_ctrl.u32all);
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
	struct acp_pdata *acp;

	dai_info(dai, "ACP: acp_dmic_dai_probe");
	/* allocate private data */
	acp = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*acp));
	if (!acp) {
		dai_err(dai, "acp_dmic_dai_probe(): alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, acp);
	return 0;
}

static int acp_dmic_dai_remove(struct dai *dai)
{
	struct acp_pdata *acp = dai_get_drvdata(dai);

	dai_info(dai, "acp_dmic_dai_remove()");
	rfree(acp);
	dai_set_drvdata(dai, NULL);
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
	struct acp_pdata *acpdata = dai_get_drvdata(dai);

	switch (acpdata->dmic_params.pdm_rate) {
	case 48000:
	case 16000:
		params->rate = acpdata->dmic_params.pdm_rate;
		break;
	default:
		dai_info(dai, "ACP:unsupported samplerate %d", acpdata->dmic_params.pdm_rate);
		params->rate = ACP_DEFAULT_SAMPLE_RATE;
	}
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
		.remove			= acp_dmic_dai_remove,
		.get_fifo		= acp_dmic_dai_get_fifo,
		.get_handshake		= acp_dmic_dai_get_handshake,
		.get_hw_params		= acp_dmic_dai_get_hw_params,
	},
};
