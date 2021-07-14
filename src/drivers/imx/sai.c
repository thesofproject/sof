// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>
// Author: Jerome Laclavere <jerome.laclavere@nxp.com>
// Author: Guido Roncarolo <guido.roncarolo@nxp.com>

#include <sof/audio/component.h>
#include <sof/bit.h>
#include <sof/drivers/edma.h>
#include <sof/drivers/sai.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/wait.h>
#include <sof/lib/uuid.h>
#include <ipc/dai.h>
#include <errno.h>
#include <stdint.h>

/* 9302adf5-88be-4234-a0a7-dca538ef81f4 */
DECLARE_SOF_UUID("sai", sai_uuid, 0x9302adf5, 0x88be, 0x4234,
		 0xa0, 0xa7, 0xdc, 0xa5, 0x38, 0xef, 0x81, 0xf4);

DECLARE_TR_CTX(sai_tr, SOF_UUID(sai_uuid), LOG_LEVEL_INFO);

static bool sai_dir_is_synced(struct sai_pdata *sai, bool rx)
{
	if (rx)
		return (sai->sync_mode == SAI_SYNC_ON_TX);
	else
		return (sai->sync_mode == SAI_SYNC_ON_RX);
}

static void sai_start(struct dai *dai, int direction)
{
	struct sai_pdata *sai = dai_get_drvdata(dai);
	bool rx = (direction == DAI_DIR_CAPTURE);
	uint32_t xrd0 = (rx ? REG_SAI_RDR0 : REG_SAI_TDR0);
	uint32_t xcsr = 0;

	dai_info(dai, "SAI: sai_start %cX", rx ? 'R' : 'T');

	/* add one word to FIFO before TRCE is enabled */
	dai_write(dai, xrd0, 0x0);

	dai_update_bits(dai, REG_SAI_RCR2, REG_SAI_CR2_SYNC_MASK,
			sai_dir_is_synced(sai, true) ? REG_SAI_CR2_SYNC : 0);
	dai_update_bits(dai, REG_SAI_TCR2, REG_SAI_CR2_SYNC_MASK,
			sai_dir_is_synced(sai, false) ? REG_SAI_CR2_SYNC : 0);

	xcsr |= REG_SAI_CSR_FRDE; /* FIFO Request DMA Enable */
	xcsr |= REG_SAI_CSR_TERE; /* Transmit Enable */
	xcsr |= REG_SAI_CSR_SE;   /* Stop Enable */
	xcsr |= REG_SAI_CSR_SEIE; /* Sync Error Interrupt Enable */
	xcsr |= REG_SAI_CSR_FEIE; /* FIFO Error Interrupt Enable */
	xcsr |= REG_SAI_CSR_FEF;  /* W1C, clear FIFO Error Flag */
	xcsr |= REG_SAI_CSR_SEF;  /* W1C, clear Sync Error Flag */
	xcsr |= REG_SAI_CSR_WSF;  /* W1C, clear Word Start Flag */

	dai_update_bits(dai, REG_SAI_XCSR(rx), xcsr, xcsr);

	if (sai_dir_is_synced(sai, rx))
		dai_update_bits(dai, REG_SAI_XCSR(!rx), REG_SAI_CSR_TERE,
				REG_SAI_CSR_TERE);
#ifdef CONFIG_IMX8M
	dai_update_bits(dai, REG_SAI_MCTL, REG_SAI_MCTL_MCLK_EN,
			REG_SAI_MCTL_MCLK_EN);
#endif
}

static void sai_dir_disable(struct dai *dai, bool rx)
{
	struct sai_pdata *sai = dai_get_drvdata(dai);
	int ret;

	dai_update_bits(dai, REG_SAI_XCSR(rx), REG_SAI_CSR_TERE, 0);
	ret = poll_for_register_delay(dai_base(dai) + REG_SAI_XCSR(rx),
				      REG_SAI_CSR_TERE, 0, 100);
	if (ret < 0)
		dai_warn(dai, "sai: poll for register delay failed");

	dai_update_bits(dai, REG_SAI_XCSR(rx), REG_SAI_CSR_FR, REG_SAI_CSR_FR);

	if (!sai->consumer_mode) {
		/* Software Reset */
		dai_update_bits(dai, REG_SAI_XCSR(rx), REG_SAI_CSR_SR, REG_SAI_CSR_SR);
		/* Clear SR bit to finish the reset */
		dai_update_bits(dai, REG_SAI_XCSR(rx), REG_SAI_CSR_SR, 0U);
	}
}

