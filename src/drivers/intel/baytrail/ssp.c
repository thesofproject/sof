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
 */

#include <errno.h>
#include <stdbool.h>
#include <sof/stream.h>
#include <sof/ssp.h>
#include <sof/alloc.h>
#include <sof/interrupt.h>

/* tracing */
#define trace_ssp(__e, ...)	trace_event(TRACE_CLASS_SSP, __e, ##__VA_ARGS__)
#define trace_ssp_error(__e, ...)	trace_error(TRACE_CLASS_SSP, __e, ##__VA_ARGS__)
#define tracev_ssp(__e, ...)	tracev_event(TRACE_CLASS_SSP, __e, ##__VA_ARGS__)

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
		entries = SFIFOL_RFL(ssp_read(dai, SFIFOL));
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
	uint32_t sscr4;
	uint32_t sscr5;
	uint32_t sspsp;
	uint32_t sfifott;
	uint32_t mdiv;
	uint32_t bdiv;
	uint32_t data_size;
	uint32_t start_delay;
	uint32_t active_tx_slots = 2;
	uint32_t active_rx_slots = 2;
	uint32_t frame_len = 0;
	bool inverted_frame = false;
	bool cfs = false;
	bool cbs = false;
	int ret = 0;

	spin_lock(&dai->lock);

	/* is playback/capture already running */
	if (ssp->state[DAI_DIR_PLAYBACK] == COMP_STATE_ACTIVE ||
	    ssp->state[DAI_DIR_CAPTURE] == COMP_STATE_ACTIVE) {
		trace_ssp_error("ssp_set_config(): "
				"playback/capture already running");
		ret = -EINVAL;
		goto out;
	}

	trace_ssp("ssp_set_config(), config->format = %d", config->format);

	/* reset SSP settings */
	/* sscr0 dynamic settings are DSS, EDSS, SCR, FRDC, ECS */
	/*
	 * FIXME: MOD, ACS, NCS are not set,
	 * no support for network mode for now
	 */
	sscr0 = SSCR0_PSP | SSCR0_RIM | SSCR0_TIM;

	/*
	 * FIXME: PINTE and RWOT are not set in sscr1
	 *   sscr1 = SSCR1_PINTE | SSCR1_RWOT;
	 */

	/* sscr1 dynamic settings are TFT, RFT, SFRMDIR, SCLKDIR, SCFR */
	sscr1 = 0;
#ifdef ENABLE_SSCR1_TRISTATE
	sscr1 |= SSCR1_TTE; /* make sure SDO line is tri-stated when inactive */
#endif
#ifdef ENABLE_TIE_RIE /* FIXME: not enabled, difference with SST driver */
	sscr1 |= SSCR1_TIE | SSCR1_RIE;
#endif

	/* sscr2 dynamic setting is SLV_EXT_CLK_RUN_EN */
	sscr2 = SSCR2_URUN_FIX0;
	sscr2 |= SSCR2_ASRC_INTR_MASK;
#ifdef ENABLE_SSCR2_FIXES /* FIXME: is this needed ? */
	sscr2 |= SSCR2_UNDRN_FIX_EN | SSCR2_FIFO_EMPTY_FIX_EN;
#endif

	/*
	 * sscr3 dynamic settings are FRM_MS_EN, I2S_MODE_EN, I2S_FRM_POL,
	 * I2S_TX_EN, I2S_RX_EN, I2S_CLK_MST
	 */
	sscr3 = SSCR3_SYN_FIX_EN;
#ifdef ENABLE_SSRC3_FIXES
	/*
	 * this seems to prevent DSP modes from working but is harmless for
	 * I2S and LEFT_J. Keep with ifdef in case it's ever needed.
	 */
	sscr3 |= SSCR3_I2S_TX_SS_FIX_EN | SSCR3_I2S_RX_SS_FIX_EN |
		SSCR3_STRETCH_TX | SSCR3_STRETCH_RX;
#endif

#ifdef ENABLE_CLK_EDGE_SEL /* FIXME: is this needed ? */
	sscr3 |= SSCR3_CLK_EDGE_SEL;
