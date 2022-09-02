// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/drivers/ssp.h>
#include <rtos/alloc.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/shim.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <ipc/dai-intel.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

/* Digital Audio interface formatting */
static int ssp_set_config(struct dai *dai, struct ipc_config_dai *common_config,
			  void *spec_config)
{
	struct sof_ipc_dai_config *config = spec_config;
	struct ssp_pdata *ssp = dai_get_drvdata(dai);
	uint32_t sscr0;
	uint32_t sscr1;
	uint32_t sscr2;
	uint32_t sspsp;
	uint32_t sspsp2;
	uint32_t mdiv;
	uint32_t bdiv;
	uint32_t data_size;
	uint32_t start_delay;
	uint32_t frame_end_padding;
	uint32_t slot_end_padding;
	uint32_t frame_len = 0;
	uint32_t bdiv_min;
	uint32_t format;
	bool inverted_frame = false;
	int ret = 0;
	k_spinlock_key_t key;

	key = k_spin_lock(&dai->lock);

	/* is playback/capture already running */
	if (ssp->state[DAI_DIR_PLAYBACK] == COMP_STATE_ACTIVE ||
	    ssp->state[DAI_DIR_CAPTURE] == COMP_STATE_ACTIVE) {
		dai_info(dai, "ssp_set_config(): playback/capture active. Ignore config");
		goto out;
	}

	dai_info(dai, "ssp_set_config()");

	/* disable clock */
	shim_update_bits(SHIM_CLKCTL, SHIM_CLKCTL_EN_SSP(dai->index), 0);

	/* enable MCLK */
	shim_update_bits(SHIM_CLKCTL, SHIM_CLKCTL_SMOS(0x3),
			 SHIM_CLKCTL_SMOS(0x3));

	/* reset SSP settings */
	/* sscr0 dynamic settings are DSS, EDSS, SCR, FRDC, ECS */
	sscr0 = SSCR0_MOD | SSCR0_PSP;

	/* sscr1 dynamic settings are TFT, RFT, SFRMDIR, SCLKDIR, SCFR */
	sscr1 = SSCR1_TTE | SSCR1_TTELP;

	/* enable Transmit underrun mode 1 */
	sscr2 = SSCR2_TURM1;

	/* sspsp dynamic settings are SCMODE, SFRMP, DMYSTRT, SFRMWDTH */
	sspsp = 0x0;

	/* sspsp2 no dynamic setting */
	sspsp2 = 0x0;

	ssp->config = *config;
	ssp->params = config->ssp;

	switch (config->format & SOF_DAI_FMT_CLOCK_PROVIDER_MASK) {
	case SOF_DAI_FMT_CBP_CFP:
		sscr1 |= SSCR1_SCLKDIR | SSCR1_SFRMDIR;
#ifdef ENABLE_SSRCR1_SCFR
		sscr1 |= SSCR1_SCFR;
#endif
		break;
	case SOF_DAI_FMT_CBC_CFC:
		break;
	case SOF_DAI_FMT_CBP_CFC:
		sscr1 |= SSCR1_SCLKDIR;
#ifdef ENABLE_SSRCR1_SCFR
		sscr1 |= SSCR1_SCFR;
#endif
		break;
	case SOF_DAI_FMT_CBC_CFP:
		sscr1 |= SSCR1_SFRMDIR;
		break;
	default:
		dai_err(dai, "ssp_set_config(): format & PROVIDER_MASK EINVAL");
		ret = -EINVAL;
		goto out;
	}

	/* clock signal polarity */
	switch (config->format & SOF_DAI_FMT_INV_MASK) {
	case SOF_DAI_FMT_NB_NF:
		break;
	case SOF_DAI_FMT_NB_IF:
		inverted_frame = true; /* handled later with format */
		break;
	case SOF_DAI_FMT_IB_IF:
		sspsp |= SSPSP_SCMODE(2);
		inverted_frame = true; /* handled later with format */
		break;
	case SOF_DAI_FMT_IB_NF:
		sspsp |= SSPSP_SCMODE(2);
		break;
	default:
		dai_err(dai, "ssp_set_config(): format & INV_MASK EINVAL");
		ret = -EINVAL;
		goto out;
	}

	/* Additional hardware settings */

	/* Receiver Time-out Interrupt Disabled/Enabled */
	sscr1 |= (ssp->params.quirks & SOF_DAI_INTEL_SSP_QUIRK_TINTE) ?
		SSCR1_TINTE : 0;

	/* Peripheral Trailing Byte Interrupts Disable/Enable */
	sscr1 |= (ssp->params.quirks & SOF_DAI_INTEL_SSP_QUIRK_PINTE) ?
		SSCR1_PINTE : 0;

	/* Enable/disable internal loopback. Output of transmit serial
	 * shifter connected to input of receive serial shifter, internally.
	 */
	sscr1 |= (ssp->params.quirks & SOF_DAI_INTEL_SSP_QUIRK_LBM) ?
		SSCR1_LBM : 0;

	/* Transmit data are driven at the same/opposite clock edge specified
	 * in SSPSP.SCMODE[1:0]
	 */
	sscr2 |= (ssp->params.quirks & SOF_DAI_INTEL_SSP_QUIRK_SMTATF) ?
		SSCR2_SMTATF : 0;

	/* Receive data are sampled at the same/opposite clock edge specified
	 * in SSPSP.SCMODE[1:0]
	 */
	sscr2 |= (ssp->params.quirks & SOF_DAI_INTEL_SSP_QUIRK_MMRATF) ?
		SSCR2_MMRATF : 0;

	/* Enable/disable the fix for PSP consumer mode TXD wait for frame
	 * de-assertion before starting the second channel
	 */
	sscr2 |= (ssp->params.quirks & SOF_DAI_INTEL_SSP_QUIRK_PSPSTWFDFD) ?
		SSCR2_PSPSTWFDFD : 0;

	/* Enable/disable the fix for PSP provider mode FSRT with dummy stop &
	 * frame end padding capability
	 */
	sscr2 |= (ssp->params.quirks & SOF_DAI_INTEL_SSP_QUIRK_PSPSRWFDFD) ?
		SSCR2_PSPSRWFDFD : 0;

	/* BCLK is generated from MCLK - must be divisable */
	if (config->ssp.mclk_rate % config->ssp.bclk_rate) {
		dai_err(dai, "ssp_set_config(): MCLK is not divisable");
		ret = -EINVAL;
		goto out;
	}

	/* divisor must be within SCR range */
	mdiv = (config->ssp.mclk_rate / config->ssp.bclk_rate) - 1;
	if (mdiv > (SSCR0_SCR_MASK >> 8)) {
		dai_err(dai, "ssp_set_config(): divisor is not within SCR range");
		ret = -EINVAL;
		goto out;
	}

	/* set the SCR divisor */
	sscr0 |= SSCR0_SCR(mdiv);

	/* calc frame width based on BCLK and rate - must be divisable */
	if (config->ssp.bclk_rate % config->ssp.fsync_rate) {
		dai_err(dai, "ssp_set_config(): BLCK is not divisable");
		ret = -EINVAL;
		goto out;
	}

	/* must be enouch BCLKs for data */
	bdiv = config->ssp.bclk_rate / config->ssp.fsync_rate;
	if (bdiv < config->ssp.tdm_slot_width *
	    config->ssp.tdm_slots) {
		dai_err(dai, "ssp_set_config(): not enough BCLKs");
		ret = -EINVAL;
		goto out;
	}

	/* tdm_slot_width must be <= 38 for SSP */
	if (config->ssp.tdm_slot_width > 38) {
		dai_err(dai, "ssp_set_config(): tdm_slot_width > 38");
		ret = -EINVAL;
		goto out;
	}

	bdiv_min = config->ssp.tdm_slots * config->ssp.sample_valid_bits;
	if (bdiv < bdiv_min) {
		dai_err(dai, "ssp_set_config(): bdiv < bdiv_min");
		ret = -EINVAL;
		goto out;
	}

	frame_end_padding = bdiv - bdiv_min;
	if (frame_end_padding > SSPSP2_FEP_MASK) {
		dai_err(dai, "ssp_set_config(): frame_end_padding > SSPSP2_FEP_MASK");
		ret = -EINVAL;
		goto out;
	}

	/* format */
	format = config->format & SOF_DAI_FMT_FORMAT_MASK;
	switch (format) {
	case SOF_DAI_FMT_I2S:
	case SOF_DAI_FMT_LEFT_J:

		if (format == SOF_DAI_FMT_I2S) {
			start_delay = 1;

		/*
		 * handle frame polarity, I2S default is falling/active low,
		 * non-inverted(inverted_frame=0) -- active low(SFRMP=0),
		 * inverted(inverted_frame=1) -- rising/active high(SFRMP=1),
		 * so, we should set SFRMP to inverted_frame.
		 */
			sspsp |= SSPSP_SFRMP(inverted_frame);
			sspsp |= SSPSP_FSRT;

		} else {
			start_delay = 0;

		/*
		 * handle frame polarity, LEFT_J default is rising/active high,
		 * non-inverted(inverted_frame=0) -- active high(SFRMP=1),
		 * inverted(inverted_frame=1) -- falling/active low(SFRMP=0),
		 * so, we should set SFRMP to !inverted_frame.
		 */
			sspsp |= SSPSP_SFRMP(!inverted_frame);
		}

		sscr0 |= SSCR0_FRDC(config->ssp.tdm_slots);

		if (bdiv % 2) {
			dai_err(dai, "ssp_set_config(): bdiv is not divisible by 2");
			ret = -EINVAL;
			goto out;
		}

		/* set asserted frame length to half frame length */
		frame_len = bdiv / 2;

		/*
		 *  for I2S/LEFT_J, the padding has to happen at the end
		 * of each slot
		 */
		if (frame_end_padding % 2) {
			dai_err(dai, "ssp_set_config(): frame_end_padding is not divisible by 2");
			ret = -EINVAL;
			goto out;
		}

		slot_end_padding = frame_end_padding / 2;

		if (slot_end_padding > 15) {
			/* can't handle padding over 15 bits */
			dai_err(dai, "ssp_set_config(): slot_end_padding over 15 bits");
			ret = -EINVAL;
			goto out;
		}

		sspsp |= SSPSP_DMYSTOP(slot_end_padding);
		slot_end_padding >>= SSPSP_DMYSTOP_BITS;
		sspsp |= SSPSP_EDMYSTOP(slot_end_padding);

		break;
	case SOF_DAI_FMT_DSP_A:

		start_delay = 1;

		sscr0 |= SSCR0_FRDC(config->ssp.tdm_slots);

		/* set asserted frame length */
		frame_len = 1;

		/* handle frame polarity, DSP_A default is rising/active high */
		sspsp |= SSPSP_SFRMP(!inverted_frame);
		sspsp2 |= (frame_end_padding & SSPSP2_FEP_MASK);

		break;
	case SOF_DAI_FMT_DSP_B:

		start_delay = 0;

		sscr0 |= SSCR0_FRDC(config->ssp.tdm_slots);

		/* set asserted frame length */
		frame_len = 1;

		/* handle frame polarity, DSP_B default is rising/active high */
		sspsp |= SSPSP_SFRMP(!inverted_frame);
		sspsp2 |= (frame_end_padding & SSPSP2_FEP_MASK);

		break;
	default:
		dai_err(dai, "ssp_set_config(): invalid format");
		ret = -EINVAL;
		goto out;
	}

	if (start_delay)
		sspsp |= SSPSP_FSRT;

	sspsp |= SSPSP_SFRMWDTH(frame_len);

	data_size = config->ssp.sample_valid_bits;

	if (data_size > 16)
		sscr0 |= (SSCR0_EDSS | SSCR0_DSIZE(data_size - 16));
	else
		sscr0 |= SSCR0_DSIZE(data_size);

	sscr1 |= SSCR1_TFT(0x8) | SSCR1_RFT(0x8);

	ssp_write(dai, SSCR0, sscr0);
	ssp_write(dai, SSCR1, sscr1);
	ssp_write(dai, SSCR2, sscr2);
	ssp_write(dai, SSPSP, sspsp);
	ssp_write(dai, SSTSA, SSTSA_SSTSA(config->ssp.tx_slots));
	ssp_write(dai, SSRSA, SSRSA_SSRSA(config->ssp.rx_slots));
	ssp_write(dai, SSPSP2, sspsp2);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_PREPARE;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_PREPARE;

	/* enable clock */
	shim_update_bits(SHIM_CLKCTL, SHIM_CLKCTL_EN_SSP(dai->index),
			 SHIM_CLKCTL_EN_SSP(dai->index));

	/* enable free running clock */
	ssp_update_bits(dai, SSCR0, SSCR0_SSE, SSCR0_SSE);
	ssp_update_bits(dai, SSCR0, SSCR0_SSE, 0);

	dai_info(dai, "ssp_set_config(), done");

out:
	k_spin_unlock(&dai->lock, key);

	return ret;
}