static void sai_stop(struct dai *dai, int direction)
{
	struct sai_pdata *sai = dai_get_drvdata(dai);
	bool rx = (direction == DAI_DIR_CAPTURE);
	uint32_t xcsr = 0U;

	dai_info(dai, "SAI: sai_stop %cX", rx ? 'R' : 'T');

	xcsr |= REG_SAI_CSR_FRDE; /* FIFO Request DMA Enable */
	xcsr |= REG_SAI_CSR_SE;   /* Stop Enable */
	xcsr |= REG_SAI_CSR_SEIE; /* Sync Error Interrupt Enable */
	xcsr |= REG_SAI_CSR_FEIE; /* FIFO Error Interrupt Enable */

	dai_update_bits(dai, REG_SAI_XCSR(rx), xcsr, 0);

	/* Check if the opposite direction is also disabled */
	xcsr = dai_read(dai, REG_SAI_XCSR(!rx));

	/*
	 * Disable the opposite stream if:
	 * 1. it provides clocks for synchronous mode for current stream and,
	 * 2. is inactive, ie: ended before current stream
	 */
	if (sai_dir_is_synced(sai, rx) && !(xcsr & REG_SAI_CSR_FRDE))
		sai_dir_disable(dai, !rx);
	/*
	 * Disable current stream if:
	 * 1. it does not provide clocks for synchronous mode for opposite stream or,
	 * 2. the opposite stream is inactive, ie: ended before current stream
	 */
	if (!sai_dir_is_synced(sai, !rx) || !(xcsr & REG_SAI_CSR_FRDE))
		sai_dir_disable(dai, rx);

#ifdef CONFIG_IMX8M
	if (!(xcsr & REG_SAI_CSR_FRDE))
		dai_update_bits(dai, REG_SAI_MCTL, REG_SAI_MCTL_MCLK_EN, 0);
#endif
}

static int sai_context_store(struct dai *dai)
{
	return 0;
}

static int sai_context_restore(struct dai *dai)
{
	return 0;
}

