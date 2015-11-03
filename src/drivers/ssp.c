/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <errno.h>
#include <reef/dai.h>
#include <reef/io.h>
#include <reef/stream.h>
#include <reef/ssp.h>

/* SSP register offsets */
#define SSCR0		0x00
#define SSCR1		0x04
#define SSSR		0x08
#define SSITR		0x0C
#define SSDR		0x10
#define SSTO		0x28
#define SSPSP		0x2C
#define SSTSA		0x30
#define SSRSA		0x34
#define SSTSS		0x38

/* SSCR0 bits */
#define SSCR0_DSS_MASK	(0x0000000f)
#define SSCR0_DSIZE(x)  ((x) - 1)
#define SSCR0_FRF	(0x00000030)
#define SSCR0_MOT	(00 << 4)
#define SSCR0_TI	(1 << 4)
#define SSCR0_NAT	(2 << 4)
#define SSCR0_PSP	(3 << 4)
#define SSCR0_ECS	(1 << 6)
#define SSCR0_SSE	(1 << 7)
#define SSCR0_SCR(x)	((x) << 8)
#define SSCR0_EDSS	(1 << 20)
#define SSCR0_NCS	(1 << 21)
#define SSCR0_RIM	(1 << 22)
#define SSCR0_TUM	(1 << 23)
#define SSCR0_FRDC	(0x07000000)
#define SSCR0_ACS	(1 << 30)
#define SSCR0_MOD	(1 << 31)

/* SSCR1 bits */
#define SSCR1_RIE	(1 << 0)
#define SSCR1_TIE	(1 << 1)
#define SSCR1_LBM	(1 << 2)
#define SSCR1_SPO	(1 << 3)
#define SSCR1_SPH	(1 << 4)
#define SSCR1_MWDS	(1 << 5)
#define SSCR1_TFT_MASK	(0x000003c0)
#define SSCR1_TX(x) (((x) - 1) << 6)
#define SSCR1_RFT_MASK	(0x00003c00)
#define SSCR1_RX(x) (((x) - 1) << 10)
#define SSCR1_EFWR	(1 << 14)
#define SSCR1_STRF	(1 << 15)
#define SSCR1_IFS	(1 << 16)
#define SSCR1_PINTE	(1 << 18)
#define SSCR1_TINTE	(1 << 19)
#define SSCR1_RSRE	(1 << 20)
#define SSCR1_TSRE	(1 << 21)
#define SSCR1_TRAIL	(1 << 22)
#define SSCR1_RWOT	(1 << 23)
#define SSCR1_SFRMDIR	(1 << 24)
#define SSCR1_SCLKDIR	(1 << 25)
#define SSCR1_ECRB	(1 << 26)
#define SSCR1_ECRA	(1 << 27)
#define SSCR1_SCFR	(1 << 28)
#define SSCR1_EBCEI	(1 << 29)
#define SSCR1_TTE	(1 << 30)
#define SSCR1_TTELP	(1 << 31)

/* SSR bits */
#define SSSR_TNF	(1 << 2)
#define SSSR_RNE	(1 << 3)
#define SSSR_BSY	(1 << 4)
#define SSSR_TFS	(1 << 5)
#define SSSR_RFS	(1 << 6)
#define SSSR_ROR	(1 << 7)

/* SSPSP bits */
#define SSPSP_SCMODE(x)		((x) << 0)
#define SSPSP_SFRMP		(1 << 2)
#define SSPSP_ETDS		(1 << 3)
#define SSPSP_STRTDLY(x)	((x) << 4)
#define SSPSP_DMYSTRT(x)	((x) << 7)
#define SSPSP_SFRMDLY(x)	((x) << 9)
#define SSPSP_SFRMWDTH(x)	((x) << 16)
#define SSPSP_DMYSTOP(x)	((x) << 23)
#define SSPSP_FSRT		(1 << 25)

/* SSP private data */
struct ssp_config {
	uint32_t sscr0;
	uint32_t sscr1;
	uint32_t psp;
};

static int ssp_context_store(struct dai *dai)
{
	struct ssp_config *ssp = (struct ssp_config *)dai->data;

	ssp->sscr0 = io_reg_read(dai->base + SSCR0);
	ssp->sscr1 = io_reg_read(dai->base + SSCR1);
	ssp->psp = io_reg_read(dai->base + SSPSP);

	return 0;
}

static int ssp_context_restore(struct dai *dai)
{
	struct ssp_config *ssp = (struct ssp_config *)dai->data;

	io_reg_write(dai->base + SSCR0, ssp->sscr0);
	io_reg_write(dai->base + SSCR1, ssp->sscr1);
	io_reg_write(dai->base + SSPSP, ssp->psp);

	return 0;
}

