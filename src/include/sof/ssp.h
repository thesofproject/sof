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
 */

#ifndef __INCLUDE_SSP__
#define __INCLUDE_SSP__

#include <sof/dai.h>
#include <sof/io.h>
#include <sof/lock.h>
#include <sof/schedule.h>
#include <sof/trace.h>
#include <sof/wait.h>

#define SSP_CLK_AUDIO	0
#define SSP_CLK_NET_PLL	1
#define SSP_CLK_EXT	2
#define SSP_CLK_NET	3
#define SSP_CLK_DEFAULT        4

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

extern const struct dai_ops ssp_ops;

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
#define SSCR0_SCR_MASK (0x000fff00)
#define SSCR0_SCR(x)	((x) << 8)
#define SSCR0_EDSS	(1 << 20)
#define SSCR0_NCS	(1 << 21)
#define SSCR0_RIM	(1 << 22)
#define SSCR0_TIM	(1 << 23)
#define SSCR0_FRDC(x)	(((x) - 1) << 24)
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
#define SSCR1_TFT(x) (((x) - 1) << 6)
#define SSCR1_RFT_MASK	(0x00003c00)
#define SSCR1_RFT(x) (((x) - 1) << 10)
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
#define SSPSP_SCMODE(x)		((x) << 0)
#define SSPSP_SFRMP(x)		((x) << 2)
#define SSPSP_ETDS		(1 << 3)
#define SSPSP_STRTDLY(x)	((x) << 4)
#define SSPSP_DMYSTRT(x)	((x) << 7)
#define SSPSP_SFRMDLY(x)	((x) << 9)
#define SSPSP_SFRMWDTH(x)	((x) << 16)
#define SSPSP_DMYSTOP(x)	((x) << 23)
#define SSPSP_DMYSTOP_BITS	2
#define SSPSP_DMYSTOP_MASK	((0x1 << SSPSP_DMYSTOP_BITS) - 1)
#define SSPSP_FSRT		(1 << 25)
#define SSPSP_EDMYSTOP(x)	((x) << 26)
#define SSPSP_EDMYSTOP_MASK	0x7

#define SSPSP2			0x44
#define SSPSP2_FEP_MASK		0xff

#if CONFIG_CAVS
#define SSCR3		0x48
#define SSIOC		0x4C

#define SSP_REG_MAX	SSIOC
#endif

/* SSCR3 bits */
#define SSCR3_FRM_MST_EN	(1 << 0)
#define SSCR3_I2S_MODE_EN	(1 << 1)
#define SSCR3_I2S_FRM_POL(x)	((x) << 2)
#define SSCR3_I2S_TX_SS_FIX_EN	(1 << 3)
#define SSCR3_I2S_RX_SS_FIX_EN	(1 << 4)
#define SSCR3_I2S_TX_EN		(1 << 9)
#define SSCR3_I2S_RX_EN		(1 << 10)
#define SSCR3_CLK_EDGE_SEL	(1 << 12)
#define SSCR3_STRETCH_TX	(1 << 14)
#define SSCR3_STRETCH_RX	(1 << 15)
#define SSCR3_MST_CLK_EN	(1 << 16)
#define SSCR3_SYN_FIX_EN	(1 << 17)


/* SSCR4 bits */
#define SSCR4_TOT_FRM_PRD(x)	((x) << 7)

/* SSCR5 bits */
#define SSCR5_FRM_ASRT_CLOCKS(x)	(((x) - 1) << 1)
#define SSCR5_FRM_POLARITY(x)	((x) << 0)

/* SFIFOTT bits */
#define SFIFOTT_TX(x)		((x) - 1)
#define SFIFOTT_RX(x)		(((x) - 1) << 16)

/* SFIFOL bits */
#define SFIFOL_TFL(x)		((x) & 0xFFFF)
#define SFIFOL_RFL(x)		((x) >> 16)

#if CONFIG_CAVS || CONFIG_HASWELL || CONFIG_BROADWELL
#define SSTSA_TSEN			BIT(8)
#define SSRSA_RSEN			BIT(8)

#define SSCR3_TFL_MASK	(0x0000003f)
#define SSCR3_RFL_MASK	(0x00003f00)
#define SSCR3_TFT_MASK	(0x003f0000)
#define SSCR3_TX(x) (((x) - 1) << 16)
#define SSCR3_RFT_MASK	(0x3f000000)
#define SSCR3_RX(x) (((x) - 1) << 24)

#define SSIOC_TXDPDEB	BIT(1)
#define SSIOC_SFCR	BIT(4)
#define SSIOC_SCOE	BIT(5)
#endif

#if CONFIG_CAVS
#define MNDSS(x)	((x) << 20)
#define MCDSS(x)	((x) << 16)
#endif

/* tracing */
#define trace_ssp(__e, ...)	trace_event(TRACE_CLASS_SSP, __e, ##__VA_ARGS__)
#define trace_ssp_error(__e, ...)	trace_error(TRACE_CLASS_SSP, __e, ##__VA_ARGS__)
#define tracev_ssp(__e, ...)	tracev_event(TRACE_CLASS_SSP, __e, ##__VA_ARGS__)


#define ssp_irq(ssp) \
	ssp->plat_data.irq

/* SSP private data */
struct ssp_pdata {
	uint32_t sscr0;
	uint32_t sscr1;
	uint32_t psp;
	uint32_t state[2];		/* SSP_STATE_ for each direction */
	completion_t drain_complete;
	struct sof_ipc_dai_config config;
	struct sof_ipc_dai_ssp_params params;
};

static inline void ssp_write(struct dai *dai, uint32_t reg, uint32_t value)
{
	io_reg_write(dai_base(dai) + reg, value);
}

static inline uint32_t ssp_read(struct dai *dai, uint32_t reg)
{
	return io_reg_read(dai_base(dai) + reg);
}

static inline void ssp_update_bits(struct dai *dai, uint32_t reg, uint32_t mask,
	uint32_t value)
{
	io_reg_update_bits(dai_base(dai) + reg, mask, value);
}


#endif
