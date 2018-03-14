/*
 * Copyright (c) 2017, Intel Corporation
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
 *         Rander Wang <rander.wang@intel.com>
 */

#ifndef __INCLUDE_PLATFORM_INTERRUPT__
#define __INCLUDE_PLATFORM_INTERRUPT__

#include <stdint.h>
#include <reef/interrupt-map.h>

#define PLATFORM_IRQ_CHILDREN	32

/* IRQ numbers - wrt Tensilica DSP */
#define IRQ_NUM_SOFTWARE0	0	/* level 1 */
#define IRQ_NUM_TIMER1		1	/* level 1 */
#define IRQ_NUM_EXT_LEVEL1	2	/* level 1 */
#define IRQ_NUM_SOFTWARE1	3	/* level 1 */
#define IRQ_NUM_SOFTWARE2	4	/* level 2 */
#define IRQ_NUM_TIMER2		5	/* level 2 */
#define IRQ_NUM_EXT_LEVEL2	6	/* level 2 */
#define IRQ_NUM_SOFTWARE3	7	/* level 2 */
#define IRQ_NUM_SOFTWARE4	8	/* level 3 */
#define IRQ_NUM_TIMER3		9	/* level 3 */
#define IRQ_NUM_EXT_LEVEL3	10	/* level 3 */
#define IRQ_NUM_SOFTWARE5	11	/* level 3 */
#define IRQ_NUM_SOFTWARE6	12	/* level 4 */
#define IRQ_NUM_EXT_LEVEL4	13	/* level 4 */
#define IRQ_NUM_SOFTWARE7	14	/* level 4 */
#define IRQ_NUM_SOFTWARE8	15	/* level 5 */
#define IRQ_NUM_EXT_LEVEL5	16	/* level 5 */
#define IRQ_NUM_EXT_LEVEL6	17	/* level 5 */
#define IRQ_NUM_EXT_LEVEL7	18	/* level 5 */
#define IRQ_NUM_SOFTWARE9	19	/* level 5 */
#define IRQ_NUM_NMI		20	/* level 7 */

/* IRQ Level 2 bits */
#define IRQ_BIT_LVL2_HP_GP_DMA0(x)	(x + 24)
#define IRQ_BIT_LVL2_WALL_CLK1		23
#define IRQ_BIT_LVL2_WALL_CLK0		22
#define IRQ_BIT_LVL2_L2_MEMERR		21
#define IRQ_BIT_LVL2_SHA256		16
#define IRQ_BIT_LVL2_L2_CACHE		15
#define IRQ_BIT_LVL2_IDC		8
#define IRQ_BIT_LVL2_HOST_IPC		7
#define IRQ_BIT_LVL2_CSME_IPC		6
#define IRQ_BIT_LVL2_PMC_IPC		5

/* IRQ Level 3 bits */
#define IRQ_BIT_LVL3_CODE_LOADER	31
#define IRQ_BIT_LVL3_HOST_STREAM_OUT(x)	(16 + x)
#define IRQ_BIT_LVL3_HOST_STREAM_IN(x)	(0 + x)

/* IRQ Level 4 bits */
#define IRQ_BIT_LVL4_LINK_STREAM_OUT(x)	(16 + x)
#define IRQ_BIT_LVL4_LINK_STREAM_IN(x)	(0 + x)

/* IRQ Level 5 bits */
#define IRQ_BIT_LVL5_LP_GP_DMA1	15
#define IRQ_BIT_LVL5_LP_GP_DMA0	16
#define IRQ_BIT_LVL5_DMIC		6
#define IRQ_BIT_LVL5_SSP(x)		(0 + x)

/* Level 2 Peripheral IRQ mappings */
#define IRQ_EXT_HP_GPDMA_LVL2(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL2_HP_GP_DMA0(0), 2, xcpu, IRQ_NUM_EXT_LEVEL2)
#define IRQ_EXT_IDC_LVL2(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL2_IDC, 2, xcpu, IRQ_NUM_EXT_LEVEL2)
#define IRQ_EXT_IPC_LVL2(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL2_HOST_IPC, 2, xcpu, IRQ_NUM_EXT_LEVEL2)
#define IRQ_EXT_TSTAMP1_LVL2(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL2_WALL_CLK1, 2, xcpu, IRQ_NUM_EXT_LEVEL2)
#define IRQ_EXT_TSTAMP0_LVL2(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL2_WALL_CLK0, 2, xcpu, IRQ_NUM_EXT_LEVEL2)
#define IRQ_EXT_MERR_LVL2(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL2_L2_MEMERR, 2, xcpu, IRQ_NUM_EXT_LEVEL2)
#define IRQ_EXT_L2CACHE_LVL2(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL2_L2_CACHE, 2, xcpu, IRQ_NUM_EXT_LEVEL2)
#define IRQ_EXT_SHA256_LVL2(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL2_SHA256, 2, xcpu, IRQ_NUM_EXT_LEVEL2)

/* Level 3 Peripheral IRQ mappings */
#define IRQ_EXT_CODE_DMA_LVL3(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL3_CODE_LOADER, 3, xcpu, IRQ_NUM_EXT_LEVEL3)
#define IRQ_EXT_HOST_DMA_IN_LVL3(xcpu, channel) \
	REEF_IRQ(IRQ_BIT_LVL3_HOST_STREAM_IN(channel), 3, xcpu, IRQ_NUM_EXT_LEVEL3)
#define IRQ_EXT_HOST_DMA_OUT_LVL3(xcpu, channel) \
	REEF_IRQ(IRQ_BIT_LVL3_HOST_STREAM_OUT(channel), 3, xcpu, IRQ_NUM_EXT_LEVEL3)

/* Level 4 Peripheral IRQ mappings */
#define IRQ_EXT_LINK_DMA_IN_LVL4(xcpu, channel) \
	REEF_IRQ(IRQ_BIT_LVL4_LINK_STREAM_IN(channel), 4, xcpu, IRQ_NUM_EXT_LEVEL4)
#define IRQ_EXT_LINK_DMA_OUT_LVL4(xcpu, channel) \
	REEF_IRQ(IRQ_BIT_LVL4_LINK_STREAM_OUT(channel), 4, xcpu, IRQ_NUM_EXT_LEVEL4)

/* Level 5 Peripheral IRQ mappings */
#define IRQ_EXT_LP_GPDMA0_LVL5(xcpu, channel) \
	REEF_IRQ(IRQ_BIT_LVL5_LP_GP_DMA0, 5, xcpu, IRQ_NUM_EXT_LEVEL5)
#define IRQ_EXT_LP_GPDMA1_LVL4(xcpu, channel) \
	REEF_IRQ(IRQ_BIT_LVL5_LP_GP_DMA1, 4, xcpu, IRQ_NUM_EXT_LEVEL4)
#define IRQ_EXT_SSP0_LVL5(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL5_SSP(0), 5, xcpu, IRQ_NUM_EXT_LEVEL5)
#define IRQ_EXT_SSP1_LVL5(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL5_SSP(1), 5, xcpu, IRQ_NUM_EXT_LEVEL5)
#define IRQ_EXT_SSP2_LVL5(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL5_SSP(2), 5, xcpu, IRQ_NUM_EXT_LEVEL5)
#define IRQ_EXT_SSP3_LVL5(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL5_SSP(3), 5, xcpu, IRQ_NUM_EXT_LEVEL5)
#define IRQ_EXT_DMIC_LVL5(xcpu) \
	REEF_IRQ(IRQ_BIT_LVL5_DMIC, 5, xcpu, IRQ_NUM_EXT_LEVEL5)


/* IRQ Masks */
#define IRQ_MASK_SOFTWARE0	(1 << IRQ_NUM_SOFTWARE0)
#define IRQ_MASK_TIMER1		(1 << IRQ_NUM_TIMER1)
#define IRQ_MASK_EXT_LEVEL1	(1 << IRQ_NUM_EXT_LEVEL1)
#define IRQ_MASK_SOFTWARE1	(1 << IRQ_NUM_SOFTWARE1)
#define IRQ_MASK_SOFTWARE2	(1 << IRQ_NUM_SOFTWARE2)
#define IRQ_MASK_TIMER2		(1 << IRQ_NUM_TIMER2)
#define IRQ_MASK_EXT_LEVEL2	(1 << IRQ_NUM_EXT_LEVEL2)
#define IRQ_MASK_SOFTWARE3	(1 << IRQ_NUM_SOFTWARE3)
#define IRQ_MASK_SOFTWARE4	(1 << IRQ_NUM_SOFTWARE4)
#define IRQ_MASK_TIMER3		(1 << IRQ_NUM_TIMER3)
#define IRQ_MASK_EXT_LEVEL3	(1 << IRQ_NUM_EXT_LEVEL3)
#define IRQ_MASK_SOFTWARE5	(1 << IRQ_NUM_SOFTWARE5)
#define IRQ_MASK_SOFTWARE6	(1 << IRQ_NUM_SOFTWARE6)
#define IRQ_MASK_EXT_LEVEL4	(1 << IRQ_NUM_EXT_LEVEL4)
#define IRQ_MASK_SOFTWARE7	(1 << IRQ_NUM_SOFTWARE7)
#define IRQ_MASK_SOFTWARE8	(1 << IRQ_NUM_SOFTWARE8)
#define IRQ_MASK_EXT_LEVEL5	(1 << IRQ_NUM_EXT_LEVEL5)
#define IRQ_MASK_EXT_LEVEL6	(1 << IRQ_NUM_EXT_LEVEL6)
#define IRQ_MASK_EXT_LEVEL7	(1 << IRQ_NUM_EXT_LEVEL7)
#define IRQ_MASK_SOFTWARE9	(1 << IRQ_NUM_SOFTWARE9)

void platform_interrupt_init(void);

struct irq_parent *platform_irq_get_parent(uint32_t irq);
void platform_interrupt_set(int irq);
void platform_interrupt_clear(uint32_t irq, uint32_t mask);
uint32_t platform_interrupt_get_enabled(void);
void platform_interrupt_mask(uint32_t irq, uint32_t mask);
void platform_interrupt_unmask(uint32_t irq, uint32_t mask);

#endif
