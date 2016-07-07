/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __PLATFORM_DMA_H__
#define __PLATFORM_DMA_H__

#include <stdint.h>

/* available DMACs */
#define DMA_GP_LP_DMAC0		0
#define DMA_GP_LP_DMAC1		1
#define DMA_GP_HP_DMAC0		2
#define DMA_GP_HP_DMAC1		3
#define DMA_HOST_IN_DMAC	4
#define DMA_HOST_OUT_DMAC	5
#define DMA_LINK_IN_DMAC	6
#define DMA_LINK_OUT_DMAC	7

/* mappings - TODO improve API to get type */
#define DMA_ID_DMAC0	DMA_HOST_IN_DMAC
#define DMA_ID_DMAC1	DMA_GP_LP_DMAC0
#define DMA_ID_DMAC2	DMA_HOST_OUT_DMAC
#define DMA_ID_DMAC3	DMA_GP_HP_DMAC0
#define DMA_ID_DMAC4	DMA_GP_LP_DMAC1
#define DMA_ID_DMAC5	DMA_GP_HP_DMAC1
#define DMA_ID_DMAC6	DMA_LINK_IN_DMAC
#define DMA_ID_DMAC7	DMA_LINK_OUT_DMAC

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

#endif