#endif

	/* sscr4 dynamic settings is TOT_FRAME_PRD */
	sscr4 = 0x0;

	/* sscr4 dynamic settings are FRM_ASRT_CLOCKS and FRM_POLARITY */
	sscr5 = 0x0;

	/* sspsp dynamic settings are SCMODE, SFRMP, DMYSTRT, SFRMWDTH */
	sspsp = SSPSP_ETDS; /* last value (bit 0) */

	ssp->config = *config;
	ssp->params = config->ssp;

	/* clock masters */
	/*
	 * On TNG/BYT/CHT, the SSP wrapper generates the fs even in master mode,
	 * the master/slave choice depends on the clock type
	 */
	sscr1 |= SSCR1_SFRMDIR;

	switch (config->format & SOF_DAI_FMT_MASTER_MASK) {
	case SOF_DAI_FMT_CBM_CFM:
		sscr0 |= SSCR0_ECS; /* external clock used */
		sscr1 |= SSCR1_SCLKDIR;
		/*
		 * FIXME: does SSRC1.SCFR need to be set
		 * when codec is master ?
		 */
		sscr2 |= SSCR2_SLV_EXT_CLK_RUN_EN;
		break;
	case SOF_DAI_FMT_CBS_CFS:
#ifdef ENABLE_SSRCR1_SCFR /* FIXME: is this needed ? */
		sscr1 |= SSCR1_SCFR;
#endif
		sscr3 |= SSCR3_FRM_MST_EN;
		cfs = true;
		cbs = true;
		break;
	case SOF_DAI_FMT_CBM_CFS:
		sscr0 |= SSCR0_ECS; /* external clock used */
		sscr1 |= SSCR1_SCLKDIR;
		/*
		 * FIXME: does SSRC1.SCFR need to be set
		 * when codec is master ?
		 */
		sscr2 |= SSCR2_SLV_EXT_CLK_RUN_EN;
		sscr3 |= SSCR3_FRM_MST_EN;
		cfs = true;
		/* FIXME: this mode has not been tested */
		break;
	case SOF_DAI_FMT_CBS_CFM:
#ifdef ENABLE_SSRCR1_SCFR /* FIXME: is this needed ? */
		sscr1 |= SSCR1_SCFR;
#endif
		/* FIXME: this mode has not been tested */
		cbs = true;
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
		sspsp |= SSPSP_SCMODE(2);
		inverted_frame = true; /* handled later with format */
		break;
	case SOF_DAI_FMT_IB_NF:
		sspsp |= SSPSP_SCMODE(2);
		break;
	default:
		trace_ssp_error("ssp_set_config() error: "
				"format & INV_MASK EINVAL");
		ret = -EINVAL;
		goto out;
	}

	/* BCLK is generated from MCLK - must be divisable */
	if (config->ssp.mclk_rate % config->ssp.bclk_rate) {
		trace_ssp_error("ssp_set_config() error: "
				"MCLK is not divisable");
		ret = -EINVAL;
		goto out;
	}

	/* divisor must be within SCR range */
	mdiv = (config->ssp.mclk_rate / config->ssp.bclk_rate) - 1;
	if (mdiv > (SSCR0_SCR_MASK >> 8)) {
		trace_ssp_error("ssp_set_config() error: "
				"divisor is not within SCR range");
		ret = -EINVAL;
		goto out;
	}

	/* set the SCR divisor */
	sscr0 |= SSCR0_SCR(mdiv);

	/* calc frame width based on BCLK and rate - must be divisable */
	if (config->ssp.bclk_rate % config->ssp.fsync_rate) {
		trace_ssp_error("ssp_set_config() error: "
				"BLCK is not divisable");
		ret = -EINVAL;
		goto out;
	}

	/* must be enouch BCLKs for data */
	bdiv = config->ssp.bclk_rate / config->ssp.fsync_rate;
	if (bdiv < config->ssp.tdm_slot_width *
	    config->ssp.tdm_slots) {
		trace_ssp_error("ssp_set_config() error: not enough BCLKs");
		ret = -EINVAL;
		goto out;
	}

	/* tdm_slot_width must be <= 38 for SSP */
	if (config->ssp.tdm_slot_width > 38) {
		trace_ssp_error("ssp_set_config() error: tdm_slot_width > 38");
		ret = -EINVAL;
		goto out;
	}

	/* format */
	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_I2S:

		start_delay = 1;

		/* enable I2S mode */
		sscr3 |= SSCR3_I2S_MODE_EN | SSCR3_I2S_TX_EN | SSCR3_I2S_RX_EN;

		/* set asserted frame length */
		frame_len = config->ssp.tdm_slot_width;

		/* handle frame polarity, I2S default is falling/active low */
		sspsp |= SSPSP_SFRMP(!inverted_frame);
		sscr3 |= SSCR3_I2S_FRM_POL(!inverted_frame);

		if (cbs) {
			/*
			 * keep RX functioning on a TX underflow
			 * (I2S/LEFT_J master only)
			 */
			sscr3 |= SSCR3_MST_CLK_EN;

			/*
			 * total frame period (both asserted and
			 * deasserted time of frame
			 */
			sscr4 |= SSCR4_TOT_FRM_PRD(frame_len << 1);
		}

		break;

	case SOF_DAI_FMT_LEFT_J:

		start_delay = 0;

		/* apparently we need the same initialization as for I2S */
		sscr3 |= SSCR3_I2S_MODE_EN | SSCR3_I2S_TX_EN | SSCR3_I2S_RX_EN;

		/* set asserted frame length */
		frame_len = config->ssp.tdm_slot_width;

		/* LEFT_J default is rising/active high, opposite of I2S */
		sspsp |= SSPSP_SFRMP(inverted_frame);
		sscr3 |= SSCR3_I2S_FRM_POL(inverted_frame);

		if (cbs) {
			/*
			 * keep RX functioning on a TX underflow
			 * (I2S/LEFT_J master only)
			 */
			sscr3 |= SSCR3_MST_CLK_EN;

			/*
			 * total frame period (both asserted and
			 * deasserted time of frame
			 */
			sscr4 |= SSCR4_TOT_FRM_PRD(frame_len << 1);
		}

		break;
	case SOF_DAI_FMT_DSP_A:

		start_delay = 1;

		sscr0 |= SSCR0_MOD | SSCR0_FRDC(config->ssp.tdm_slots);

		/* set asserted frame length */
		frame_len = 1;

		/* handle frame polarity, DSP_A default is rising/active high */
		sspsp |= SSPSP_SFRMP(!inverted_frame);
		if (cfs) {
			/* set sscr frame polarity in DSP/master mode only */
			sscr5 |= SSCR5_FRM_POLARITY(inverted_frame);
		}

		/*
		 * total frame period (both asserted and
		 * deasserted time of frame)
		 */
		if (cbs)
			sscr4 |= SSCR4_TOT_FRM_PRD(config->ssp.tdm_slots *
					   config->ssp.tdm_slot_width);

		active_tx_slots = hweight_32(config->ssp.tx_slots);
		active_rx_slots = hweight_32(config->ssp.rx_slots);

		break;
	case SOF_DAI_FMT_DSP_B:

		start_delay = 0;

		sscr0 |= SSCR0_MOD | SSCR0_FRDC(config->ssp.tdm_slots);

		/* set asserted frame length */
		frame_len = 1;

		/* handle frame polarity, DSP_A default is rising/active high */
		sspsp |= SSPSP_SFRMP(!inverted_frame);
		if (cfs) {
			/* set sscr frame polarity in DSP/master mode only */
			sscr5 |= SSCR5_FRM_POLARITY(inverted_frame);
		}

		/*
		 * total frame period (both asserted and
		 * deasserted time of frame
		 */
		if (cbs)
			sscr4 |= SSCR4_TOT_FRM_PRD(config->ssp.tdm_slots *
					   config->ssp.tdm_slot_width);

		active_tx_slots = hweight_32(config->ssp.tx_slots);
		active_rx_slots = hweight_32(config->ssp.rx_slots);

		break;
	default:
		trace_ssp_error("ssp_set_config() error: "
				"format & FORMAT_MASK EINVAL");
		ret = -EINVAL;
		goto out;
	}

	sspsp |= SSPSP_DMYSTRT(start_delay);
	sspsp |= SSPSP_SFRMWDTH(frame_len);
	sscr5 |= SSCR5_FRM_ASRT_CLOCKS(frame_len);

	data_size = config->ssp.sample_valid_bits;

	if (data_size > 16)
		sscr0 |= (SSCR0_EDSS | SSCR0_DSIZE(data_size - 16));
	else
		sscr0 |= SSCR0_DSIZE(data_size);

	/* FIXME:
	 * watermarks - (RFT + 1) should equal DMA SRC_MSIZE
	 */
	sfifott = (SFIFOTT_TX(2 * active_tx_slots) |
		   SFIFOTT_RX(2 * active_rx_slots));

	ssp_write(dai, SSCR0, sscr0);
	ssp_write(dai, SSCR1, sscr1);
	ssp_write(dai, SSCR2, sscr2);
	ssp_write(dai, SSCR3, sscr3);
	ssp_write(dai, SSCR4, sscr4);
	ssp_write(dai, SSCR5, sscr5);
	ssp_write(dai, SSPSP, sspsp);
	ssp_write(dai, SFIFOTT, sfifott);
	ssp_write(dai, SSTSA, config->ssp.tx_slots);
	ssp_write(dai, SSRSA, config->ssp.rx_slots);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_PREPARE;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_PREPARE;

	trace_ssp("ssp_set_config(), done");

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
	if (direction == DAI_DIR_PLAYBACK)
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, SSCR1_TSRE);
	else
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, SSCR1_RSRE);

	spin_unlock(&dai->lock);
}

