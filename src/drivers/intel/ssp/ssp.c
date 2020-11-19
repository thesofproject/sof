// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@linux.intel.com>

#include <sof/audio/component.h>
#include <sof/common.h>
#include <sof/drivers/mn.h>
#include <sof/drivers/timestamp.h>
#include <sof/drivers/ssp.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <ipc/dai-intel.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

/* 31458125-95c4-4085-8f3f-497434cb2daf */
DECLARE_SOF_UUID("ssp-dai", ssp_uuid, 0x31458125, 0x95c4, 0x4085,
		 0x8f, 0x3f, 0x49, 0x74, 0x34, 0xcb, 0x2d, 0xaf);

DECLARE_TR_CTX(ssp_tr, SOF_UUID(ssp_uuid), LOG_LEVEL_INFO);

/* empty SSP transmit FIFO */
static void ssp_empty_tx_fifo(struct dai *dai)
{
	int ret;
	uint32_t sssr;

	/*
	 * SSSR_TNF is cleared when TX FIFO is empty or full,
	 * so wait for set TNF then for TFL zero - order matter.
	 */
	ret = poll_for_register_delay(dai_base(dai) + SSSR, SSSR_TNF, SSSR_TNF,
				      SSP_MAX_SEND_TIME_PER_SAMPLE);
	ret |= poll_for_register_delay(dai_base(dai) + SSCR3, SSCR3_TFL_MASK, 0,
				       SSP_MAX_SEND_TIME_PER_SAMPLE *
				       (SSP_FIFO_DEPTH - 1) / 2);

	if (ret)
		dai_warn(dai, "ssp_empty_tx_fifo() warning: timeout");

	sssr = ssp_read(dai, SSSR);

	/* clear interrupt */
	if (sssr & SSSR_TUR)
		ssp_write(dai, SSSR, sssr);
}

/* empty SSP receive FIFO */
static void ssp_empty_rx_fifo(struct dai *dai)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);
	uint64_t sample_ticks = clock_ticks_per_sample(PLATFORM_DEFAULT_CLOCK,
						       ssp->params.fsync_rate);
	uint32_t retry = SSP_RX_FLUSH_RETRY_MAX;
	uint32_t entries;
	uint32_t i;

	/*
	 * To make sure all the RX FIFO entries are read out for the flushing,
	 * we need to wait a minimal SSP port delay after entries are all read,
	 * and then re-check to see if there is any subsequent entries written
	 * to the FIFO. This will help to make sure there is no sample mismatched
	 * issue for the next run with the SSP RX.
	 */
	while ((ssp_read(dai, SSSR) & SSSR_RNE) && retry--) {
		entries = SSCR3_RFL_VAL(ssp_read(dai, SSCR3));
		dai_dbg(dai, "ssp_empty_rx_fifo(), before flushing, entries %d", entries);
		for (i = 0; i < entries + 1; i++)
			/* read to try empty fifo */
			ssp_read(dai, SSDR);

		/* wait to get valid fifo status and re-check */
		wait_delay(sample_ticks);
		entries = SSCR3_RFL_VAL(ssp_read(dai, SSCR3));
		dai_dbg(dai, "ssp_empty_rx_fifo(), after flushing, entries %d", entries);
	}

	/* clear interrupt */
	ssp_update_bits(dai, SSSR, SSSR_ROR, SSSR_ROR);
}

/* save SSP context prior to entering D3 */
static int ssp_context_store(struct dai *dai)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	ssp->sscr0 = ssp_read(dai, SSCR0);
	ssp->sscr1 = ssp_read(dai, SSCR1);

	/* FIXME: need to store sscr2,3,4,5 */
	ssp->psp = ssp_read(dai, SSPSP);

	return 0;
}

/* restore SSP context after leaving D3 */
static int ssp_context_restore(struct dai *dai)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	ssp_write(dai, SSCR0, ssp->sscr0);
	ssp_write(dai, SSCR1, ssp->sscr1);
	/* FIXME: need to restore sscr2,3,4,5 */
	ssp_write(dai, SSPSP, ssp->psp);

	return 0;
}

