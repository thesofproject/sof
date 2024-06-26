// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2023 AMD. All rights reserved.
//
// Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//		SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>

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
#include <platform/acp_dmic_dma.h>

extern struct acp_dmic_silence acp_initsilence;

SOF_DEFINE_REG_UUID(acp_dmic_dai);

DECLARE_TR_CTX(acp_dmic_dai_tr, SOF_UUID(acp_dmic_dai_uuid), LOG_LEVEL_INFO);

static inline int acp_dmic_dai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
					  const void *spec_config)
{
	acp_wov_pdm_no_of_channels_t pdm_channels;
	const struct sof_ipc_dai_config *config = spec_config;
	struct acp_pdata *acpdata = dai_get_drvdata(dai);
	acp_wov_clk_ctrl_t clk_ctrl;

	acpdata->config = *config;
	acpdata->dmic_params = config->acpdmic;
	clk_ctrl = (acp_wov_clk_ctrl_t)io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_DMA_ENABLE);
	clk_ctrl.u32all = 0;
	pdm_channels.u32all = io_reg_read(PU_REGISTER_BASE + ACP_WOV_PDM_NO_OF_CHANNELS);
	acp_initsilence.samplerate_khz = acpdata->dmic_params.pdm_rate / 1000;
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
		dai_err(dai, "unsupported samplerate");
		return -EINVAL;
	}
	io_reg_write(PU_REGISTER_BASE + ACP_WOV_CLK_CTRL, clk_ctrl.u32all);
	acp_initsilence.num_chs = acpdata->dmic_params.pdm_ch;
	switch (acpdata->dmic_params.pdm_ch) {
	case 2:
		pdm_channels.bits.pdm_no_of_channels = 0;
		break;
	case 4:
		pdm_channels.bits.pdm_no_of_channels = 1;
		break;
	default:
		dai_err(dai, "acp_dmic_set_config unsupported channels");
		return -EINVAL;
	}
	io_reg_write(PU_REGISTER_BASE + ACP_WOV_PDM_NO_OF_CHANNELS,
		     pdm_channels.u32all);
	return 0;
}

static int acp_dmic_dai_trigger(struct dai *dai, int cmd, int direction)
{
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

	dai_info(dai, "dmic dai probe");
	/* allocate private data */
	acp = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*acp));
	if (!acp) {
		dai_err(dai, "dmic dai probe alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, acp);
	return 0;
}

static int acp_dmic_dai_remove(struct dai *dai)
{
	struct acp_pdata *acp = dai_get_drvdata(dai);

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
		dai_err(dai, "Invalid direction");
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
		dai_err(dai, "unsupported samplerate %d", acpdata->dmic_params.pdm_rate);
		return -EINVAL;
	}
	switch (acpdata->dmic_params.pdm_ch) {
	case 2:
	case 4:
		params->channels = acpdata->dmic_params.pdm_ch;
		break;
	default:
		dai_err(dai, "unsupported channels %d", acpdata->dmic_params.pdm_ch);
		return -EINVAL;
	}
	params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params->frame_fmt = SOF_IPC_FRAME_S32_LE;
	acp_initsilence.bytes_per_sample = 4;
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
