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
#include <sof/pm_runtime.h>
#include <sof/math/numbers.h>
#include <config.h>

/* tracing */
#define trace_ssp(__e, ...)	trace_event(TRACE_CLASS_SSP, __e, ##__VA_ARGS__)
#define trace_ssp_error(__e, ...)	trace_error(TRACE_CLASS_SSP, __e, ##__VA_ARGS__)
#define tracev_ssp(__e, ...)	tracev_event(TRACE_CLASS_SSP, __e, ##__VA_ARGS__)

#define F_19200_kHz 19200000
#define F_24000_kHz 24000000
#define F_24576_kHz 24576000
#define F_38400_kHz 38400000

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

/* empty SSP transmit FIFO */
static void ssp_empty_tx_fifo(struct dai *dai)
{
	uint32_t sssr;

	spin_lock(&dai->lock);

	sssr = ssp_read(dai, SSSR);

	/* clear interrupt */
	if (sssr & SSSR_TUR)
		ssp_write(dai, SSSR, sssr);

	spin_unlock(&dai->lock);
}

/* empty SSP receive FIFO */
static void ssp_empty_rx_fifo(struct dai *dai)
{
	uint32_t sssr;
	uint32_t entries;
	uint32_t i;

	spin_lock(&dai->lock);

	sssr = ssp_read(dai, SSSR);

	/* clear interrupt */
	if (sssr & SSSR_ROR)
		ssp_write(dai, SSSR, sssr);

	/* empty fifo */
	if (sssr & SSSR_RNE) {
		entries = (ssp_read(dai, SSCR3) & SSCR3_RFL_MASK) >> 8;
		for (i = 0; i < entries + 1; i++)
			ssp_read(dai, SSDR);
	}

	spin_unlock(&dai->lock);
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
		trace_ssp_error("ssp_set_config() error: "
				"playback/capture already running");
		ret = -EINVAL;
		goto out;
	}

	trace_ssp("ssp_set_config(), config->format = 0x%4x", config->format);

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

	switch (config->format & SOF_DAI_FMT_MASTER_MASK) {
	case SOF_DAI_FMT_CBM_CFM:
		sscr1 |= SSCR1_SCLKDIR | SSCR1_SFRMDIR;
		break;
	case SOF_DAI_FMT_CBS_CFS:
		sscr1 |= SSCR1_SCFR;
		cfs = true;
		break;
	case SOF_DAI_FMT_CBM_CFS:
		sscr1 |= SSCR1_SCLKDIR;
		/* FIXME: this mode has not been tested */

		cfs = true;
		break;
	case SOF_DAI_FMT_CBS_CFM:
		sscr1 |= SSCR1_SCFR | SSCR1_SFRMDIR;
		/* FIXME: this mode has not been tested */
		break;
	default:
		trace_ssp_error("ssp_set_config() error: "
				"format & MASTER_MASK EINVAL");
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
		trace_ssp_error("ssp_set_config() error: "
				"format & INV_MASK EINVAL");
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

	mdivc = mn_reg_read(0x0);
	mdivc |= 0x1;

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

	/* Enable/disable the fix for PSP slave mode TXD wait for frame
	 * de-assertion before starting the second channel
	 */
	sscr2 |= (ssp->params.quirks & SOF_DAI_INTEL_SSP_QUIRK_PSPSTWFDFD) ?
		SSCR2_PSPSTWFDFD : 0;

	/* Enable/disable the fix for PSP master mode FSRT with dummy stop &
	 * frame end padding capability
	 */
	sscr2 |= (ssp->params.quirks & SOF_DAI_INTEL_SSP_QUIRK_PSPSRWFDFD) ?
		SSCR2_PSPSRWFDFD : 0;

#if defined(CONFIG_ICELAKE)
	if (!config->ssp.mclk_rate || config->ssp.mclk_rate > F_38400_kHz) {
		trace_ssp_error("ssp_set_config() error: "
				"invalid MCLK = %d Hz (valid < 38400kHz)",
				config->ssp.mclk_rate);
		ret = -EINVAL;
		goto out;
	}
	if (!config->ssp.bclk_rate ||
	    config->ssp.bclk_rate > config->ssp.mclk_rate) {
		trace_ssp_error("ssp_set_config() error: "
				"BCLK %d Hz = 0 or > MCLK %d Hz",
				config->ssp.bclk_rate, config->ssp.mclk_rate);
		ret = -EINVAL;
		goto out;
	}

	if (F_38400_kHz % config->ssp.mclk_rate == 0) {
		mdivr_val = F_38400_kHz / config->ssp.mclk_rate;
	} else {
		trace_ssp_error("ssp_set_config() error: 38.4MHz / %d Hz MCLK not divisable",
				config->ssp.mclk_rate);
		ret = -EINVAL;
		goto out;
	}

	if (F_38400_kHz % config->ssp.bclk_rate == 0) {
		mdiv = F_38400_kHz / config->ssp.bclk_rate;
	} else {
		trace_ssp_error("ssp_set_config() error: 38.4MHz / %d Hz BCLK not divisable",
				config->ssp.bclk_rate);
		ret = -EINVAL;
		goto out;
	}
#elif defined(CONFIG_CANNONLAKE)
	if (!config->ssp.mclk_rate || config->ssp.mclk_rate > F_24000_kHz) {
		trace_ssp_error("ssp_set_config() error: "
				"invalid MCLK = %d Hz (valid < 24000kHz)",
				config->ssp.mclk_rate);
		ret = -EINVAL;
		goto out;
	}
	if (!config->ssp.bclk_rate ||
	    config->ssp.bclk_rate > config->ssp.mclk_rate) {
		trace_ssp_error("ssp_set_config() error: "
				"BCLK %d Hz = 0 or > MCLK %d Hz",
				config->ssp.bclk_rate, config->ssp.mclk_rate);
		ret = -EINVAL;
		goto out;
	}

	if (F_24000_kHz % config->ssp.mclk_rate == 0) {
		mdivr_val = F_24000_kHz / config->ssp.mclk_rate;
	} else {
		trace_ssp_error("ssp_set_config() error: 24.0MHz / %d Hz MCLK not divisable",
				config->ssp.mclk_rate);
		ret = -EINVAL;
		goto out;
	}

	if (F_24000_kHz % config->ssp.bclk_rate == 0) {
		mdiv = F_24000_kHz / config->ssp.bclk_rate;
	} else {
		trace_ssp_error("ssp_set_config() error: 24.0MHz / %d Hz BCLK not divisable",
				config->ssp.bclk_rate);
		ret = -EINVAL;
		goto out;
	}
#else
	if (!config->ssp.mclk_rate || config->ssp.mclk_rate > F_24576_kHz) {
		trace_ssp_error("ssp_set_config() error: "
				"invalid MCLK = %d Hz (valid < 24567kHz)",
				config->ssp.mclk_rate);
		ret = -EINVAL;
		goto out;
	}
	if (!config->ssp.bclk_rate ||
	    config->ssp.bclk_rate > config->ssp.mclk_rate) {
		trace_ssp_error("ssp_set_config() error: "
				"BCLK %d Hz = 0 or > MCLK %d Hz",
				config->ssp.bclk_rate, config->ssp.mclk_rate);
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
		trace_ssp_error("ssp_set_config() error: MCLK %d",
				config->ssp.mclk_rate);
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
		trace_ssp_error("ssp_set_config() error: BCLK %d",
				config->ssp.bclk_rate);
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
		trace_ssp_error("ssp_set_config() error: invalid mdivr_val %d",
				mdivr_val);
		ret = -EINVAL;
		goto out;
	}

	if (config->ssp.mclk_id > 1) {
		trace_ssp_error("ssp_set_config() error: mclk ID (%d) > 1",
				config->ssp.mclk_id);
		ret = -EINVAL;
		goto out;
	}

	/* divisor must be within SCR range */
	mdiv -= 1;
	if (mdiv > (SSCR0_SCR_MASK >> 8)) {
		trace_ssp_error("ssp_set_config() error: "
				"divisor %d is not within SCR range", mdiv);
		ret = -EINVAL;
		goto out;
	}

	/* set the SCR divisor */
	sscr0 |= SSCR0_SCR(mdiv);

	/* calc frame width based on BCLK and rate - must be divisable */
	if (config->ssp.bclk_rate % config->ssp.fsync_rate) {
		trace_ssp_error("ssp_set_config() error: "
				"BCLK %d is not divisable by rate %d",
				config->ssp.bclk_rate, config->ssp.fsync_rate);
		ret = -EINVAL;
		goto out;
	}

	/* must be enough BCLKs for data */
	bdiv = config->ssp.bclk_rate / config->ssp.fsync_rate;
	if (bdiv < config->ssp.tdm_slot_width * config->ssp.tdm_slots) {
		trace_ssp_error("ssp_set_config() error: not enough BCLKs need %d",
				config->ssp.tdm_slot_width *
				config->ssp.tdm_slots);
		ret = -EINVAL;
		goto out;
	}

	/* tdm_slot_width must be <= 38 for SSP */
	if (config->ssp.tdm_slot_width > 38) {
		trace_ssp_error("ssp_set_config() error: tdm_slot_width %d > 38",
				config->ssp.tdm_slot_width);
		ret = -EINVAL;
		goto out;
	}

	bdiv_min = config->ssp.tdm_slots * (config->ssp.tdm_per_slot_padding_flag ?
		   config->ssp.tdm_slot_width : config->ssp.sample_valid_bits);
	if (bdiv < bdiv_min) {
		trace_ssp_error("ssp_set_config() error: bdiv(%d) < bdiv_min(%d)",
				bdiv < bdiv_min);
		ret = -EINVAL;
		goto out;
	}

	frame_end_padding = bdiv - bdiv_min;
	if (frame_end_padding > SSPSP2_FEP_MASK) {
		trace_ssp_error("ssp_set_config() error: frame_end_padding "
				"too big: %u", frame_end_padding);
		ret = -EINVAL;
		goto out;
	}

	/* format */
	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_I2S:

		start_delay = true;

		sscr0 |= SSCR0_FRDC(config->ssp.tdm_slots);

		if (bdiv % 2) {
			trace_ssp_error("ssp_set_config() error: "
					"bdiv %d is not divisible by 2", bdiv);
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
			trace_ssp_error("ssp_set_config() error: "
					"frame_end_padding %d "
					"is not divisible by 2",
					frame_end_padding);
			ret = -EINVAL;
			goto out;
		}

		slot_end_padding = frame_end_padding / 2;

		if (slot_end_padding > SOF_DAI_INTEL_SSP_SLOT_PADDING_MAX) {
			/* too big padding */
			trace_ssp_error("ssp_set_config() error: "
					"slot_end_padding > %d",
					SOF_DAI_INTEL_SSP_SLOT_PADDING_MAX);
			ret = -EINVAL;
			goto out;
		}

		sspsp |= SSPSP_DMYSTOP(slot_end_padding & SSPSP_DMYSTOP_MASK);
		slot_end_padding >>= SSPSP_DMYSTOP_BITS;
		sspsp |= SSPSP_EDMYSTOP(slot_end_padding & SSPSP_EDMYSTOP_MASK);

		break;

	case SOF_DAI_FMT_LEFT_J:

		/* default start_delay value is set to false */

		sscr0 |= SSCR0_FRDC(config->ssp.tdm_slots);

		/* LJDFD enable */
		sscr2 &= ~SSCR2_LJDFD;

		if (bdiv % 2) {
			trace_ssp_error("ssp_set_config() error: "
					"bdiv %d is not divisible by 2", bdiv);
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
			trace_ssp_error("ssp_set_config() error: "
					"frame_end_padding %d "
					"is not divisible by 2",
					frame_end_padding);
			ret = -EINVAL;
			goto out;
		}

		slot_end_padding = frame_end_padding / 2;

		if (slot_end_padding > 15) {
			/* can't handle padding over 15 bits */
			trace_ssp_error("ssp_set_config() error: "
					"slot_end_padding %d > 15 bits",
					slot_end_padding);
			ret = -EINVAL;
			goto out;
		}

		sspsp |= SSPSP_DMYSTOP(slot_end_padding & SSPSP_DMYSTOP_MASK);
		slot_end_padding >>= SSPSP_DMYSTOP_BITS;
		sspsp |= SSPSP_EDMYSTOP(slot_end_padding & SSPSP_EDMYSTOP_MASK);

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
			trace_ssp_error("ssp_set_config() error: "
					"frame_pulse_width > %d",
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

		active_tx_slots = hweight_32(config->ssp.tx_slots);
		active_rx_slots = hweight_32(config->ssp.rx_slots);

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
				trace_ssp_error("ssp_set_config() error: "
						"slot_end_padding > %d",
						SOF_DAI_INTEL_SSP_SLOT_PADDING_MAX);
				ret = -EINVAL;
				goto out;
			}

			sspsp |= SSPSP_DMYSTOP(slot_end_padding &
				SSPSP_DMYSTOP_MASK);
			slot_end_padding >>= SSPSP_DMYSTOP_BITS;
			sspsp |= SSPSP_EDMYSTOP(slot_end_padding &
				SSPSP_EDMYSTOP_MASK);
		}

		sspsp2 |= (frame_end_padding & SSPSP2_FEP_MASK);

		break;
	default:
		trace_ssp_error("ssp_set_config() error: "
				"invalid format 0x%04", config->format);
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
			trace_ssp_error("ssp_set_config() error: "
					"sample_valid_bits %d",
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

	trace_ssp("ssp_set_config(), sscr0 = 0x%08x, sscr1 = 0x%08x, ssto = 0x%08x, "
		  "sspsp = 0x%0x", sscr0, sscr1, ssto, sspsp);
	trace_ssp("ssp_set_config(), sscr2 = 0x%08x, sspsp2 = 0x%08x, sscr3 = 0x%08x, "
		  "ssioc = 0x%08x",
		  sscr2, sspsp2, sscr3, ssioc);
	trace_ssp("ssp_set_config(), ssrsa = 0x%08x, sstsa = 0x%08x", ssrsa, sstsa);

	/* TODO: move this into M/N driver */
	mn_reg_write(0x0, mdivc);
	mn_reg_write(0x80 + config->ssp.mclk_id * 0x4, mdivr);
	mn_reg_write(0x100 + config->dai_index * 0x8 + 0x0, i2s_m);
	mn_reg_write(0x100 + config->dai_index * 0x8 + 0x4, i2s_n);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_PREPARE;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_PREPARE;

out:
	spin_unlock(&dai->lock);

	return ret;
}