/* Digital Audio interface formatting */
static int ssp_set_config(struct dai *dai,
			  struct sof_ipc_dai_config *config)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);
	uint32_t sscr0;
	uint32_t sscr1;
	uint32_t sscr2;
	uint32_t sscr3;
	uint32_t sspsp;
	uint32_t sspsp2;
	uint32_t sstsa;
	uint32_t ssrsa;
	uint32_t ssto;
	uint32_t ssioc;
	uint32_t bdiv;
	uint32_t data_size;
	uint32_t frame_end_padding;
	uint32_t slot_end_padding;
	uint32_t frame_len = 0;
	uint32_t bdiv_min;
	uint32_t tft;
	uint32_t rft;
	uint32_t active_tx_slots = 2;
	uint32_t active_rx_slots = 2;
	uint32_t sample_width = 2;

	bool inverted_bclk = false;
	bool inverted_frame = false;
	bool cfs = false;
	bool start_delay = false;

	int ret = 0;

	spin_lock(&dai->lock);

	/* is playback/capture already running */
	if (ssp->state[DAI_DIR_PLAYBACK] == COMP_STATE_ACTIVE ||
	    ssp->state[DAI_DIR_CAPTURE] == COMP_STATE_ACTIVE) {
		dai_info(dai, "ssp_set_config(): playback/capture active. Ignore config");
		goto out;
	}

	dai_info(dai, "ssp_set_config(), config->format = 0x%4x",
		 config->format);

	/* reset SSP settings */
	/* sscr0 dynamic settings are DSS, EDSS, SCR, FRDC, ECS */
	/*
	 * FIXME: MOD, ACS, NCS are not set,
	 * no support for network mode for now
	 */
	sscr0 = SSCR0_PSP | SSCR0_RIM | SSCR0_TIM;

	/* sscr1 dynamic settings are SFRMDIR, SCLKDIR, SCFR */
	sscr1 = SSCR1_TTE | SSCR1_TTELP | SSCR1_TRAIL | SSCR1_RSRE | SSCR1_TSRE;

	/* sscr2 dynamic setting is LJDFD */
	sscr2 = SSCR2_SDFD | SSCR2_TURM1;

	/* sscr3 dynamic settings are TFT, RFT */
	sscr3 = 0;

	/* sspsp dynamic settings are SCMODE, SFRMP, DMYSTRT, SFRMWDTH */
	sspsp = 0;

	ssp->config = *config;
	ssp->params = config->ssp;

	/* sspsp2 no dynamic setting */
	sspsp2 = 0x0;

	/* ssioc dynamic setting is SFCR */
	ssioc = SSIOC_SCOE;

	/* ssto no dynamic setting */
	ssto = 0x0;

	/* sstsa dynamic setting is TTSA, default 2 slots */
	sstsa = SSTSA_SSTSA(config->ssp.tx_slots);

	/* ssrsa dynamic setting is RTSA, default 2 slots */
	ssrsa = SSRSA_SSRSA(config->ssp.rx_slots);

	switch (config->format & SOF_DAI_FMT_CLOCK_PROVIDER_MASK) {
	case SOF_DAI_FMT_CBP_CFP:
		sscr1 |= SSCR1_SCLKDIR | SSCR1_SFRMDIR;
		break;
	case SOF_DAI_FMT_CBC_CFC:
		sscr1 |= SSCR1_SCFR;
		cfs = true;
		break;
	case SOF_DAI_FMT_CBP_CFC:
		sscr1 |= SSCR1_SCLKDIR;
		/* FIXME: this mode has not been tested */

		cfs = true;
		break;
	case SOF_DAI_FMT_CBC_CFP:
		sscr1 |= SSCR1_SCFR | SSCR1_SFRMDIR;
		/* FIXME: this mode has not been tested */
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
		inverted_bclk = true; /* handled later with bclk idle */
		inverted_frame = true; /* handled later with format */
		break;
	case SOF_DAI_FMT_IB_NF:
		inverted_bclk = true; /* handled later with bclk idle */
		break;
	default:
		dai_err(dai, "ssp_set_config(): format & INV_MASK EINVAL");
		ret = -EINVAL;
		goto out;
	}

	/* supporting bclk idle state */
	if (ssp->params.clks_control &
		SOF_DAI_INTEL_SSP_CLKCTRL_BCLK_IDLE_HIGH) {
		/* bclk idle state high */
		sspsp |= SSPSP_SCMODE((inverted_bclk ^ 0x3) & 0x3);
	} else {
		/* bclk idle state low */
		sspsp |= SSPSP_SCMODE(inverted_bclk);
	}

	sscr0 |= SSCR0_MOD | SSCR0_ACS;

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

	if (!config->ssp.mclk_rate ||
	    config->ssp.mclk_rate > ssp_freq[MAX_SSP_FREQ_INDEX].freq) {
		dai_err(dai, "ssp_set_config(): invalid MCLK = %d Hz (valid < %d)",
			config->ssp.mclk_rate,
			ssp_freq[MAX_SSP_FREQ_INDEX].freq);
		ret = -EINVAL;
		goto out;
	}

	if (!config->ssp.bclk_rate ||
	    config->ssp.bclk_rate > config->ssp.mclk_rate) {
		dai_err(dai, "ssp_set_config(): BCLK %d Hz = 0 or > MCLK %d Hz",
			config->ssp.bclk_rate, config->ssp.mclk_rate);
		ret = -EINVAL;
		goto out;
	}

	/* MCLK config */
	ret = mn_set_mclk(config->ssp.mclk_id, config->ssp.mclk_rate);
	if (ret < 0) {
		dai_err(dai, "invalid mclk_rate = %d for mclk_id = %d",
			config->ssp.mclk_rate, config->ssp.mclk_id);
		goto out;
	}

	/* calc frame width based on BCLK and rate - must be divisable */
	if (config->ssp.bclk_rate % config->ssp.fsync_rate) {
		dai_err(dai, "ssp_set_config(): BCLK %d is not divisable by rate %d",
			config->ssp.bclk_rate, config->ssp.fsync_rate);
		ret = -EINVAL;
		goto out;
	}

	/* must be enough BCLKs for data */
	bdiv = config->ssp.bclk_rate / config->ssp.fsync_rate;
	if (bdiv < config->ssp.tdm_slot_width * config->ssp.tdm_slots) {
		dai_err(dai, "ssp_set_config(): not enough BCLKs need %d",
			config->ssp.tdm_slot_width *
			config->ssp.tdm_slots);
		ret = -EINVAL;
		goto out;
	}

	/* tdm_slot_width must be <= 38 for SSP */
	if (config->ssp.tdm_slot_width > 38) {
		dai_err(dai, "ssp_set_config(): tdm_slot_width %d > 38",
			config->ssp.tdm_slot_width);
		ret = -EINVAL;
		goto out;
	}

	bdiv_min = config->ssp.tdm_slots *
		   (config->ssp.tdm_per_slot_padding_flag ?
		    config->ssp.tdm_slot_width : config->ssp.sample_valid_bits);
	if (bdiv < bdiv_min) {
		dai_err(dai, "ssp_set_config(): bdiv(%d) < bdiv_min(%d)",
			bdiv, bdiv_min);
		ret = -EINVAL;
		goto out;
	}

	frame_end_padding = bdiv - bdiv_min;
	if (frame_end_padding > SSPSP2_FEP_MASK) {
		dai_err(dai, "ssp_set_config(): frame_end_padding too big: %u",
			frame_end_padding);
		ret = -EINVAL;
		goto out;
	}

	/* format */
	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_I2S:

		start_delay = true;

		sscr0 |= SSCR0_FRDC(config->ssp.tdm_slots);

		if (bdiv % 2) {
			dai_err(dai, "ssp_set_config(): bdiv %d is not divisible by 2",
				bdiv);
			ret = -EINVAL;
			goto out;
		}

		/* set asserted frame length to half frame length */
		frame_len = bdiv / 2;

		/*
		 * handle frame polarity, I2S default is falling/active low,
		 * non-inverted(inverted_frame=0) -- active low(SFRMP=0),
		 * inverted(inverted_frame=1) -- rising/active high(SFRMP=1),
		 * so, we should set SFRMP to inverted_frame.
		 */
		sspsp |= SSPSP_SFRMP(inverted_frame);

		/*
		 *  for I2S/LEFT_J, the padding has to happen at the end
		 * of each slot
		 */
		if (frame_end_padding % 2) {
			dai_err(dai, "ssp_set_config(): frame_end_padding %d is not divisible by 2",
				frame_end_padding);
			ret = -EINVAL;
			goto out;
		}

		slot_end_padding = frame_end_padding / 2;

		if (slot_end_padding > SOF_DAI_INTEL_SSP_SLOT_PADDING_MAX) {
			/* too big padding */
			dai_err(dai, "ssp_set_config(): slot_end_padding > %d",
				SOF_DAI_INTEL_SSP_SLOT_PADDING_MAX);
			ret = -EINVAL;
			goto out;
		}

		sspsp |= SSPSP_DMYSTOP(slot_end_padding);
		slot_end_padding >>= SSPSP_DMYSTOP_BITS;
		sspsp |= SSPSP_EDMYSTOP(slot_end_padding);

		break;

	case SOF_DAI_FMT_LEFT_J:

		/* default start_delay value is set to false */

		sscr0 |= SSCR0_FRDC(config->ssp.tdm_slots);

		/* LJDFD enable */
		sscr2 &= ~SSCR2_LJDFD;

		if (bdiv % 2) {
			dai_err(dai, "ssp_set_config(): bdiv %d is not divisible by 2",
				bdiv);
			ret = -EINVAL;
			goto out;
		}

		/* set asserted frame length to half frame length */
		frame_len = bdiv / 2;

		/*
		 * handle frame polarity, LEFT_J default is rising/active high,
		 * non-inverted(inverted_frame=0) -- active high(SFRMP=1),
		 * inverted(inverted_frame=1) -- falling/active low(SFRMP=0),
		 * so, we should set SFRMP to !inverted_frame.
		 */
		sspsp |= SSPSP_SFRMP(!inverted_frame);

		/*
		 *  for I2S/LEFT_J, the padding has to happen at the end
		 * of each slot
		 */
		if (frame_end_padding % 2) {
			dai_err(dai, "ssp_set_config(): frame_end_padding %d is not divisible by 2",
				frame_end_padding);
			ret = -EINVAL;
			goto out;
		}

		slot_end_padding = frame_end_padding / 2;

		if (slot_end_padding > 15) {
			/* can't handle padding over 15 bits */
			dai_err(dai, "ssp_set_config(): slot_end_padding %d > 15 bits",
				slot_end_padding);
			ret = -EINVAL;
			goto out;
		}

		sspsp |= SSPSP_DMYSTOP(slot_end_padding);
		slot_end_padding >>= SSPSP_DMYSTOP_BITS;
		sspsp |= SSPSP_EDMYSTOP(slot_end_padding);

		break;
	case SOF_DAI_FMT_DSP_A:

		start_delay = true;

		/* fallthrough */

	case SOF_DAI_FMT_DSP_B:

		/* default start_delay value is set to false */

		sscr0 |= SSCR0_MOD | SSCR0_FRDC(config->ssp.tdm_slots);

		/* set asserted frame length */
		frame_len = 1; /* default */

		if (cfs && ssp->params.frame_pulse_width > 0 &&
		    ssp->params.frame_pulse_width <=
		    SOF_DAI_INTEL_SSP_FRAME_PULSE_WIDTH_MAX) {
			frame_len = ssp->params.frame_pulse_width;
		}

		/* frame_pulse_width must less or equal 38 */
		if (ssp->params.frame_pulse_width >
			SOF_DAI_INTEL_SSP_FRAME_PULSE_WIDTH_MAX) {
			dai_err(dai, "ssp_set_config(): frame_pulse_width > %d",
				SOF_DAI_INTEL_SSP_FRAME_PULSE_WIDTH_MAX);
			ret = -EINVAL;
			goto out;
		}
		/*
		 * handle frame polarity, DSP_B default is rising/active high,
		 * non-inverted(inverted_frame=0) -- active high(SFRMP=1),
		 * inverted(inverted_frame=1) -- falling/active low(SFRMP=0),
		 * so, we should set SFRMP to !inverted_frame.
		 */
		sspsp |= SSPSP_SFRMP(!inverted_frame);

		active_tx_slots = popcount(config->ssp.tx_slots);
		active_rx_slots = popcount(config->ssp.rx_slots);

		/*
		 * handle TDM mode, TDM mode has padding at the end of
		 * each slot. The amount of padding is equal to result of
		 * subtracting slot width and valid bits per slot.
		 */
		if (ssp->params.tdm_per_slot_padding_flag) {
			frame_end_padding = bdiv - config->ssp.tdm_slots *
				config->ssp.tdm_slot_width;

			slot_end_padding = config->ssp.tdm_slot_width -
				config->ssp.sample_valid_bits;

			if (slot_end_padding >
				SOF_DAI_INTEL_SSP_SLOT_PADDING_MAX) {
				dai_err(dai, "ssp_set_config(): slot_end_padding > %d",
					SOF_DAI_INTEL_SSP_SLOT_PADDING_MAX);
				ret = -EINVAL;
				goto out;
			}

			sspsp |= SSPSP_DMYSTOP(slot_end_padding);
			slot_end_padding >>= SSPSP_DMYSTOP_BITS;
			sspsp |= SSPSP_EDMYSTOP(slot_end_padding);
		}

		sspsp2 |= (frame_end_padding & SSPSP2_FEP_MASK);

		break;
	default:
		dai_err(dai, "ssp_set_config(): invalid format 0x%04x",
			config->format);
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

	/* setting TFT and RFT */
	switch (config->ssp.sample_valid_bits) {
	case 16:
			/* use 2 bytes for each slot */
			sample_width = 2;
			break;
	case 24:
	case 32:
			/* use 4 bytes for each slot */
			sample_width = 4;
			break;
	default:
			dai_err(dai, "ssp_set_config(): sample_valid_bits %d",
				config->ssp.sample_valid_bits);
			ret = -EINVAL;
			goto out;
	}

	tft = MIN(SSP_FIFO_DEPTH - SSP_FIFO_WATERMARK,
		  sample_width * active_tx_slots);
	rft = MIN(SSP_FIFO_DEPTH - SSP_FIFO_WATERMARK,
		  sample_width * active_rx_slots);

	sscr3 |= SSCR3_TX(tft) | SSCR3_RX(rft);

	ssp_write(dai, SSCR0, sscr0);
	ssp_write(dai, SSCR1, sscr1);
	ssp_write(dai, SSCR2, sscr2);
	ssp_write(dai, SSCR3, sscr3);
	ssp_write(dai, SSPSP, sspsp);
	ssp_write(dai, SSPSP2, sspsp2);
	ssp_write(dai, SSIOC, ssioc);
	ssp_write(dai, SSTO, ssto);
	ssp_write(dai, SSTSA, sstsa);
	ssp_write(dai, SSRSA, ssrsa);

	dai_info(dai, "ssp_set_config(), sscr0 = 0x%08x, sscr1 = 0x%08x, ssto = 0x%08x, sspsp = 0x%0x",
		 sscr0, sscr1, ssto, sspsp);
	dai_info(dai, "ssp_set_config(), sscr2 = 0x%08x, sspsp2 = 0x%08x, sscr3 = 0x%08x, ssioc = 0x%08x",
		 sscr2, sspsp2, sscr3, ssioc);
	dai_info(dai, "ssp_set_config(), ssrsa = 0x%08x, sstsa = 0x%08x",
		 ssrsa, sstsa);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_PREPARE;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_PREPARE;

