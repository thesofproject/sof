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
#include <reef/stream.h>
#include <reef/ssp.h>
#include <reef/alloc.h>
#include <reef/interrupt.h>
#include <config.h>

/* tracing */
#define trace_ssp(__e)	trace_event(TRACE_CLASS_SSP, __e)
#define trace_ssp_error(__e)	trace_error(TRACE_CLASS_SSP, __e)
#define tracev_ssp(__e)	tracev_event(TRACE_CLASS_SSP, __e)

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
	uint32_t i2s_m;
	uint32_t i2s_n;
	uint32_t data_size;
	uint32_t start_delay;
	uint32_t frame_end_padding;
	uint32_t frame_len = 0;
	uint32_t bdiv_min;
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
	sscr3 = SSCR3_TX(8) | SSCR3_RX(8);

	/* sspsp dynamic settings are SCMODE, SFRMP, DMYSTRT, SFRMWDTH */
	sspsp = 0;

	ssp->config = *config;
	ssp->params = config->ssp[0];

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
	sstsa = config->tx_slot_mask;

	/* ssrsa dynamic setting is RTSA, default 2 slots */
	ssrsa = config->rx_slot_mask;

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

#ifdef CLK_TYPE /* not enabled, keep the code for reference */
	/* TODO: allow topology to define SSP clock type */
	config->ssp[0].clk_id = SSP_CLK_EXT;

	/* clock source */
	switch (config->ssp[0].clk_id) {
	case SSP_CLK_AUDIO:
		sscr0 |= SSCR0_ACS;
		break;
	case SSP_CLK_NET_PLL:
		sscr0 |= SSCR0_MOD;
		break;
	case SSP_CLK_EXT:
		sscr0 |= SSCR0_ECS;
		break;
	case SSP_CLK_NET:
		sscr0 |= SSCR0_NCS | SSCR0_MOD;
		break;
	default:
		trace_ssp_error("ec4");
		ret = -EINVAL;
		goto out;
	}
#elif CONFIG_APOLLOLAKE
	sscr0 |= SSCR0_MOD | SSCR0_ACS | SSCR0_ECS;
#else
	sscr0 |= SSCR0_MOD | SSCR0_ACS;
#endif

	/* BCLK is generated from MCLK - must be divisable */
	if (config->mclk % config->bclk) {
		trace_ssp_error("ec5");
		ret = -EINVAL;
		goto out;
	}

	/* divisor must be within SCR range */
	mdiv = (config->mclk / config->bclk) - 1;
	if (mdiv > (SSCR0_SCR_MASK >> 8)) {
		trace_ssp_error("ec6");
		ret = -EINVAL;
		goto out;
	}

	/* set the SCR divisor */
	sscr0 |= SSCR0_SCR(mdiv);

	/* calc frame width based on BCLK and rate - must be divisable */
	if (config->bclk % config->fclk) {
		trace_ssp_error("ec7");
		ret = -EINVAL;
		goto out;
	}

	/* must be enough BCLKs for data */
	bdiv = config->bclk / config->fclk;
	if (bdiv < config->sample_container_bits * config->num_slots) {
		trace_ssp_error("ec8");
		ret = -EINVAL;
		goto out;
	}

	/* sample_container_bits must be <= 38 for SSP */
	if (config->sample_container_bits > 38) {
		trace_ssp_error("ec9");
		ret = -EINVAL;
		goto out;
	}

	/* format */
	switch (config->format & SOF_DAI_FMT_FORMAT_MASK) {
	case SOF_DAI_FMT_I2S:

		start_delay = 1;

		sscr0 |= SSCR0_FRDC(config->num_slots);

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

		break;

	case SOF_DAI_FMT_LEFT_J:

		start_delay = 0;

		sscr0 |= SSCR0_FRDC(config->num_slots);

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

		break;
	case SOF_DAI_FMT_DSP_A:

		start_delay = 0;

		sscr0 |= SSCR0_MOD | SSCR0_FRDC(config->num_slots);

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

		break;
	case SOF_DAI_FMT_DSP_B:

		start_delay = 0;

		sscr0 |= SSCR0_MOD | SSCR0_FRDC(config->num_slots);

		/* set asserted frame length */
		frame_len = 1;

		/*
		 * handle frame polarity, DSP_B default is rising/active high,
		 * non-inverted(inverted_frame=0) -- active high(SFRMP=1),
		 * inverted(inverted_frame=1) -- falling/active low(SFRMP=0),
		 * so, we should set SFRMP to !inverted_frame.
		 */
		sspsp |= SSPSP_SFRMP(!inverted_frame);

		break;
	default:
		trace_ssp_error("eca");
		ret = -EINVAL;
		goto out;
	}

	sspsp |= SSPSP_STRTDLY(start_delay);
	sspsp |= SSPSP_SFRMWDTH(frame_len);

	bdiv_min = config->num_slots * config->sample_valid_bits;
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

	sspsp2 |= (frame_end_padding & SSPSP2_FEP_MASK);

	data_size = config->sample_valid_bits;

	if (data_size > 16)
		sscr0 |= (SSCR0_EDSS | SSCR0_DSIZE(data_size - 16));
	else
		sscr0 |= SSCR0_DSIZE(data_size);

#ifdef CONFIG_CANNONLAKE
	mdivc = 0x1;
#else
	mdivc = 0x00100001;
#endif
	/* bypass divider for MCLK */
	mdivr = 0x00000fff;

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
	mn_reg_write(0x80, mdivr);
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
static void ssp_stop(struct dai *dai)
{
	struct ssp_pdata *ssp = dai_get_drvdata(dai);

	spin_lock(&ssp->lock);

	/* stop Rx if we are not capturing */
	if (ssp->state[SOF_IPC_STREAM_CAPTURE] != COMP_STATE_ACTIVE) {
		ssp_update_bits(dai, SSCR1, SSCR1_RSRE, 0);
		ssp_update_bits(dai, SSTSA, 0x1 << 8, 0x0 << 8);
		trace_ssp("Ss0");
	}

	/* stop Tx if we are not playing */
	if (ssp->state[SOF_IPC_STREAM_PLAYBACK] != COMP_STATE_ACTIVE) {
		ssp_update_bits(dai, SSCR1, SSCR1_TSRE, 0);
		ssp_update_bits(dai, SSRSA, 0x1 << 8, 0x0 << 8);
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
	case COMP_CMD_START:
		if (ssp->state[direction] == COMP_STATE_PREPARE ||
			ssp->state[direction] == COMP_STATE_PAUSED)
			ssp_start(dai, direction);
		break;
	case COMP_CMD_RELEASE:
		if (ssp->state[direction] == COMP_STATE_PAUSED ||
			ssp->state[direction] == COMP_STATE_PREPARE)
			ssp_start(dai, direction);
		break;
	case COMP_CMD_STOP:
	case COMP_CMD_PAUSE:
		ssp->state[direction] = COMP_STATE_PAUSED;
		ssp_stop(dai);
		break;
	case COMP_CMD_RESUME:
		ssp_context_restore(dai);
		break;
	case COMP_CMD_SUSPEND:
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
	platform_interrupt_clear(ssp_irq(dai), 1);
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
	platform_interrupt_unmask(ssp_irq(dai), 1);
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
