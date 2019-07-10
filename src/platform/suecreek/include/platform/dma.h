/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifndef __PLATFORM_DMA_H__
#define __PLATFORM_DMA_H__

#include <sof/dma.h>

/* available DMACs */
#define DMA_GP_LP_DMAC0		0
#define DMA_GP_LP_DMAC1		1
#define DMA_GP_LP_DMAC2		2


/* mappings - TODO improve API to get type */
#define DMA_ID_DMAC0	DMA_GP_LP_DMAC0
#define DMA_ID_DMAC1	DMA_GP_LP_DMAC1
#define DMA_ID_DMAC2	DMA_GP_LP_DMAC1

/* handshakes */
#define DMA_HANDSHAKE_DMIC_CH0	0
#define DMA_HANDSHAKE_DMIC_CH1	1
#define DMA_HANDSHAKE_SSP0_RX	2
#define DMA_HANDSHAKE_SSP0_TX	3
#define DMA_HANDSHAKE_SSP1_RX	4
#define DMA_HANDSHAKE_SSP1_TX	5
#define DMA_HANDSHAKE_SSP2_RX	6
#define DMA_HANDSHAKE_SSP2_TX	7
#define DMA_HANDSHAKE_SSP3_RX	8
#define DMA_HANDSHAKE_SSP3_TX	9
#define DMA_HANDSHAKE_SSI_TX	26
#define DMA_HANDSHAKE_SSI_RX	27

#define dma_chan_irq(dma, cpu, chan) dma_irq(dma, cpu)

int dmac_init(void);

#endif /* __PLATFORM_DMA_H__ */