out:
	platform_shared_commit(ssp, sizeof(*ssp));

	spin_unlock(&dai->lock);

	return ret;
}

/*
 * Portion of the SSP configuration should be applied just before the
 * SSP dai is activated, for either power saving or params runtime
 * configurable flexibility.
 */
static int ssp_pre_start(struct dai *dai)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);
	struct sof_ipc_dai_config *config = &ssp->config;
	uint32_t sscr0;
	uint32_t mdiv;
	bool need_ecs = false;

	int ret = 0;

	dai_info(dai, "ssp_pre_start()");

	/* SSP active means bclk already configured. */
	if (ssp->state[SOF_IPC_STREAM_PLAYBACK] == COMP_STATE_ACTIVE ||
	    ssp->state[SOF_IPC_STREAM_CAPTURE] == COMP_STATE_ACTIVE)
		return 0;

	sscr0 = ssp_read(dai, SSCR0);

#if CONFIG_INTEL_MN
	/* BCLK config */
	ret = mn_set_bclk(config->dai_index, config->ssp.bclk_rate,
			  &mdiv, &need_ecs);
	if (ret < 0) {
		dai_err(dai, "invalid bclk_rate = %d for dai_index = %d",
			config->ssp.bclk_rate, config->dai_index);
		goto out;
	}
