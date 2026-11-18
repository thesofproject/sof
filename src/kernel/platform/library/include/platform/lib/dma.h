/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Curtis Malainey <cujomalainey@chromium.org>
 */

#ifdef __SOF_LIB_DMA_H__

#ifndef __PLATFORM_LIB_DMA_H__
#define __PLATFORM_LIB_DMA_H__

#define DMA_ID_DMAC0	0
#define DMA_ID_DMAC1	1

#define DMA_DEV_PCM			0
#define DMA_DEV_WAV			1

#define PLATFORM_NUM_DMACS		1
#define PLATFORM_MAX_DMA_CHAN		1

#define dma_chan_irq(dma, chan)		0
#define dma_chan_irq_name(dma, chan)	"chan0-irq"

#endif /* __PLATFORM_LIB_DMA_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/dma.h"

#endif /* __SOF_LIB_DMA_H__ */