/* get SSP hw params */
static int ssp_get_hw_params(struct dai *dai,
			     struct sof_ipc_stream_params  *params, int dir)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	params->rate = ssp->params.fsync_rate;
	params->buffer_fmt = 0;

	if (dir == SOF_IPC_STREAM_PLAYBACK)
		params->channels = popcount(ssp->params.tx_slots);
	else
		params->channels = popcount(ssp->params.rx_slots);

	switch (ssp->params.sample_valid_bits) {
	case 16:
		params->frame_fmt = SOF_IPC_FRAME_S16_LE;
		break;
	case 24:
		params->frame_fmt = SOF_IPC_FRAME_S24_4LE;
		break;
	case 32:
		params->frame_fmt = SOF_IPC_FRAME_S32_LE;
		break;
	default:
		dai_err(dai, "ssp_get_hw_params(): not supported format");
		return -EINVAL;
	}

	return 0;
}

/* start the SSP for either playback or capture */
static void ssp_start(struct dai *dai, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);
	k_spinlock_key_t key;

	key = k_spin_lock(&dai->lock);

	dai_info(dai, "ssp_start()");

	/* enable DMA */
	if (direction == DAI_DIR_PLAYBACK) {
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE | SSCR1_EBCEI,
				SSCR1_TSRE | SSCR1_EBCEI);
		ssp_update_bits(dai, SSCR0, SSCR0_SSE, SSCR0_SSE);
		ssp_update_bits(dai, SSCR0, SSCR0_TIM, 0);
		ssp_update_bits(dai, SSTSA, SSTSA_TSEN, SSTSA_TSEN);
	} else {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE | SSCR1_EBCEI,
				SSCR1_RSRE | SSCR1_EBCEI);
		ssp_update_bits(dai, SSCR0, SSCR0_SSE, SSCR0_SSE);
		ssp_update_bits(dai, SSCR0, SSCR0_RIM, 0);
		ssp_update_bits(dai, SSRSA, SSRSA_RSEN, SSRSA_RSEN);
	}

	/* enable port */
	ssp->state[direction] = COMP_STATE_ACTIVE;

	k_spin_unlock(&dai->lock, key);
}