static inline int sai_set_config(struct dai *dai, struct ipc_config_dai *common_config,
				 void *spec_config)
{
	dai_info(dai, "SAI: sai_set_config");
	struct sof_ipc_dai_config *config = spec_config;
	uint32_t val_cr2 = 0, val_cr4 = 0;
	uint32_t mask_cr2 = 0, mask_cr4 = 0;
	struct sai_pdata *sai = dai_get_drvdata(dai);

	sai->config = *config;
	sai->params = config->sai;

	val_cr4 |= REG_SAI_CR4_MF;
	sai->dsp_mode = false;

	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_I2S:
		/*
		 * Frame low, 1clk before data, one word length for frame sync,
		 * frame sync starts one serial clock cycle earlier,
		 * that is, together with the last bit of the previous
		 * data word.
		 */
		val_cr2 |= REG_SAI_CR2_BCP;
		val_cr4 |= REG_SAI_CR4_FSE | REG_SAI_CR4_FSP;
		break;
	case SOF_DAI_FMT_LEFT_J:
		/*
		 * Frame high, one word length for frame sync,
		 * frame sync asserts with the first bit of the frame.
		 */
		val_cr2 |= REG_SAI_CR2_BCP;
		break;
	case SOF_DAI_FMT_DSP_A:
		/*
		 * Frame high, 1clk before data, one bit for frame sync,
		 * frame sync starts one serial clock cycle earlier,
		 * that is, together with the last bit of the previous
		 * data word.
		 */
		val_cr2 |= REG_SAI_CR2_BCP;
		val_cr4 |= REG_SAI_CR4_FSE;
		sai->dsp_mode = true;
		break;
	case SOF_DAI_FMT_DSP_B:
		/*
		 * Frame high, one bit for frame sync,
		 * frame sync asserts with the first bit of the frame.
		 */
		val_cr2 |= REG_SAI_CR2_BCP;
		sai->dsp_mode = true;
		break;
	case SOF_DAI_FMT_PDM:
		val_cr2 |= REG_SAI_CR2_BCP;
		val_cr4 &= ~REG_SAI_CR4_MF;
		sai->dsp_mode = true;
		break;
	case SOF_DAI_FMT_RIGHT_J:
		/* To be done, currently not supported */
	default:
		return -EINVAL;
	}

	/* DAI clock inversion */
	switch (config->format & SOF_DAI_FMT_INV_MASK) {
	case SOF_DAI_FMT_IB_IF:
		/* Invert both clocks */
		val_cr2 ^= REG_SAI_CR2_BCP;
		val_cr4 ^= REG_SAI_CR4_FSP;
		break;
	case SOF_DAI_FMT_IB_NF:
		/* Invert bit clock */
		val_cr2 ^= REG_SAI_CR2_BCP;
		break;
	case SOF_DAI_FMT_NB_IF:
		/* Invert frame clock */
		val_cr4 ^= REG_SAI_CR4_FSP;
		break;
	case SOF_DAI_FMT_NB_NF:
		/* Nothing to do for both normal cases */
		break;
	default:
		return -EINVAL;
	}

	sai->consumer_mode = false;

	/* DAI clock provider masks */
	switch (config->format & SOF_DAI_FMT_CLOCK_PROVIDER_MASK) {
	case SOF_DAI_FMT_CBC_CFC:
		dai_info(dai, "SAI: codec is consumer");
		val_cr2 |= REG_SAI_CR2_BCD_MSTR;
		val_cr4 |= REG_SAI_CR4_FSD_MSTR;
		break;
	case SOF_DAI_FMT_CBP_CFP:
		dai_info(dai, "SAI: codec is provider");
		/*
		 * fields CR2_BCD and CR4_MFSD already at 0
		 */
		sai->consumer_mode = true;
		break;
	case SOF_DAI_FMT_CBC_CFP:
		val_cr2 |= REG_SAI_CR2_BCD_MSTR;
		break;
	case SOF_DAI_FMT_CBP_CFC:
		val_cr4 |= REG_SAI_CR4_FSD_MSTR;
		sai->consumer_mode = true;
		break;
	default:
		return -EINVAL;
	}

	mask_cr2  = REG_SAI_CR2_BCP | REG_SAI_CR2_BCD_MSTR;
	mask_cr4  = REG_SAI_CR4_MF | REG_SAI_CR4_FSE |
			REG_SAI_CR4_FSP | REG_SAI_CR4_FSD_MSTR;

	dai_update_bits(dai, REG_SAI_TCR1, REG_SAI_CR1_RFW_MASK,
			dai->plat_data.fifo[DAI_DIR_PLAYBACK].watermark);
	dai_update_bits(dai, REG_SAI_TCR2, mask_cr2, val_cr2);
	dai_update_bits(dai, REG_SAI_TCR4, mask_cr4, val_cr4);

	dai_update_bits(dai, REG_SAI_RCR1, REG_SAI_CR1_RFW_MASK,
			dai->plat_data.fifo[DAI_DIR_CAPTURE].watermark);
	dai_update_bits(dai, REG_SAI_RCR2, mask_cr2, val_cr2);
	dai_update_bits(dai, REG_SAI_RCR4, mask_cr4, val_cr4);

	return 0;
}

static int sai_trigger(struct dai *dai, int cmd, int direction)
{
	dai_info(dai, "SAI: sai_trigger");

	switch (cmd) {
	case COMP_TRIGGER_START:
		sai_start(dai, direction);
		break;
	case COMP_TRIGGER_STOP:
		sai_stop(dai, direction);
		break;
	case COMP_TRIGGER_PAUSE:
		sai_stop(dai, direction);
		break;
	case COMP_TRIGGER_RELEASE:
		break;
	case COMP_TRIGGER_SUSPEND:
		break;
	case COMP_TRIGGER_RESUME:
		break;
	default:
		dai_err(dai, "SAI: invalid trigger cmd %d", cmd);
		break;
	}
	return 0;
}