/* start the SSP for either playback or capture */
static void ssp_start(struct dai *dai, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	spin_lock(&dai->lock);

	/* enable port */
	ssp_update_bits(dai, SSCR0, SSCR0_SSE, SSCR0_SSE);
	ssp->state[direction] = COMP_STATE_ACTIVE;

	trace_ssp("ssp_start()");

	/* enable DMA */
	if (direction == DAI_DIR_PLAYBACK) {
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, SSCR1_TSRE);
		ssp_update_bits(dai, SSTSA, 0x1 << 8, 0x1 << 8);
	} else {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, SSCR1_RSRE);
		ssp_update_bits(dai, SSRSA, 0x1 << 8, 0x1 << 8);
	}

	spin_unlock(&dai->lock);
}

/* stop the SSP for either playback or capture */
static void ssp_stop(struct dai *dai, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	spin_lock(&dai->lock);

	/* wait to get valid fifo status */
	wait_delay(PLATFORM_SSP_STOP_DELAY);

	/* stop Rx if neeed */
	if (direction == DAI_DIR_CAPTURE &&
	    ssp->state[SOF_IPC_STREAM_CAPTURE] == COMP_STATE_ACTIVE) {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, 0);
		ssp_update_bits(dai, SSRSA, 0x1 << 8, 0x0 << 8);
		ssp_empty_rx_fifo(dai);
		ssp->state[SOF_IPC_STREAM_CAPTURE] = COMP_STATE_PAUSED;
		trace_ssp("ssp_stop(), RX stop");
	}

	/* stop Tx if needed */
	if (direction == DAI_DIR_PLAYBACK &&
	    ssp->state[SOF_IPC_STREAM_PLAYBACK] == COMP_STATE_ACTIVE) {
		ssp_empty_tx_fifo(dai);
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, 0);
		ssp_update_bits(dai, SSTSA, 0x1 << 8, 0x0 << 8);
		ssp->state[SOF_IPC_STREAM_PLAYBACK] = COMP_STATE_PAUSED;
		trace_ssp("ssp_stop(), TX stop");
	}

	/* disable SSP port if no users */
	if (ssp->state[SOF_IPC_STREAM_CAPTURE] != COMP_STATE_ACTIVE &&
	    ssp->state[SOF_IPC_STREAM_PLAYBACK] != COMP_STATE_ACTIVE) {
		ssp_update_bits(dai, SSCR0, SSCR0_SSE, 0);
		ssp->state[SOF_IPC_STREAM_CAPTURE] = COMP_STATE_PREPARE;
		ssp->state[SOF_IPC_STREAM_PLAYBACK] = COMP_STATE_PREPARE;
		trace_ssp("ssp_stop(), SSP port disabled");
	}

	spin_unlock(&dai->lock);
}

