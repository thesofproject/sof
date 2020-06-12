/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifdef __SOF_LIB_DMA_H__

#ifndef __PLATFORM_LIB_DMA_H__
#define __PLATFORM_LIB_DMA_H__



#if defined CONFIG_CHERRYTRAIL_EXTRA_DW_DMA
#define PLATFORM_NUM_DMACS	3
#else
#define PLATFORM_NUM_DMACS	2
#endif

/* max number of supported DMA channels */
#define PLATFORM_MAX_DMA_CHAN	8

#define DMA_ID_DMAC0	0
#define DMA_ID_DMAC1	1
#define DMA_ID_DMAC2	2

#define DMA_HANDSHAKE_SSP0_RX	0
#define DMA_HANDSHAKE_SSP0_TX	1
#define DMA_HANDSHAKE_SSP1_RX	2
#define DMA_HANDSHAKE_SSP1_TX	3
#define DMA_HANDSHAKE_SSP2_RX	4
#define DMA_HANDSHAKE_SSP2_TX	5
#define DMA_HANDSHAKE_SSP3_RX	6
#define DMA_HANDSHAKE_SSP3_TX	7
#define DMA_HANDSHAKE_SSP4_RX	8
#define DMA_HANDSHAKE_SSP4_TX	9
#define DMA_HANDSHAKE_SSP5_RX	10
#define DMA_HANDSHAKE_SSP5_TX	11
#define DMA_HANDSHAKE_SSP6_RX	12
#define DMA_HANDSHAKE_SSP6_TX	13

#define dma_chan_irq(dma, chan) dma_irq(dma)
#define dma_chan_irq_name(dma, chan) dma_irq_name(dma)

#endif /* __PLATFORM_LIB_DMA_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/dma.h"

#endif /* __SOF_LIB_DMA_H__ */
