// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Anup Kulkarni <anup.kulkarni@amd.com>
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
SOF_DEFINE_UUID("spdai", spdai_uuid, 0x4abd71ba, 0x8619, 0x458a,
		0xb3, 0x3f, 0x16, 0x0f, 0xc0, 0xcf, 0x80, 0x9b);
DECLARE_TR_CTX(spdai_tr, SOF_UUID(spdai_uuid), LOG_LEVEL_INFO);

static inline int spdai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
				   const void *spec_config)
{
	const struct sof_ipc_dai_config *config = spec_config;
	struct acp_pdata *acpdata = dai_get_drvdata(dai);

	acp_i2stdm_iter_t sp_iter;
	acp_i2stdm_irer_t sp_irer;

	acpdata->config = *config;
	acpdata->params = config->acpsp;

	sp_iter = (acp_i2stdm_iter_t)io_reg_read(PU_REGISTER_BASE + ACP_I2STDM_ITER);
	sp_irer = (acp_i2stdm_irer_t)io_reg_read(PU_REGISTER_BASE + ACP_I2STDM_IRER);
	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_DSP_A:
		{
		acp_i2stdm_txfrmt_t i2stdm_txfrmt;
		acp_i2stdm_rxfrmt_t reg_i2stdm_rxfrmt;

		i2stdm_txfrmt.u32all = io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_TXFRMT));
		i2stdm_txfrmt.bits.i2stdm_num_slots = acpdata->params.tdm_slots;
		i2stdm_txfrmt.bits.i2stdm_slot_len = 16;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_TXFRMT), i2stdm_txfrmt.u32all);

		sp_iter.bits.i2stdm_tx_protocol_mode = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_ITER), sp_iter.u32all);

		reg_i2stdm_rxfrmt.u32all = io_reg_read((PU_REGISTER_BASE + ACP_I2STDM_RXFRMT));
		reg_i2stdm_rxfrmt.bits.i2stdm_num_slots = 2;
		reg_i2stdm_rxfrmt.bits.i2stdm_slot_len = 16;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_RXFRMT), reg_i2stdm_rxfrmt.u32all);

		sp_irer.bits.i2stdm_rx_protocol_mode = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_IRER), sp_irer.u32all);
		}
		break;
	case SOF_DAI_FMT_I2S:
		sp_iter.bits.i2stdm_tx_protocol_mode = 0;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_ITER), sp_iter.u32all);

		sp_irer.bits.i2stdm_rx_protocol_mode = 0;
		io_reg_write((PU_REGISTER_BASE + ACP_I2STDM_IRER), sp_irer.u32all);
		break;
	default:
		dai_err(dai, "hsdai_set_config invalid format");
		return -EINVAL;
		}
	return 0;
}

static int spdai_trigger(struct dai *dai, int cmd, int direction)
{
	/* nothing to do on renoir for SP dai */
	return 0;
}

static int spdai_probe(struct dai *dai)
{
	struct acp_pdata *acp;

	dai_info(dai, "SP dai probe");
	/* allocate private data */
	acp = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*acp));
	if (!acp) {
		dai_err(dai, "SP dai probe alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, acp);
	return 0;
}

static int spdai_remove(struct dai *dai)
{
	struct acp_pdata *acp = dai_get_drvdata(dai);

	rfree(acp);
	dai_set_drvdata(dai, NULL);
	return 0;
}

static int spdai_get_fifo(struct dai *dai, int direction, int stream_id)
{
	switch (direction) {
	case DAI_DIR_PLAYBACK:
	case DAI_DIR_CAPTURE:
		return dai_fifo(dai, direction);
	default:
		dai_err(dai, "spdai_get_fifo(): Invalid direction");
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
	struct acp_pdata *acpdata = dai_get_drvdata(dai);

	if (dir == DAI_DIR_PLAYBACK) {
		/* SP DAI currently supports only these parameters */
		params->rate = ACP_DEFAULT_SAMPLE_RATE;
		params->channels = acpdata->params.tdm_slots;
		params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
		params->frame_fmt = SOF_IPC_FRAME_S16_LE;
	} else if (dir == DAI_DIR_CAPTURE) {
		/* SP DAI currently supports only these parameters */
		params->rate = ACP_DEFAULT_SAMPLE_RATE;
		params->channels = 2;
		params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
		params->frame_fmt = SOF_IPC_FRAME_S16_LE;
	}
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