#else
	if (ssp_freq[SSP_DEFAULT_IDX].freq % config->ssp.bclk_rate != 0) {
		dai_err(dai, "invalid bclk_rate = %d for dai_index = %d",
			config->ssp.bclk_rate, config->dai_index);
		goto out;
	}

	mdiv = ssp_freq[SSP_DEFAULT_IDX].freq / config->ssp.bclk_rate;
#endif

	if (need_ecs)
		sscr0 |= SSCR0_ECS;

	/* clock divisor is SCR + 1 */
	mdiv -= 1;

	/* divisor must be within SCR range */
	if (mdiv > (SSCR0_SCR_MASK >> 8)) {
		dai_err(dai, "ssp_pre_start(): divisor %d is not within SCR range",
			mdiv);
		ret = -EINVAL;
		goto out;
	}

	/* set the SCR divisor */
	sscr0 &= ~SSCR0_SCR_MASK;
	sscr0 |= SSCR0_SCR(mdiv);

	ssp_write(dai, SSCR0, sscr0);

	dai_info(dai, "ssp_set_config(), sscr0 = 0x%08x", sscr0);
out:
	platform_shared_commit(ssp, sizeof(*ssp));

	return ret;
}

/*
 * For power saving, we should do kinds of power release when the SSP
 * dai is changed to inactive, though the runtime param configuration
 * don't have to be reset.
 */
