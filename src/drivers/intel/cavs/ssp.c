/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@linux.intel.com>
 */

#include <errno.h>
#include <stdbool.h>
#include <sof/stream.h>
#include <sof/ssp.h>
#include <sof/alloc.h>
#include <sof/interrupt.h>
#include <sof/math/numbers.h>
#include <config.h>

/* tracing */
#define trace_ssp(__e)	trace_event(TRACE_CLASS_SSP, __e)
#define trace_ssp_error(__e)	trace_error(TRACE_CLASS_SSP, __e)
#define tracev_ssp(__e)	tracev_event(TRACE_CLASS_SSP, __e)

#define F_19200_kHz 19200000
#define F_24000_kHz 24000000
#define F_24576_kHz 24576000

/* FIXME: move this to a helper and optimize */
static int hweight_32(uint32_t mask)
{
	int i;
	int count = 0;

	for (i = 0; i < 32; i++) {
		count += mask & 1;
		mask >>= 1;
	}
	return count;
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
static inline int ssp_set_config(struct dai *dai,
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
	uint32_t mdiv;
	uint32_t bdiv;
	uint32_t mdivc;
	uint32_t mdivr;
	uint32_t mdivr_val;
	uint32_t i2s_m;
	uint32_t i2s_n;
	uint32_t data_size;
	uint32_t start_delay;
	uint32_t frame_end_padding;
	uint32_t slot_end_padding;
	uint32_t frame_len = 0;
	uint32_t bdiv_min;
	uint32_t tft;
	uint32_t rft;
	uint32_t active_tx_slots = 2;
	uint32_t active_rx_slots = 2;
	uint32_t sample_width = 2;

	bool inverted_frame = false;
	int ret = 0;

	spin_lock(&ssp->lock);

	/* is playback/capture already running */
	if (ssp->state[DAI_DIR_PLAYBACK] == COMP_STATE_ACTIVE ||
		ssp->state[DAI_DIR_CAPTURE] == COMP_STATE_ACTIVE) {
		trace_ssp_error("ec1");
		ret = -EINVAL;
		goto out;
	}

	trace_ssp("cos");
	trace_value(config->format);

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

	/* i2s_m M divider setting, default 1 */
	i2s_m = 0x1;

	/* i2s_n N divider setting, default 1 */
	i2s_n = 0x1;

	/* ssto no dynamic setting */
	ssto = 0x0;

	/* sstsa dynamic setting is TTSA, default 2 slots */
	sstsa = config->ssp.tx_slots;

	/* ssrsa dynamic setting is RTSA, default 2 slots */
	ssrsa = config->ssp.rx_slots;

	/* clock masters */
	sscr1 &= ~SSCR1_SFRMDIR;

	trace_value(config->format);
	switch (config->format & SOF_DAI_FMT_MASTER_MASK) {
	case SOF_DAI_FMT_CBM_CFM:
		sscr0 |= SSCR0_ECS; /* external clock used */
		sscr1 |= SSCR1_SCLKDIR;
		/*
		 * FIXME: does SSRC1.SCFR need to be set
		 * when codec is master ?
		 */
		break;
	case SOF_DAI_FMT_CBS_CFS:
		sscr1 |= SSCR1_SCFR;
		ssioc |= SSIOC_SFCR;
		break;
	case SOF_DAI_FMT_CBM_CFS:
		sscr0 |= SSCR0_ECS; /* external clock used */
		sscr1 |= SSCR1_SCLKDIR;
		/*
		 * FIXME: does SSRC1.SCFR need to be set
		 * when codec is master ?
		 */
		/* FIXME: this mode has not been tested */
		break;
	case SOF_DAI_FMT_CBS_CFM:
		sscr1 |= SSCR1_SCFR;
		/* FIXME: this mode has not been tested */
		break;
	default:
		trace_ssp_error("ec2");
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
		trace_ssp_error("ec3");
		ret = -EINVAL;
		goto out;
	}

	sscr0 |= SSCR0_MOD | SSCR0_ACS;

	mdivc = 0x1;
#ifdef CONFIG_CANNONLAKE
	if (!config->ssp.mclk_rate || config->ssp.mclk_rate > F_24000_kHz) {
		trace_ssp_error("eci");
		ret = -EINVAL;
		goto out;
	}
	if (!config->ssp.bclk_rate ||
	    config->ssp.bclk_rate > config->ssp.mclk_rate) {
		trace_ssp_error("ecj");
		ret = -EINVAL;
		goto out;
	}

	if (F_24000_kHz % config->ssp.mclk_rate == 0) {
		mdivr_val = F_24000_kHz / config->ssp.mclk_rate;
	} else {
		trace_ssp_error("eck");
		ret = -EINVAL;
		goto out;
	}

	if (F_24000_kHz % config->ssp.bclk_rate == 0) {
		mdiv = F_24000_kHz / config->ssp.bclk_rate;
	} else {
		trace_ssp_error("ecl");
		ret = -EINVAL;
		goto out;
	}
#else
	if (!config->ssp.mclk_rate || config->ssp.mclk_rate > F_24576_kHz) {
		trace_ssp_error("eci");
		ret = -EINVAL;
		goto out;
	}
	if (!config->ssp.bclk_rate ||
	    config->ssp.bclk_rate > config->ssp.mclk_rate) {
		trace_ssp_error("ecj");
		ret = -EINVAL;
		goto out;
	}
	if (F_24576_kHz % config->ssp.mclk_rate == 0) {
		/* select Audio Cardinal clock for MCLK */
		mdivc |= MCDSS(1);
		mdivr_val = F_24576_kHz / config->ssp.mclk_rate;
	} else if (config->ssp.mclk_rate <= F_19200_kHz &&
		   F_19200_kHz % config->ssp.mclk_rate == 0) {
		mdivr_val = F_19200_kHz / config->ssp.mclk_rate;
	} else {
		trace_ssp_error("eck");
		ret = -EINVAL;
		goto out;
	}

	if (F_24576_kHz % config->ssp.bclk_rate == 0) {
		/* select Audio Cardinal clock for M/N dividers */
		mdivc |= MNDSS(1);
		mdiv = F_24576_kHz / config->ssp.bclk_rate;
		/* select M/N output for bclk */
		sscr0 |= SSCR0_ECS;
	} else if (F_19200_kHz % config->ssp.bclk_rate == 0) {
		mdiv = F_19200_kHz / config->ssp.bclk_rate;
	} else {
		trace_ssp_error("ecl");
		ret = -EINVAL;
		goto out;
	}
#endif

	switch (mdivr_val) {
	case 1:
		mdivr = 0x00000fff; /* bypass divider for MCLK */
		break;
	case 2:
		mdivr = 0x0; /* 1/2 */
		break;
	case 4:
		mdivr = 0x2; /* 1/4 */
		break;
	case 8:
		mdivr = 0x6; /* 1/8 */
		break;
	default:
		trace_ssp_error("ecm");
		ret = -EINVAL;
		goto out;
	}

	if (config->ssp.mclk_id > 1) {
		trace_ssp_error("ecn");
		ret = -EINVAL;
		goto out;
	}

	/* divisor must be within SCR range */
	mdiv -= 1;
	if (mdiv > (SSCR0_SCR_MASK >> 8)) {
		trace_ssp_error("ec6");
		ret = -EINVAL;
		goto out;
	}

	/* set the SCR divisor */
	sscr0 |= SSCR0_SCR(mdiv);

	/* calc frame width based on BCLK and rate - must be divisable */
	if (config->ssp.bclk_rate % config->ssp.fsync_rate) {
		trace_ssp_error("ec7");
		ret = -EINVAL;
		goto out;
	}

	/* must be enough BCLKs for data */
	bdiv = config->ssp.bclk_rate / config->ssp.fsync_rate;
	if (bdiv < config->ssp.tdm_slot_width * config->ssp.tdm_slots) {
		trace_ssp_error("ec8");
		ret = -EINVAL;
		goto out;
	}

	/* tdm_slot_width must be <= 38 for SSP */
	if (config->ssp.tdm_slot_width > 38) {
		trace_ssp_error("ec9");
		ret = -EINVAL;
		goto out;
	}

	bdiv_min = config->ssp.tdm_slots * config->ssp.sample_valid_bits;
	if (bdiv < bdiv_min) {
		trace_ssp_error("ecc");
		ret = -EINVAL;
		goto out;
	}

	frame_end_padding = bdiv - bdiv_min;
	if (frame_end_padding > SSPSP2_FEP_MASK) {
		trace_ssp_error("ecd");
		ret = -EINVAL;
		goto out;
	}

	/* format */
	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_I2S:

		start_delay = 1;

		sscr0 |= SSCR0_FRDC(config->ssp.tdm_slots);

		if (bdiv % 2) {
			trace_ssp_error("eca");
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
		sspsp |= SSPSP_FSRT;

		/*
		 *  for I2S/LEFT_J, the padding has to happen at the end
		 * of each slot
		 */
		if (frame_end_padding % 2) {
			trace_ssp_error("ece");
			ret = -EINVAL;
			goto out;
		}

		slot_end_padding = frame_end_padding / 2;

		if (slot_end_padding > 15) {
			/* can't handle padding over 15 bits */
			trace_ssp_error("ecf");
			ret = -EINVAL;
			goto out;
		}

		sspsp |= SSPSP_DMYSTOP(slot_end_padding & SSPSP_DMYSTOP_MASK);
		slot_end_padding >>= SSPSP_DMYSTOP_BITS;
		sspsp |= SSPSP_EDMYSTOP(slot_end_padding & SSPSP_EDMYSTOP_MASK);

		break;

	case SOF_DAI_FMT_LEFT_J:

		start_delay = 0;

		sscr0 |= SSCR0_FRDC(config->ssp.tdm_slots);

		/* LJDFD enable */
		sscr2 &= ~SSCR2_LJDFD;

		if (bdiv % 2) {
			trace_ssp_error("ecb");
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
			trace_ssp_error("ecg");
			ret = -EINVAL;
			goto out;
		}

		slot_end_padding = frame_end_padding / 2;

		if (slot_end_padding > 15) {
			/* can't handle padding over 15 bits */
			trace_ssp_error("ech");
			ret = -EINVAL;
			goto out;
		}

		sspsp |= SSPSP_DMYSTOP(slot_end_padding & SSPSP_DMYSTOP_MASK);
		slot_end_padding >>= SSPSP_DMYSTOP_BITS;
		sspsp |= SSPSP_EDMYSTOP(slot_end_padding & SSPSP_EDMYSTOP_MASK);

		break;
	case SOF_DAI_FMT_DSP_A:

		start_delay = 0;

		sscr0 |= SSCR0_MOD | SSCR0_FRDC(config->ssp.tdm_slots);

		/* set asserted frame length */
		frame_len = 1;

		/*
		 * handle frame polarity, DSP_A default is rising/active high,
		 * non-inverted(inverted_frame=0) -- active high(SFRMP=1),
		 * inverted(inverted_frame=1) -- falling/active low(SFRMP=0),
		 * so, we should set SFRMP to !inverted_frame.
		 */
		sspsp |= SSPSP_SFRMP(!inverted_frame);
		sspsp |= SSPSP_FSRT;

		active_tx_slots = hweight_32(config->ssp.tx_slots);
		active_rx_slots = hweight_32(config->ssp.rx_slots);

		sspsp2 |= (frame_end_padding & SSPSP2_FEP_MASK);

		break;
	case SOF_DAI_FMT_DSP_B:

		start_delay = 0;

		sscr0 |= SSCR0_MOD | SSCR0_FRDC(config->ssp.tdm_slots);

		/* set asserted frame length */
		frame_len = 1;

		/*
		 * handle frame polarity, DSP_B default is rising/active high,
		 * non-inverted(inverted_frame=0) -- active high(SFRMP=1),
		 * inverted(inverted_frame=1) -- falling/active low(SFRMP=0),
		 * so, we should set SFRMP to !inverted_frame.
		 */
		sspsp |= SSPSP_SFRMP(!inverted_frame);

		active_tx_slots = hweight_32(config->ssp.tx_slots);
		active_rx_slots = hweight_32(config->ssp.rx_slots);

		sspsp2 |= (frame_end_padding & SSPSP2_FEP_MASK);

		break;
	default:
		trace_ssp_error("eca");
		ret = -EINVAL;
		goto out;
	}

	sspsp |= SSPSP_STRTDLY(start_delay);
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
			trace_ssp_error("ecd");
			ret = -EINVAL;
			goto out;
	}

	tft = MIN(8, sample_width * active_tx_slots);
	rft = MIN(8, sample_width * active_rx_slots);

	sscr3 |= SSCR3_TX(tft) | SSCR3_RX(rft);

	trace_ssp("coe");
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

	trace_value(sscr0);
	trace_value(sscr1);
	trace_value(ssto);
	trace_value(sspsp);
	trace_value(sstsa);
	trace_value(ssrsa);
	trace_value(sscr2);
	trace_value(sspsp2);
	trace_value(sscr3);
	trace_value(ssioc);

	/* TODO: move this into M/N driver */
	mn_reg_write(0x0, mdivc);
	mn_reg_write(0x80 + config->ssp.mclk_id * 0x4, mdivr);
	mn_reg_write(0x100 + config->id * 0x8 + 0x0, i2s_m);
	mn_reg_write(0x100 + config->id * 0x8 + 0x4, i2s_n);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_PREPARE;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_PREPARE;

out:
	spin_unlock(&ssp->lock);

	return ret;
}

/* Digital Audio interface formatting */
static inline int ssp_set_loopback_mode(struct dai *dai, uint32_t lbm)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	trace_ssp("loo");
	spin_lock(&ssp->lock);

	ssp_update_bits(dai, SSCR1, SSCR1_LBM, lbm ? SSCR1_LBM : 0);

	spin_unlock(&ssp->lock);

	return 0;
}

/* start the SSP for either playback or capture */
static void ssp_start(struct dai *dai, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	spin_lock(&ssp->lock);

	/* enable port */
	ssp_update_bits(dai, SSCR0, SSCR0_SSE, SSCR0_SSE);
	ssp->state[direction] = COMP_STATE_ACTIVE;

	trace_ssp("sta");

	/* enable DMA */
	if (direction == DAI_DIR_PLAYBACK) {
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, SSCR1_TSRE);
		ssp_update_bits(dai, SSTSA, 0x1 << 8, 0x1 << 8);
	} else {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, SSCR1_RSRE);
		ssp_update_bits(dai, SSRSA, 0x1 << 8, 0x1 << 8);
	}

	spin_unlock(&ssp->lock);
}

/* stop the SSP for either playback or capture */
static void ssp_stop(struct dai *dai, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	spin_lock(&ssp->lock);

	/* stop Rx if neeed */
	if (direction == DAI_DIR_CAPTURE &&
	    ssp->state[SOF_IPC_STREAM_CAPTURE] == COMP_STATE_ACTIVE) {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, 0);
		ssp_update_bits(dai, SSRSA, 0x1 << 8, 0x0 << 8);
		ssp->state[SOF_IPC_STREAM_CAPTURE] = COMP_STATE_PAUSED;
		trace_ssp("Ss0");
	}

	/* stop Tx if needed */
	if (direction == DAI_DIR_PLAYBACK &&
	    ssp->state[SOF_IPC_STREAM_PLAYBACK] == COMP_STATE_ACTIVE) {
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, 0);
		ssp_update_bits(dai, SSTSA, 0x1 << 8, 0x0 << 8);
		ssp->state[SOF_IPC_STREAM_PLAYBACK] = COMP_STATE_PAUSED;
		trace_ssp("Ss1");
	}

	/* disable SSP port if no users */
	if (ssp->state[SOF_IPC_STREAM_CAPTURE] != COMP_STATE_ACTIVE &&
		ssp->state[SOF_IPC_STREAM_PLAYBACK] != COMP_STATE_ACTIVE) {
		ssp_update_bits(dai, SSCR0, SSCR0_SSE, 0);
		ssp->state[SOF_IPC_STREAM_CAPTURE] = COMP_STATE_PREPARE;
		ssp->state[SOF_IPC_STREAM_PLAYBACK] = COMP_STATE_PREPARE;
		trace_ssp("Ss2");
	}

	spin_unlock(&ssp->lock);
}

static int ssp_trigger(struct dai *dai, int cmd, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	trace_ssp("tri");

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
	case COMP_TRIGGER_PAUSE:
		ssp_stop(dai, direction);
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

	return 0;
}

/* clear IRQ sources atm */
static void ssp_irq_handler(void *data)
{
	struct dai *dai = data;

	trace_ssp("irq");
	trace_value(ssp_read(dai, SSSR));

	/* clear IRQ */
	ssp_write(dai, SSSR, ssp_read(dai, SSSR));
	drivers_interrupt_clear(ssp_irq(dai), 1);
}

static int ssp_probe(struct dai *dai)
{
	struct ssp_pdata *ssp;

	/* allocate private data */
	ssp = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*ssp));
	dai_set_drvdata(dai, ssp);

	spinlock_init(&ssp->lock);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_READY;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_READY;

	/* register our IRQ handler */
	interrupt_register(ssp_irq(dai), ssp_irq_handler, dai);
	drivers_interrupt_unmask(ssp_irq(dai), 1);
	interrupt_enable(ssp_irq(dai));

	return 0;
}

const struct dai_ops ssp_ops = {
	.trigger		= ssp_trigger,
	.set_config		= ssp_set_config,
	.pm_context_store	= ssp_context_store,
	.pm_context_restore	= ssp_context_restore,
	.probe			= ssp_probe,
	.set_loopback_mode	= ssp_set_loopback_mode,
};
