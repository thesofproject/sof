/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 AMD.All rights reserved.
 *
 * Author:      Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *              Bala Kishore <balakishore.pati@amd.com>
 */
#ifdef __SOF_LIB_DMA_H__

#ifndef __PLATFORM_LIB_DMA_H__
#define __PLATFORM_LIB_DMA_H__

#define PLATFORM_NUM_DMACS	4

/* max number of supported DMA channels */
#define PLATFORM_MAX_DMA_CHAN	8

#define DMA_ID_DMA0		0
#define DMA_ID_HOST		1
#define DMA_ID_DAI		2
#define DMA_ID_DAI_SP		3
#define DMA_ID_DAI_DMIC		4
#define DMA_ID_DAI_HS		5

#define dma_chan_irq(dma, chan) dma_irq(dma)

#define dma_chan_irq_name(dma, chan) dma_irq_name(dma)
#endif /* __PLATFORM_LIB_DMA_H__ */
#else
#error "This file shouldn't be included from outside of sof/lib/dma.h"
#endif /* __SOF_LIB_DMA_H__ */
