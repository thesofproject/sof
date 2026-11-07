/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifdef __SOF_LIB_DMA_H__

#ifndef __PLATFORM_LIB_DMA_H__
#define __PLATFORM_LIB_DMA_H__

/* i.MX93 uses dummy DMA and EDMA */
#define PLATFORM_NUM_DMACS 2

#define DMA_ID_EDMA2 0
#define DMA_ID_HOST 1

/* TODO: this is already defined with EDMA2_CHAN_MAX */
#define PLATFORM_MAX_DMA_CHAN 64

/* TODO: required by Zephyr DMA domain to work */
#define dma_chan_irq_name(dma, chan) dma_irq_name(dma)
#define dma_chan_irq(dma, chan) ((int *)(dma)->plat_data.drv_plat_data)[chan]

#endif /* __PLATFORM_LIB_DMA_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/dma.h"

#endif /* __SOF_LIB_DMA_H__ */
