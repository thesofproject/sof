/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_SSP_H__
#define __SOF_DRIVERS_SSP_H__

#include <sof/bit.h>
#include <sof/lib/clk.h>
#include <sof/lib/dai.h>
#include <sof/lib/wait.h>
#include <sof/trace/trace.h>
#include <ipc/dai.h>
#include <ipc/dai-intel.h>
#include <user/trace.h>

#include <stdint.h>

#define SSP_CLOCK_XTAL_OSCILLATOR	0x0
#define SSP_CLOCK_AUDIO_CARDINAL	0x1
#define SSP_CLOCK_PLL_FIXED		0x2

extern const struct freq_table *ssp_freq;
extern const uint32_t *ssp_freq_sources;

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
#define SSCR2		0x40

#if CONFIG_BAYTRAIL || CONFIG_CHERRYTRAIL || CONFIG_BROADWELL || CONFIG_HASWELL
#define SFIFOL		0x68
#define SFIFOTT		0x6C
#define SSCR3		0x70
#define SSCR4		0x74
#define SSCR5		0x78
#endif

extern const struct dai_driver ssp_driver;

/* SSCR0 bits */
#define SSCR0_DSIZE(x)	SET_BITS(3, 0, (x) - 1)
#define SSCR0_FRF	MASK(5, 4)
#define SSCR0_MOT	SET_BITS(5, 4, 0)
#define SSCR0_TI	SET_BITS(5, 4, 1)
#define SSCR0_NAT	SET_BITS(5, 4, 2)
#define SSCR0_PSP	SET_BITS(5, 4, 3)
#define SSCR0_ECS	BIT(6)
#define SSCR0_SSE	BIT(7)
#define SSCR0_SCR_MASK	MASK(19, 8)
#define SSCR0_SCR(x)	SET_BITS(19, 8, x)
#define SSCR0_EDSS	BIT(20)
#define SSCR0_NCS	BIT(21)
#define SSCR0_RIM	BIT(22)
#define SSCR0_TIM	BIT(23)
#define SSCR0_FRDC(x)	SET_BITS(26, 24, (x) - 1)
#define SSCR0_ACS	BIT(30)
#define SSCR0_MOD	BIT(31)

/* SSCR1 bits */
#define SSCR1_RIE	BIT(0)
#define SSCR1_TIE	BIT(1)
#define SSCR1_LBM	BIT(2)
#define SSCR1_SPO	BIT(3)
#define SSCR1_SPH	BIT(4)
#define SSCR1_MWDS	BIT(5)
#define SSCR1_TFT_MASK	MASK(9, 6)
#define SSCR1_TFT(x)	SET_BITS(9, 6, (x) - 1)
#define SSCR1_RFT_MASK	MASK(13, 10)
#define SSCR1_RFT(x)	SET_BITS(13, 10, (x) - 1)
#define SSCR1_EFWR	BIT(14)
#define SSCR1_STRF	BIT(15)
#define SSCR1_IFS	BIT(16)
#define SSCR1_PINTE	BIT(18)
#define SSCR1_TINTE	BIT(19)
#define SSCR1_RSRE	BIT(20)
#define SSCR1_TSRE	BIT(21)
#define SSCR1_TRAIL	BIT(22)
#define SSCR1_RWOT	BIT(23)
#define SSCR1_SFRMDIR	BIT(24)
#define SSCR1_SCLKDIR	BIT(25)
#define SSCR1_ECRB	BIT(26)
#define SSCR1_ECRA	BIT(27)
#define SSCR1_SCFR	BIT(28)
#define SSCR1_EBCEI	BIT(29)
#define SSCR1_TTE	BIT(30)
#define SSCR1_TTELP	BIT(31)

#if CONFIG_BAYTRAIL || CONFIG_CHERRYTRAIL
/* SSCR2 bits */
#define SSCR2_URUN_FIX0	BIT(0)
#define SSCR2_URUN_FIX1	BIT(1)
#define SSCR2_SLV_EXT_CLK_RUN_EN	BIT(2)
#define SSCR2_CLK_DEL_EN		BIT(3)
#define SSCR2_UNDRN_FIX_EN		BIT(6)
#define SSCR2_FIFO_EMPTY_FIX_EN		BIT(7)
#define SSCR2_ASRC_CNTR_EN		BIT(8)
#define SSCR2_ASRC_CNTR_CLR		BIT(9)
#define SSCR2_ASRC_FRM_CNRT_EN		BIT(10)
#define SSCR2_ASRC_INTR_MASK		BIT(11)
#elif CONFIG_CAVS || CONFIG_HASWELL || CONFIG_BROADWELL
#define SSCR2_TURM1		BIT(1)
#define SSCR2_PSPSRWFDFD	BIT(3)
#define SSCR2_PSPSTWFDFD	BIT(4)
#define SSCR2_SDFD		BIT(14)
#define SSCR2_SDPM		BIT(16)
#define SSCR2_LJDFD		BIT(17)
#define SSCR2_MMRATF		BIT(18)
#define SSCR2_SMTATF		BIT(19)
#endif

/* SSR bits */
#define SSSR_TNF	BIT(2)
#define SSSR_RNE	BIT(3)
#define SSSR_BSY	BIT(4)
#define SSSR_TFS	BIT(5)
#define SSSR_RFS	BIT(6)
#define SSSR_ROR	BIT(7)
#define SSSR_TUR	BIT(21)