/* Digital Audio interface formatting */
static inline int ssp_set_fmt(struct dai *dai)
{
	struct ssp_config *ssp = (struct ssp_config *)dai->data;
	uint32_t sscr0, sscr1, sspsp;

	/* reset SSP settings */
	sscr0 = 0;
	sscr1 = 0;
	sspsp = 0;

	/* clock masters */
	switch (dai->config.format & DAI_FMT_MASTER_MASK) {
	case DAI_FMT_CBM_CFM:
		sscr1 |= SSCR1_SCLKDIR | SSCR1_SFRMDIR | SSCR1_SCFR;
		break;
	case DAI_FMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* clock signal polarity */
	switch (dai->config.format & DAI_FMT_INV_MASK) {
	case DAI_FMT_NB_NF:
		sspsp |= SSPSP_SFRMP;
		break;
	case DAI_FMT_NB_IF:
		break;
	case DAI_FMT_IB_IF:
		sspsp |= SSPSP_SCMODE(2);
		break;
	case DAI_FMT_IB_NF:
		sspsp |= SSPSP_SCMODE(2) | SSPSP_SFRMP;
		break;
	default:
		return -EINVAL;
	}

	/* clock source */
	switch (dai->config.clk_src) {
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
		return -ENODEV;
	}

	/* format */
	switch (dai->config.format & DAI_FMT_FORMAT_MASK) {
	case DAI_FMT_I2S:
		sscr0 |= SSCR0_PSP;
		sscr1 |= SSCR1_RWOT | SSCR1_TRAIL;
		sspsp |= SSPSP_SFRMWDTH(dai->config.frame_size + 1);
		sspsp |= SSPSP_SFRMDLY((dai->config.frame_size + 1) * 2);
		sspsp |= SSPSP_DMYSTRT(1);
		break;
	case DAI_FMT_DSP_A:
		sspsp |= SSPSP_FSRT;
	case DAI_FMT_DSP_B:
		sscr0 |= SSCR0_PSP;
		sscr1 |= SSCR1_TRAIL | SSCR1_RWOT;
		break;
	default:
		return -EINVAL;
	}

	/* frame size */
	if (dai->config.frame_size > 16)
		sscr0 |= (SSCR0_EDSS | SSCR0_DSIZE(dai->config.frame_size - 16));
	else
		sscr0 |= SSCR0_DSIZE(dai->config.frame_size);

	io_reg_write(dai->base + SSCR0, sscr0);
	io_reg_write(dai->base + SSCR1, sscr1);
	io_reg_write(dai->base + SSPSP, sspsp);

	return 0;
}

static void ssp_start(struct dai *dai, int direction)
{
	struct ssp_config *ssp = (struct ssp_config *)dai->data;

	/* enable port */
	io_reg_update_bits(dai->base + SSCR0, SSCR0_SSE, SSCR0_SSE);

	/* enable DMA */
	if (direction == DAI_DIR_PLAYBACK)
		io_reg_update_bits(dai->base + SSCR1, SSCR1_TSRE, SSCR1_TSRE);
	else
		io_reg_update_bits(dai->base + SSCR1, SSCR1_RSRE, SSCR1_RSRE);
}

static void ssp_stop(struct dai *dai, int direction)
{
	struct ssp_config *ssp = (struct ssp_config *)dai->data;
	uint32_t sscr1;

	/* disable DMA */
	if (direction == DAI_DIR_PLAYBACK)
		io_reg_update_bits(dai->base + SSCR1, SSCR1_TSRE, 0);
	else
		io_reg_update_bits(dai->base + SSCR1, SSCR1_RSRE, 0);

	/* disable port if no users */
	sscr1 = io_reg_read(dai->base + SSCR1);
	if (!(sscr1 & SSCR1_TSRE | SSCR1_RSRE))
		io_reg_update_bits(dai->base + SSCR0, SSCR0_SSE, 0);
}

static int ssp_trigger(struct dai *dai, int cmd, int direction)
{
	switch (cmd) {
	case DAI_TRIGGER_START:
	case DAI_TRIGGER_PAUSE_RELEASE:
		ssp_start(dai, direction);
		break;
	case DAI_TRIGGER_PAUSE_PUSH:
	case DAI_TRIGGER_STOP:
		ssp_stop(dai, direction);
		break;
	case DAI_TRIGGER_RESUME:
		ssp_context_restore(dai);
		ssp_start(dai, direction);
		break;
	case DAI_TRIGGER_SUSPEND:
		ssp_stop(dai, direction);
		ssp_context_store(dai);
		break;
	default:
		break;
	}

	return 0;
}

const struct dai_ops ssp_ops = {
	.trigger		= ssp_trigger,
	.set_fmt		= ssp_set_fmt,
	.pm_context_store	= ssp_context_store,
	.pm_context_restore	= ssp_context_restore,
};
