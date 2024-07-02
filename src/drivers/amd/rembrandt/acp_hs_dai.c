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

SOF_DEFINE_REG_UUID(hsdai);
DECLARE_TR_CTX(hsdai_tr, SOF_UUID(hsdai_uuid), LOG_LEVEL_INFO);

static inline int hsdai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
				   const void *spec_config)
{
	const struct sof_ipc_dai_config *config = spec_config;
	struct acp_pdata *acpdata = dai_get_drvdata(dai);

	acp_hstdm_iter_t hs_iter;
	acp_hstdm_irer_t hs_irer;
	acp_i2stdm_mstrclkgen_t i2stdm_mstrclkgen;

	acpdata->config = *config;
	acpdata->params = config->acphs;
	i2stdm_mstrclkgen.u32all = io_reg_read(PU_REGISTER_BASE + ACP_I2STDM2_MSTRCLKGEN);
	i2stdm_mstrclkgen.bits.i2stdm_master_mode = 1;
	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_DSP_A:
		i2stdm_mstrclkgen.bits.i2stdm_format_mode = 1;
		switch (acpdata->params.tdm_slots) {
		case 2:
			i2stdm_mstrclkgen.bits.i2stdm_lrclk_div_val = 0x20;
			i2stdm_mstrclkgen.bits.i2stdm_bclk_div_val = 0x80;
			break;
		case 4:
			i2stdm_mstrclkgen.bits.i2stdm_lrclk_div_val = 0x40;
			i2stdm_mstrclkgen.bits.i2stdm_bclk_div_val = 0x40;
			break;
		case 6:
			i2stdm_mstrclkgen.bits.i2stdm_lrclk_div_val = 0x60;
			i2stdm_mstrclkgen.bits.i2stdm_bclk_div_val = 0x30;
			break;
		case 8:
			i2stdm_mstrclkgen.bits.i2stdm_lrclk_div_val = 0x80;
			i2stdm_mstrclkgen.bits.i2stdm_bclk_div_val = 0x20;
			break;
		default:
			dai_err(dai, "hsdai_set_config unsupported slots");
			return -EINVAL;
		}
		break;
	case SOF_DAI_FMT_I2S:
		i2stdm_mstrclkgen.bits.i2stdm_format_mode = 0;
		i2stdm_mstrclkgen.bits.i2stdm_lrclk_div_val = 0x20;
		i2stdm_mstrclkgen.bits.i2stdm_bclk_div_val = 0x80;
		break;
	default:
		dai_err(dai, "hsdai_set_config invalid format");
		return -EINVAL;
	}

	hs_iter = (acp_hstdm_iter_t)io_reg_read((PU_REGISTER_BASE + ACP_HSTDM_ITER));
	hs_irer = (acp_hstdm_irer_t)io_reg_read((PU_REGISTER_BASE + ACP_HSTDM_IRER));
	/* set master clk for hs dai */
	io_reg_write(PU_REGISTER_BASE + ACP_I2STDM2_MSTRCLKGEN, i2stdm_mstrclkgen.u32all);
	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_DSP_A:
		{
		acp_hstdm_txfrmt_t i2stdm_txfrmt;
		acp_hstdm_rxfrmt_t i2stdm_rxfrmt;

		i2stdm_txfrmt.u32all = io_reg_read(PU_REGISTER_BASE + ACP_HSTDM_TXFRMT);
		i2stdm_txfrmt.bits.hstdm_num_slots = acpdata->params.tdm_slots;
		i2stdm_txfrmt.bits.hstdm_slot_len = 16;
		io_reg_write((PU_REGISTER_BASE + ACP_HSTDM_TXFRMT), i2stdm_txfrmt.u32all);

		hs_iter.bits.hstdm_tx_protocol_mode = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_HSTDM_ITER), hs_iter.u32all);

		i2stdm_rxfrmt.u32all = io_reg_read(PU_REGISTER_BASE + ACP_HSTDM_RXFRMT);
		i2stdm_rxfrmt.bits.hstdm_num_slots = acpdata->params.tdm_slots;
		i2stdm_rxfrmt.bits.hstdm_slot_len = 16;
		io_reg_write((PU_REGISTER_BASE + ACP_HSTDM_RXFRMT), i2stdm_rxfrmt.u32all);

		hs_irer.bits.hstdm_rx_protocol_mode = 1;
		io_reg_write((PU_REGISTER_BASE + ACP_HSTDM_IRER), hs_irer.u32all);
		}
		break;
	case SOF_DAI_FMT_I2S:
		hs_iter.bits.hstdm_tx_protocol_mode = 0;
		io_reg_write((PU_REGISTER_BASE + ACP_HSTDM_ITER), hs_iter.u32all);

		hs_irer.bits.hstdm_rx_protocol_mode = 0;
		io_reg_write((PU_REGISTER_BASE + ACP_HSTDM_IRER), hs_irer.u32all);
		break;
	default:
		dai_err(dai, "hsdai_set_config invalid format");
		return -EINVAL;
	}
	return 0;
}

static int hsdai_trigger(struct dai *dai, int cmd, int direction)
{
	/* nothing to do on rembrandt for HS dai */
	return 0;
}

static int hsdai_probe(struct dai *dai)
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

static int hsdai_remove(struct dai *dai)
{
	struct acp_pdata *acp = dai_get_drvdata(dai);

	rfree(acp);
	dai_set_drvdata(dai, NULL);

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
	int handshake = dai->plat_data.fifo[direction].handshake;

	interrupt_get_irq(handshake, "irqsteer1");
	return dai->plat_data.fifo[direction].handshake;
}

static int hsdai_get_hw_params(struct dai *dai,
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
		params->channels = acpdata->params.tdm_slots;
		params->buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
		params->frame_fmt = SOF_IPC_FRAME_S16_LE;
	}

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