/* stop the SSP for either playback or capture */
static void ssp_stop(struct dai *dai, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);
	k_spinlock_key_t key;

	key = k_spin_lock(&dai->lock);

	/* stop Rx if neeed */
	if (direction == DAI_DIR_CAPTURE &&
	    ssp->state[SOF_IPC_STREAM_CAPTURE] != COMP_STATE_PREPARE) {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, 0);
		ssp_update_bits(dai, SSCR0, SSCR0_RIM, SSCR0_RIM);
		ssp_update_bits(dai, SSRSA, SSRSA_RSEN, 0);
		ssp->state[SOF_IPC_STREAM_CAPTURE] = COMP_STATE_PREPARE;
		dai_info(dai, "ssp_stop(), RX stop");
	}

	/* stop Tx if needed */
	if (direction == DAI_DIR_PLAYBACK &&
	    ssp->state[SOF_IPC_STREAM_PLAYBACK] != COMP_STATE_PREPARE) {
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, 0);
		ssp_update_bits(dai, SSCR0, SSCR0_TIM, SSCR0_TIM);
		ssp_update_bits(dai, SSTSA, SSTSA_TSEN, 0);
		ssp->state[SOF_IPC_STREAM_PLAYBACK] = COMP_STATE_PREPARE;
		dai_info(dai, "ssp_stop(), TX stop");
	}

	/* disable SSP port if no users */
	if (ssp->state[SOF_IPC_STREAM_CAPTURE] == COMP_STATE_PREPARE &&
	    ssp->state[SOF_IPC_STREAM_PLAYBACK] == COMP_STATE_PREPARE) {
		ssp_update_bits(dai, SSCR0, SSCR0_SSE, 0);
		dai_info(dai, "ssp_stop(), SSP port disabled");
	}

	k_spin_unlock(&dai->lock, key);
}