static int sai_probe(struct dai *dai)
{
	struct sai_pdata *sai;

	dai_info(dai, "SAI: sai_probe");

	/* allocate private data */
	sai = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*sai));
	if (!sai) {
		dai_err(dai, "sai_probe(): alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, sai);

	/**
	 * Synchronize RX on TX by default
	 * @todo: to provide via topology and sai_set_config
	 */
	sai->sync_mode = SAI_SYNC_ON_TX;

	/* Software Reset for both Tx and Rx */
	dai_update_bits(dai, REG_SAI_TCSR, REG_SAI_CSR_SR, REG_SAI_CSR_SR);
	dai_update_bits(dai, REG_SAI_RCSR, REG_SAI_CSR_SR, REG_SAI_CSR_SR);

	/* Clear SR bit to finish the reset */
	dai_update_bits(dai, REG_SAI_TCSR, REG_SAI_CSR_SR, 0U);
	dai_update_bits(dai, REG_SAI_RCSR, REG_SAI_CSR_SR, 0U);

	/* Reset all Tx register values */
	dai_write(dai, REG_SAI_TCR2, 0U);
	dai_write(dai, REG_SAI_TCR3, 0U);
	dai_write(dai, REG_SAI_TCR4, 0U);
	dai_write(dai, REG_SAI_TCR5, 0U);
	dai_write(dai, REG_SAI_TMR,  0U);

	/* Reset all Rx register values */
	dai_write(dai, REG_SAI_RCR2, 0U);
	dai_write(dai, REG_SAI_RCR3, 0U);
	dai_write(dai, REG_SAI_RCR4, 0U);
	dai_write(dai, REG_SAI_RCR5, 0U);
	dai_write(dai, REG_SAI_RMR,  0U);

	return 0;
}

static int sai_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[direction].handshake;
}

static int sai_get_fifo(struct dai *dai, int direction, int stream_id)
{
	switch (direction) {
	case DAI_DIR_PLAYBACK:
	case DAI_DIR_CAPTURE:
		return dai_fifo(dai, direction); /* stream_id is unused */
	default:
		dai_err(dai, "sai_get_fifo(): Invalid direction");
		return -EINVAL;
	}
}

static int sai_get_hw_params(struct dai *dai,
			     struct sof_ipc_stream_params *params,
			     int dir)
{
	struct sai_pdata *sai = dai_get_drvdata(dai);

	/* SAI only currently supports these parameters */
	params->rate = sai->params.fsync_rate;
	params->channels = 2;
	params->buffer_fmt = 0;
	params->frame_fmt = SOF_IPC_FRAME_S32_LE;

	return 0;
}

static int sai_hw_params(struct dai *dai,
			 struct sof_ipc_stream_params *params)
{
	struct sai_pdata *sai = dai_get_drvdata(dai);
	bool rx = (params->direction == DAI_DIR_CAPTURE);
	uint32_t rate = params->rate;
	uint32_t channels = params->channels;
	uint32_t word_width = 8 * get_sample_bytes(params->frame_fmt);
	uint32_t val_cr2 = 0, val_cr3 = 0, val_cr4 = 0, val_cr5 = 0;
	uint32_t mask_cr2 = 0, mask_cr3 = 0, mask_cr4 = 0, mask_cr5 = 0;
	uint32_t slots = (channels == 1) ? 2 : channels;
	uint32_t slot_width = word_width;
	uint32_t pins, bclk, ratio, div;

	dai_info(dai, "SAI: sai_hw_params %cX", rx ? 'R' : 'T');

	/* Overwrite the number of slots with value from topology if != 0 */
	if (sai->params.tdm_slots)
		slots = sai->params.tdm_slots;