static void ssp_post_stop(struct dai *dai)
{
#if CONFIG_INTEL_MN
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	/* release bclk if SSP is inactive */
	if (ssp->state[SOF_IPC_STREAM_PLAYBACK] != COMP_STATE_ACTIVE &&
	    ssp->state[SOF_IPC_STREAM_CAPTURE] != COMP_STATE_ACTIVE)
		mn_release_bclk(dai->index);
#endif
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

	spin_lock(&dai->lock);

	/* request mclk/bclk */
	ssp_pre_start(dai);

	/* enable port */
	ssp_update_bits(dai, SSCR0, SSCR0_SSE, SSCR0_SSE);
	ssp->state[direction] = COMP_STATE_ACTIVE;

	dai_info(dai, "ssp_start()");

	if (ssp->params.bclk_delay) {
		/* drive BCLK early for guaranteed time,
		 * before first FSYNC, it is required by some codecs
		 */
		wait_delay(clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK,
					     ssp->params.bclk_delay));
	}

	/* enable DMA */
	if (direction == DAI_DIR_PLAYBACK) {
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, SSCR1_TSRE);
		ssp_update_bits(dai, SSTSA, SSTSA_TXEN, SSTSA_TXEN);
	} else {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, SSCR1_RSRE);
		ssp_update_bits(dai, SSRSA, SSRSA_RXEN, SSRSA_RXEN);
	}

	/* wait to get valid fifo status */
	wait_delay(PLATFORM_SSP_DELAY);

	spin_unlock(&dai->lock);
}

