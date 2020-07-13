/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_LIB_DMA_H__

#ifndef __PLATFORM_LIB_DMA_H__
#define __PLATFORM_LIB_DMA_H__

#define PLATFORM_NUM_DMACS	2

/* max number of supported DMA channels */
#define PLATFORM_MAX_DMA_CHAN	32

#define DMA_ID_SDMA2	0
#define DMA_ID_HOST	1
#define DMA_ID_SDMA3	2

#define dma_chan_irq(dma, chan) dma_irq(dma)
#define dma_chan_irq_name(dma, chan) dma_irq_name(dma)

/* SDMA2 specific data */

/* Interrupts must be set up interestingly -- shift them all by 32 like
 * on the other platforms.
 *
 * We want interrupt 103 + 32. To properly get it from IRQ_STEER we have
 * to divide this by 64 (gives result 2 and remainder 7) due to how the
 * IRQ_STEER driver interacts with SOF.
 */
#define SDMA2_IRQ	7
#define SDMA2_IRQ_NAME	"irqsteer2"

/* SDMA3 specific data */

/* Hardware interrupt at the input of irqsteer for SDMA3_IRQ is 34. In
 * order to map it inside the IRQSTEER we must add 32. So inside
 * irqsteer SDMA2 interrupt will be 32 + 34 = 66. Next is to map it
 * to an irqsteer child 66 % 64 = 1 and reminder 2. This means the
 * interrupt is mapped to irqsteer1 and has the index 2.
 */
#define SDMA3_IRQ	2
#define SDMA3_IRQ_NAME	"irqsteer1"

#define SDMA_CORE_RATIO 1/* Enable ACR bit as it's needed for this platform */

#endif /* __PLATFORM_LIB_DMA_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/dma.h"

#endif /* __SOF_LIB_DMA_H__ */