	/* Overwrite the slot width with value from topology if != 0 */
	if (sai->params.tdm_slot_width)
		slot_width = sai->params.tdm_slot_width;

	pins = DIV_ROUND_UP(channels, slots);
	bclk = rate * slots * slot_width;

	dai_update_bits(dai, REG_SAI_XCR1(rx), REG_SAI_CR1_RFW_MASK,
			dai->plat_data.fifo[params->direction].watermark);

	mask_cr3 = REG_SAI_CR3_TRCE_MASK;
	val_cr3 = REG_SAI_CR3_TRCE(((1 << pins) - 1));

	mask_cr4 = REG_SAI_CR4_SYWD_MASK | REG_SAI_CR4_FRSZ_MASK | REG_SAI_CR4_CHMOD_MASK;
	val_cr4 |= (sai->dsp_mode ? 0 : REG_SAI_CR4_SYWD(slot_width));
	val_cr4 |= REG_SAI_CR4_FRSZ(slots);
	val_cr4 |= (rx ? 0 : REG_SAI_CR4_CHMOD);

	mask_cr5 = REG_SAI_CR5_WNW_MASK | REG_SAI_CR5_W0W_MASK | REG_SAI_CR5_FBT_MASK;
	val_cr5  = REG_SAI_CR5_WNW(slot_width) | REG_SAI_CR5_W0W(slot_width);
	val_cr5 |= REG_SAI_CR5_FBT(slot_width);
	dai_update_bits(dai, REG_SAI_XCR3(rx), mask_cr3, val_cr3);
	dai_update_bits(dai, REG_SAI_XCR4(rx), mask_cr4, val_cr4);
	dai_update_bits(dai, REG_SAI_XCR5(rx), mask_cr5, val_cr5);
	dai_update_bits(dai, REG_SAI_XMR(rx), REG_SAI_XMR_MASK,
			~0UL - ((1 << MIN(channels, slots)) - 1));

	if (sai->consumer_mode)
		return 0;

	/* The following section configures SAI clocks provider mode */
	ratio = sai->params.mclk_rate / bclk;
	div = (ratio == 1 ? 0 : (ratio >> 1) - 1);

	mask_cr2 = REG_SAI_CR2_MSEL_MASK | REG_SAI_CR2_DIV_MASK;
	val_cr2  = REG_SAI_CR2_MSEL_MCLK1 | div;
#ifdef CONFIG_IMX8M
	mask_cr2 |= REG_SAI_CR2_BYP;
	val_cr2  |= (ratio == 1 ? REG_SAI_CR2_BYP : 0);
#endif

	if (sai_dir_is_synced(sai, rx)) {
		dai_update_bits(dai, REG_SAI_XCR2(rx), mask_cr2, 0);
		dai_update_bits(dai, REG_SAI_XCR2(!rx), mask_cr2, val_cr2);

		dai_update_bits(dai, REG_SAI_XCR1(!rx), REG_SAI_CR1_RFW_MASK,
				dai->plat_data.fifo[(params->direction + 1) % 2].watermark);
		dai_update_bits(dai, REG_SAI_XCR4(!rx), mask_cr4, val_cr4);
		dai_update_bits(dai, REG_SAI_XCR5(!rx), mask_cr5, val_cr5);
	} else {
		dai_update_bits(dai, REG_SAI_XCR2(rx), mask_cr2, val_cr2);
	}

	return 0;
}

const struct dai_driver sai_driver = {
	.type = SOF_DAI_IMX_SAI,
	.uid = SOF_UUID(sai_uuid),
	.tctx = &sai_tr,
	.dma_dev = DMA_DEV_SAI,
	.ops = {
		.trigger		= sai_trigger,
		.set_config		= sai_set_config,
		.pm_context_store	= sai_context_store,
		.pm_context_restore	= sai_context_restore,
		.probe			= sai_probe,
		.get_handshake		= sai_get_handshake,
		.get_fifo		= sai_get_fifo,
		.get_hw_params		= sai_get_hw_params,
		.hw_params		= sai_hw_params,
	},
};