/* stop the SSP for either playback or capture */
static void ssp_stop(struct dai *dai, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	spin_lock(&dai->lock);

	/* wait to get valid fifo status */
	wait_delay(PLATFORM_SSP_DELAY);

	/* stop Rx if neeed */
	if (direction == DAI_DIR_CAPTURE &&
	    ssp->state[SOF_IPC_STREAM_CAPTURE] != COMP_STATE_PREPARE) {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, 0);
		ssp_update_bits(dai, SSRSA, SSRSA_RXEN, 0);
		ssp_empty_rx_fifo(dai);
		ssp->state[SOF_IPC_STREAM_CAPTURE] = COMP_STATE_PREPARE;
		dai_info(dai, "ssp_stop(), RX stop");
	}

	/* stop Tx if needed */
	if (direction == DAI_DIR_PLAYBACK &&
	    ssp->state[SOF_IPC_STREAM_PLAYBACK] != COMP_STATE_PREPARE) {
		ssp_empty_tx_fifo(dai);
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, 0);
		ssp_update_bits(dai, SSTSA, SSTSA_TXEN, 0);
		ssp->state[SOF_IPC_STREAM_PLAYBACK] = COMP_STATE_PREPARE;
		dai_info(dai, "ssp_stop(), TX stop");
	}

	/* disable SSP port if no users */
	if (ssp->state[SOF_IPC_STREAM_CAPTURE] == COMP_STATE_PREPARE &&
	    ssp->state[SOF_IPC_STREAM_PLAYBACK] == COMP_STATE_PREPARE) {
		ssp_update_bits(dai, SSCR0, SSCR0_SSE, 0);
		dai_info(dai, "ssp_stop(), SSP port disabled");
	}

	ssp_post_stop(dai);

	spin_unlock(&dai->lock);
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

	dai_info(dai, "ssp_trigger() cmd %d", cmd);

	switch (cmd) {
	case COMP_TRIGGER_START:
		if (ssp->state[direction] == COMP_STATE_PREPARE ||
		    ssp->state[direction] == COMP_STATE_PAUSED)
			ssp_start(dai, direction);
		break;
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
	case COMP_TRIGGER_RESUME:
		ssp_context_restore(dai);
		break;
	case COMP_TRIGGER_SUSPEND:
		ssp_context_store(dai);
		break;
	default:
		break;
	}

	platform_shared_commit(ssp, sizeof(*ssp));

	return 0;
}

