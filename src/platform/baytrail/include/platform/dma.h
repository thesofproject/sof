/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifdef __SOF_DMA_H__

#ifndef __PLATFORM_DMA_H__
#define __PLATFORM_DMA_H__

#include <stdint.h>

#if defined CONFIG_CHERRYTRAIL
#define PLATFORM_NUM_DMACS	3
#else
#define PLATFORM_NUM_DMACS	2
#endif

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

#define dma_chan_irq(dma, cpu, chan) dma_irq(dma, cpu)

int dmac_init(void);

#endif /* __PLATFORM_DMA_H__ */

#else

#error "This file shouldn't be included from outside of sof/dma.h"

#endif /* __SOF_DMA_H__ */