static void ssp_pause(struct dai *dai, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	if (direction == SOF_IPC_STREAM_CAPTURE)
		dai_info(dai, "ssp_pause(), RX");
	else
		dai_info(dai, "ssp_pause(), TX");

	ssp->state[direction] = COMP_STATE_PAUSED;
}

static int ssp_trigger(struct dai *dai, int cmd, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	dai_info(dai, "ssp_trigger()");

	switch (cmd) {
	case COMP_TRIGGER_START:
	case COMP_TRIGGER_RELEASE:
		if (ssp->state[direction] == COMP_STATE_PAUSED ||
		    ssp->state[direction] == COMP_STATE_PREPARE)
			ssp_start(dai, direction);
		break;
	case COMP_TRIGGER_STOP:
		ssp_stop(dai, direction);
		break;
	case COMP_TRIGGER_PAUSE:
		ssp_pause(dai, direction);
		break;
	}

	return 0;
}

static int ssp_probe(struct dai *dai)
{
	struct ssp_pdata *ssp;

	/* allocate private data */
	ssp = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*ssp));
	dai_set_drvdata(dai, ssp);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_READY;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_READY;

	return 0;
}

static int ssp_get_handshake(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[direction].handshake;
}

static int ssp_get_fifo(struct dai *dai, int direction, int stream_id)
{
	return dai->plat_data.fifo[direction].offset;
}

const struct dai_driver ssp_driver = {
	.type = SOF_DAI_INTEL_SSP,
	.dma_caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP,
	.dma_dev = DMA_DEV_SSP,
	.ops = {
		.trigger		= ssp_trigger,
		.set_config		= ssp_set_config,
		.get_hw_params		= ssp_get_hw_params,
		.get_handshake		= ssp_get_handshake,
		.get_fifo		= ssp_get_fifo,
		.probe			= ssp_probe,
	},
};