/* SSPSP bits */
#define SSPSP_SCMODE(x)		SET_BITS(1, 0, x)
#define SSPSP_SFRMP(x)		SET_BIT(2, x)
#define SSPSP_ETDS		BIT(3)
#define SSPSP_STRTDLY(x)	SET_BITS(6, 4, x)
#define SSPSP_DMYSTRT(x)	SET_BITS(8, 7, x)
#define SSPSP_SFRMDLY(x)	SET_BITS(15, 9, x)
#define SSPSP_SFRMWDTH(x)	SET_BITS(21, 16, x)
#define SSPSP_DMYSTOP(x)	SET_BITS(24, 23, x)
#define SSPSP_DMYSTOP_BITS	2
#define SSPSP_DMYSTOP_MASK	MASK(SSPSP_DMYSTOP_BITS - 1, 0)
#define SSPSP_FSRT		BIT(25)
#define SSPSP_EDMYSTOP(x)	SET_BITS(28, 26, x)

#define SSPSP2			0x44
#define SSPSP2_FEP_MASK		0xff

#if CONFIG_CAVS
#define SSCR3		0x48
#define SSIOC		0x4C

#define SSP_REG_MAX	SSIOC
#endif

/* SSTSA bits */
#define SSTSA_SSTSA(x)		SET_BITS(7, 0, x)
#define SSTSA_TXEN		BIT(8)

/* SSRSA bits */
#define SSRSA_SSRSA(x)		SET_BITS(7, 0, x)
#define SSRSA_RXEN		BIT(8)

/* SSCR3 bits */
#define SSCR3_FRM_MST_EN	BIT(0)
#define SSCR3_I2S_MODE_EN	BIT(1)
#define SSCR3_I2S_FRM_POL(x)	SET_BIT(2, x)
#define SSCR3_I2S_TX_SS_FIX_EN	BIT(3)
#define SSCR3_I2S_RX_SS_FIX_EN	BIT(4)
#define SSCR3_I2S_TX_EN		BIT(9)
#define SSCR3_I2S_RX_EN		BIT(10)
#define SSCR3_CLK_EDGE_SEL	BIT(12)
#define SSCR3_STRETCH_TX	BIT(14)
#define SSCR3_STRETCH_RX	BIT(15)
#define SSCR3_MST_CLK_EN	BIT(16)
#define SSCR3_SYN_FIX_EN	BIT(17)

/* SSCR4 bits */
#define SSCR4_TOT_FRM_PRD(x)	((x) << 7)

/* SSCR5 bits */
#define SSCR5_FRM_ASRT_CLOCKS(x)	(((x) - 1) << 1)
#define SSCR5_FRM_POLARITY(x)	SET_BIT(0, x)

/* SFIFOTT bits */
#define SFIFOTT_TX(x)		((x) - 1)
#define SFIFOTT_RX(x)		(((x) - 1) << 16)

/* SFIFOL bits */
#define SFIFOL_TFL(x)		((x) & 0xFFFF)
#define SFIFOL_RFL(x)		((x) >> 16)

#if CONFIG_CAVS || CONFIG_HASWELL || CONFIG_BROADWELL
#define SSTSA_TSEN			BIT(8)
#define SSRSA_RSEN			BIT(8)

#define SSCR3_TFL_MASK	MASK(5, 0)
#define SSCR3_RFL_MASK	MASK(13, 8)
#define SSCR3_TFL_VAL(scr3_val)	(((scr3_val) >> 0) & MASK(5, 0))
#define SSCR3_RFL_VAL(scr3_val)	(((scr3_val) >> 8) & MASK(5, 0))
#define SSCR3_TX(x)	SET_BITS(21, 16, (x) - 1)
#define SSCR3_RX(x)	SET_BITS(29, 24, (x) - 1)

#define SSIOC_TXDPDEB	BIT(1)
#define SSIOC_SFCR	BIT(4)
#define SSIOC_SCOE	BIT(5)
#endif

#if CONFIG_CAVS

#include <sof/lib/clk.h>

/* max possible index in ssp_freq array */
#define MAX_SSP_FREQ_INDEX	(NUM_SSP_FREQ - 1)

#endif

/* For 8000 Hz rate one sample is transmitted within 125us */
#define SSP_MAX_SEND_TIME_PER_SAMPLE 125

/* SSP flush retry counts maximum */
#define SSP_RX_FLUSH_RETRY_MAX	16

#define ssp_irq(ssp) \
	ssp->plat_data.irq

/* SSP private data */
struct ssp_pdata {
	uint32_t sscr0;
	uint32_t sscr1;
	uint32_t psp;
	uint32_t state[2];		/* SSP_STATE_ for each direction */
	struct sof_ipc_dai_config config;
	struct sof_ipc_dai_ssp_params params;
};

static inline void ssp_write(struct dai *dai, uint32_t reg, uint32_t value)
{
	dai_write(dai, reg, value);
}

static inline uint32_t ssp_read(struct dai *dai, uint32_t reg)
{
	return dai_read(dai, reg);
}

static inline void ssp_update_bits(struct dai *dai, uint32_t reg, uint32_t mask,
	uint32_t value)
{
	dai_update_bits(dai, reg, mask, value);
}

#endif /* __SOF_DRIVERS_SSP_H__ */
