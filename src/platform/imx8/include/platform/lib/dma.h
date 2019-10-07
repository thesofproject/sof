/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_LIB_DMA_H__

#ifndef __PLATFORM_LIB_DMA_H__
#define __PLATFORM_LIB_DMA_H__

#define PLATFORM_NUM_DMACS	2

/* max number of supported DMA channels */
#define PLATFORM_MAX_DMA_CHAN	32

#define DMA_ID_EDMA0	0
#define DMA_ID_HOST	1

/*
 * IRQ line 442 (ESAI0_DMA_INT) groups
 *	- DMA#0 interrupt #6, ESAI0 receive channel
 *	- DMA#0 interrupt #7, ESAI0 transmit channel
 * Converting this to internal SOF irq representation we get
 *	- irq = 442 % 64 = 58
 *	- irq_name = irq_name_irqsteer[442 / 64] = "irqsteer6"
 *
 * TODO: Remove hardcoded values and add generic implementation
 */
#define dma_chan_irq(dma, chan) 58
#define dma_chan_irq_name(dma, chan) "irqsteer6"

int dmac_init(void);

#endif /* __PLATFORM_LIB_DMA_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/dma.h"

#endif /* __SOF_LIB_DMA_H__ */