static int ssp_trigger(struct dai *dai, int cmd, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	trace_ssp("ssp_trigger() cmd %d", cmd);

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

	trace_ssp("ssp_irq_handler(), IRQ = 0x%08x", ssp_read(dai, SSSR));

	/* clear IRQ */
	ssp_write(dai, SSSR, ssp_read(dai, SSSR));
	platform_interrupt_clear(ssp_irq(dai), 1);
}

static int ssp_probe(struct dai *dai)
{
	struct ssp_pdata *ssp;
	int ret;

	if (dai_get_drvdata(dai))
		return -EEXIST; /* already created */

	/* allocate private data */
	ssp = rzalloc(RZONE_SYS_RUNTIME | RZONE_FLAG_UNCACHED,
		      SOF_MEM_CAPS_RAM, sizeof(*ssp));
	if (!ssp) {
		trace_error(TRACE_CLASS_DAI, "ssp_probe() error: "
			    "alloc failed");
		return -ENOMEM;
	}
	dai_set_drvdata(dai, ssp);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_READY;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_READY;

	/* register our IRQ handler */
	ret = interrupt_register(ssp_irq(dai), IRQ_AUTO_UNMASK, ssp_irq_handler,
				 dai);
	if (ret < 0) {
		trace_ssp_error("SSP failed to allocate IRQ %d", ssp_irq(dai));
		rfree(ssp);
		return ret;
	}

	/* Disable dynamic clock gating before touching any register */
	pm_runtime_get_sync(SSP_CLK, dai->index);

	platform_interrupt_unmask(ssp_irq(dai), 1);
	interrupt_enable(ssp_irq(dai));

	ssp_empty_rx_fifo(dai);

	return 0;
}

static int ssp_remove(struct dai *dai)
{
	interrupt_disable(ssp_irq(dai));
	platform_interrupt_mask(ssp_irq(dai), 0);
	interrupt_unregister(ssp_irq(dai));

	pm_runtime_put_sync(SSP_CLK, dai->index);

	rfree(dma_get_drvdata(dai));
	dai_set_drvdata(dai, NULL);

	return 0;
}

const struct dai_ops ssp_ops = {
	.trigger		= ssp_trigger,
	.set_config		= ssp_set_config,
	.pm_context_store	= ssp_context_store,
	.pm_context_restore	= ssp_context_restore,
	.probe			= ssp_probe,
	.remove			= ssp_remove,
};
