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

#include <reef/dai.h>
#include <reef/io.h>
#include <reef/lock.h>
#include <reef/work.h>
#include <reef/trace.h>
#include <reef/wait.h>

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
#define SFIFOTT		0x6C
#define SSCR3		0x70
#define SSCR4		0x74
#define SSCR5		0x78

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
#define SSCR0_TUM	(1 << 23)
#define SSCR0_FRDC(x)	((x - 1) << 24)
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

/* SSCR3 bits */
#define SSCR3_I2S_FRM_MST	(1 << 0)
#define SSCR3_I2S_ENA		(1 << 1)
#define SSCR3_I2S_FRM_POL	(1 << 2)
#define SSCR3_I2S_TX_ENA	(1 << 9)
#define SSCR3_I2S_RX_ENA	(1 << 10)
#define SSCR3_I2S_CLK_MST	(1 << 16)

/* SSCR4 bits */
#define SSCR4_FRM_CLOCKS(x)	(x << 7)

/* SSCR5 bits */
#define SSCR5_FRM_ASRT_CLOCKS(x)	((x - 1) << 1)

/* SFIFOTT bits */
#define SFIFOTT_TX(x)		(x - 1)
#define SFIFOTT_RX(x)		((x - 1) << 16)

/* SSP port status */
#define SSP_STATE_INIT			0
#define SSP_STATE_RUNNING		1
#define SSP_STATE_IDLE			2
#define SSP_STATE_DRAINING		3
#define SSP_STATE_PAUSING		4
#define SSP_STATE_PAUSED		5

/* tracing */
#define trace_ssp(__e)	trace_event(TRACE_CLASS_SSP, __e)
#define trace_ssp_error(__e)	trace_error(TRACE_CLASS_SSP, __e)
#define tracev_ssp(__e)	tracev_event(TRACE_CLASS_SSP, __e)

/* SSP private data */
struct ssp_pdata {
	uint32_t sscr0;
	uint32_t sscr1;
	uint32_t psp;
	struct work work;
	spinlock_t lock;
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