/* stop the SSP for either playback or capture */
static void ssp_stop(struct dai *dai, int direction)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	spin_lock(&dai->lock);

	/* stop Rx if neeed */
	if (direction == DAI_DIR_CAPTURE &&
	    ssp->state[SOF_IPC_STREAM_CAPTURE] == COMP_STATE_ACTIVE) {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, 0);
		ssp_empty_rx_fifo(dai);
		ssp->state[SOF_IPC_STREAM_CAPTURE] = COMP_STATE_PAUSED;
		trace_ssp("ssp_stop(), RX stop");
	}

	/* stop Tx if needed */
	if (direction == DAI_DIR_PLAYBACK &&
	    ssp->state[SOF_IPC_STREAM_PLAYBACK] == COMP_STATE_ACTIVE) {
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, 0);
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

	trace_ssp("ssp_trigger()");

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

	trace_ssp("ssp_irq_handler(), IRQ = %u", ssp_read(dai, SSSR));

	/* clear IRQ */
	ssp_write(dai, SSSR, ssp_read(dai, SSSR));
	platform_interrupt_clear(ssp_irq(dai), 1);
}

static int ssp_probe(struct dai *dai)
{
	struct ssp_pdata *ssp;
	int ret;

	/* allocate private data */
	ssp = rzalloc(RZONE_SYS | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
		      sizeof(*ssp));
	dai_set_drvdata(dai, ssp);

	spinlock_init(&dai->lock);

	ssp->state[DAI_DIR_PLAYBACK] = COMP_STATE_READY;
	ssp->state[DAI_DIR_CAPTURE] = COMP_STATE_READY;

#if defined CONFIG_CHERRYTRAIL
	/* register our IRQ handler
	 * - CHT shares SSP 0,1,2 IRQs with SSP 3,4,5
	 */
	if (ssp_irq(dai) >= IRQ_CHT_SSP_OFFSET)
		ret = interrupt_register(ssp_irq(dai) - IRQ_CHT_SSP_OFFSET,
					 IRQ_AUTO_UNMASK, ssp_irq_handler, dai);
	else
		ret = interrupt_register(ssp_irq(dai), IRQ_AUTO_UNMASK,
					 ssp_irq_handler, dai);
#else
	/* register our IRQ handler */
	ret = interrupt_register(ssp_irq(dai), IRQ_AUTO_UNMASK, ssp_irq_handler,
				 dai);
#endif
	if (ret < 0) {
		trace_ssp_error("SSP failed to allocate IRQ");
		rfree(ssp);
		return ret;
	}

	platform_interrupt_unmask(ssp_irq(dai), 1);
	interrupt_enable(ssp_irq(dai));

	ssp_empty_rx_fifo(dai);
	return 0;
}

const struct dai_ops ssp_ops = {
	.trigger		= ssp_trigger,
	.set_config		= ssp_set_config,
	.pm_context_store	= ssp_context_store,
	.pm_context_restore	= ssp_context_restore,
	.probe			= ssp_probe,
};
