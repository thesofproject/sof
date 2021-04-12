/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __PLATFORM_DRIVERS_INTERRUPT_H__
#define __PLATFORM_DRIVERS_INTERRUPT_H__

#include <cavs/drivers/interrupt.h>
#include <sof/bit.h>

#define PLATFORM_IRQ_HW_NUM	XCHAL_NUM_INTERRUPTS
#define PLATFORM_IRQ_FIRST_CHILD  PLATFORM_IRQ_HW_NUM
#define PLATFORM_IRQ_CHILDREN	32

/* IRQ numbers - wrt Tensilica DSP */
#if CONFIG_INTERRUPT_LEVEL_1

#define IRQ_NUM_SOFTWARE0	0	/* level 1 */
#define IRQ_NUM_TIMER1		1	/* level 1 */
#define IRQ_NUM_EXT_LEVEL1	2	/* level 1 */
#define IRQ_NUM_SOFTWARE1	3	/* level 1 */

#define IRQ_MASK_SOFTWARE0	BIT(IRQ_NUM_SOFTWARE0)
#define IRQ_MASK_TIMER1		BIT(IRQ_NUM_TIMER1)
#define IRQ_MASK_EXT_LEVEL1	BIT(IRQ_NUM_EXT_LEVEL1)
#define IRQ_MASK_SOFTWARE1	BIT(IRQ_NUM_SOFTWARE1)

#endif

#if CONFIG_INTERRUPT_LEVEL_2

#define IRQ_NUM_SOFTWARE2	4	/* level 2 */
#define IRQ_NUM_TIMER2		5	/* level 2 */
#define IRQ_NUM_EXT_LEVEL2	6	/* level 2 */
#define IRQ_NUM_SOFTWARE3	7	/* level 2 */

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

/* Priority 2 Peripheral IRQ mappings */
#define IRQ_EXT_HP_GPDMA_LVL2 IRQ_BIT_LVL2_HP_GP_DMA0(0)
#define IRQ_EXT_IDC_LVL2 IRQ_BIT_LVL2_IDC
#define IRQ_EXT_IPC_LVL2 IRQ_BIT_LVL2_HOST_IPC
#define IRQ_EXT_TSTAMP1_LVL2 IRQ_BIT_LVL2_WALL_CLK1
#define IRQ_EXT_TSTAMP0_LVL2 IRQ_BIT_LVL2_WALL_CLK0
#define IRQ_EXT_MERR_LVL2 IRQ_BIT_LVL2_L2_MEMERR
#define IRQ_EXT_L2CACHE_LVL2 IRQ_BIT_LVL2_L2_CACHE
#define IRQ_EXT_SHA256_LVL2 IRQ_BIT_LVL2_SHA256

#define IRQ_MASK_SOFTWARE2	BIT(IRQ_NUM_SOFTWARE2)
#define IRQ_MASK_TIMER2		BIT(IRQ_NUM_TIMER2)
#define IRQ_MASK_EXT_LEVEL2	BIT(IRQ_NUM_EXT_LEVEL2)
#define IRQ_MASK_SOFTWARE3	BIT(IRQ_NUM_SOFTWARE3)

#endif

#if CONFIG_INTERRUPT_LEVEL_3

#define IRQ_NUM_SOFTWARE4	8	/* level 3 */
#define IRQ_NUM_TIMER3		9	/* level 3 */
#define IRQ_NUM_EXT_LEVEL3	10	/* level 3 */
#define IRQ_NUM_SOFTWARE5	11	/* level 3 */

/* IRQ Level 3 bits */
#define IRQ_BIT_LVL3_CODE_LOADER	31
#define IRQ_BIT_LVL3_HOST_STREAM_OUT(x)	(16 + x)
#define IRQ_BIT_LVL3_HOST_STREAM_IN(x)	(0 + x)