static int ssp_probe(struct dai *dai)
{
	struct ssp_pdata *ssp;

	if (dai_get_drvdata(dai))
		return -EEXIST; /* already created */

	/* allocate private data */
	ssp = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*ssp));
	if (!ssp) {
		dai_err(dai, "ssp_probe(): alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, ssp);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_READY;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_READY;

#if CONFIG_INTEL_MN
	/* Reset M/N, power-gating functions need it */
	mn_reset_bclk_divider(dai->index);
#endif

	/* Enable SSP power */
	pm_runtime_get_sync(SSP_POW, dai->index);

	/* Disable dynamic clock gating before touching any register */
	pm_runtime_get_sync(SSP_CLK, dai->index);

	ssp_empty_rx_fifo(dai);

	platform_shared_commit(ssp, sizeof(*ssp));

	return 0;
}

static int ssp_remove(struct dai *dai)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	pm_runtime_put_sync(SSP_CLK, dai->index);

	mn_release_mclk(ssp->config.ssp.mclk_id);
#if CONFIG_INTEL_MN
	mn_release_bclk(dai->index);
#endif

	/* Disable SSP power */
	pm_runtime_put_sync(SSP_POW, dai->index);

	rfree(dai_get_drvdata(dai));
	dai_set_drvdata(dai, NULL);

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
	.uid = SOF_UUID(ssp_uuid),
	.tctx = &ssp_tr,
	.dma_caps = DMA_CAP_GP_LP | DMA_CAP_GP_HP,
	.dma_dev = DMA_DEV_SSP,
	.ops = {
		.trigger		= ssp_trigger,
		.set_config		= ssp_set_config,
		.pm_context_store	= ssp_context_store,
		.pm_context_restore	= ssp_context_restore,
		.get_hw_params		= ssp_get_hw_params,
		.get_handshake		= ssp_get_handshake,
		.get_fifo		= ssp_get_fifo,
		.probe			= ssp_probe,
		.remove			= ssp_remove,
	},
	.ts_ops = {
		.ts_config		= timestamp_ssp_config,
		.ts_start		= timestamp_ssp_start,
		.ts_get			= timestamp_ssp_get,
		.ts_stop		= timestamp_ssp_stop,
	},
};