/* Priority 3 Peripheral IRQ mappings */
#define IRQ_EXT_CODE_DMA_LVL3 IRQ_BIT_LVL3_CODE_LOADER
#define IRQ_EXT_HOST_DMA_IN_LVL3(channel) IRQ_BIT_LVL3_HOST_STREAM_IN(channel)
#define IRQ_EXT_HOST_DMA_OUT_LVL3(channel) IRQ_BIT_LVL3_HOST_STREAM_OUT(channel)

#define IRQ_MASK_SOFTWARE4	BIT(IRQ_NUM_SOFTWARE4)
#define IRQ_MASK_TIMER3		BIT(IRQ_NUM_TIMER3)
#define IRQ_MASK_EXT_LEVEL3	BIT(IRQ_NUM_EXT_LEVEL3)
#define IRQ_MASK_SOFTWARE5	BIT(IRQ_NUM_SOFTWARE5)

#endif

#if CONFIG_INTERRUPT_LEVEL_4

#define IRQ_NUM_SOFTWARE6	12	/* level 4 */
#define IRQ_NUM_EXT_LEVEL4	13	/* level 4 */
#define IRQ_NUM_SOFTWARE7	14	/* level 4 */

/* IRQ Level 4 bits */
#define IRQ_BIT_LVL4_LINK_STREAM_OUT(x)	(16 + x)
#define IRQ_BIT_LVL4_LINK_STREAM_IN(x)	(0 + x)

/* Priority 4 Peripheral IRQ mappings */
#define IRQ_EXT_LINK_DMA_IN_LVL4(channel) IRQ_BIT_LVL4_LINK_STREAM_IN(channel)
#define IRQ_EXT_LINK_DMA_OUT_LVL4(channel) IRQ_BIT_LVL4_LINK_STREAM_OUT(channel)

#define IRQ_MASK_SOFTWARE6	BIT(IRQ_NUM_SOFTWARE6)
#define IRQ_MASK_EXT_LEVEL4	BIT(IRQ_NUM_EXT_LEVEL4)
#define IRQ_MASK_SOFTWARE7	BIT(IRQ_NUM_SOFTWARE7)

#endif

#if CONFIG_INTERRUPT_LEVEL_5

#define IRQ_NUM_SOFTWARE8	15	/* level 5 */
#define IRQ_NUM_EXT_LEVEL5	16	/* level 5 */
#define IRQ_NUM_EXT_LEVEL6	17	/* level 5 */
#define IRQ_NUM_EXT_LEVEL7	18	/* level 5 */
#define IRQ_NUM_SOFTWARE9	19	/* level 5 */

/* IRQ Level 5 bits */
#define IRQ_BIT_LVL5_LP_GP_DMA1	15
#define IRQ_BIT_LVL5_LP_GP_DMA0	16
#define IRQ_BIT_LVL5_DMIC(x)		8
#define IRQ_BIT_LVL5_SSP(x)		(0 + x)

/* Priority 5 Peripheral IRQ mappings */
#define IRQ_EXT_LP_GPDMA0_LVL5(channel) IRQ_BIT_LVL5_LP_GP_DMA0
#define IRQ_EXT_LP_GPDMA1_LVL5(channel) IRQ_BIT_LVL5_LP_GP_DMA0
#define IRQ_EXT_SSPx_LVL5(x) IRQ_BIT_LVL5_SSP(x)
#define IRQ_EXT_DMIC_LVL5(x) IRQ_BIT_LVL5_DMIC(x)

#define IRQ_MASK_SOFTWARE8	BIT(IRQ_NUM_SOFTWARE8)
#define IRQ_MASK_EXT_LEVEL5	BIT(IRQ_NUM_EXT_LEVEL5)
#define IRQ_MASK_EXT_LEVEL6	BIT(IRQ_NUM_EXT_LEVEL6)
#define IRQ_MASK_EXT_LEVEL7	BIT(IRQ_NUM_EXT_LEVEL7)
#define IRQ_MASK_SOFTWARE9	BIT(IRQ_NUM_SOFTWARE9)

#endif

#define IRQ_NUM_NMI		20	/* level 7 */

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */
