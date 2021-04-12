/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __SOF_DRIVERS_DW_DMA_H__

#ifndef __PLATFORM_DRIVERS_DW_DMA_H__
#define __PLATFORM_DRIVERS_DW_DMA_H__

#include <sof/bit.h>

#include <stdint.h>

struct dma;
struct dma_chan_data;

/* number of supported DW-DMACs */
#if CONFIG_CHERRYTRAIL_EXTRA_DW_DMA
#define PLATFORM_NUM_DW_DMACS	3
#else
#define PLATFORM_NUM_DW_DMACS	2
#endif

/* index of the first DW-DMAC in the array */
#define PLATFORM_DW_DMA_INDEX	0

/* DMA treats PHY addresses as host address unless within DSP region */
#define PLATFORM_DW_DMA_HOST_MASK	0xFF000000

/* CTL_HI */
#define DW_CTLH_CLASS(x)	SET_BITS(31, 29, x)
#define DW_CTLH_WEIGHT(x)	SET_BITS(28, 18, x)
#define DW_CTLH_DONE(x)		SET_BIT(17, x)
#define DW_CTLH_BLOCK_TS_MASK	MASK(16, 0)

/* CFG_HI */
#define DW_CFGH_DST_PER(x)	SET_BITS(7, 4, x)
#define DW_CFGH_SRC_PER(x)	SET_BITS(3, 0, x)
#define DW_CFGH_DST(x)		DW_CFGH_DST_PER(x)
#define DW_CFGH_SRC(x)		DW_CFGH_SRC_PER(x)

/* default initial setup register values */
#define DW_CFG_LOW_DEF		0x3
#define DW_CFG_HIGH_DEF		0x0

#define platform_dw_dma_set_class(chan, lli, class) \
	(lli->ctrl_hi |= DW_CTLH_CLASS(class))

#define platform_dw_dma_set_transfer_size(chan, lli, size) \
	(lli->ctrl_hi |= (size & DW_CTLH_BLOCK_TS_MASK))

static inline void platform_dw_dma_llp_config(struct dma *dma,
					      struct dma_chan_data *chan,
					      uint32_t config) { }

static inline void platform_dw_dma_llp_enable(struct dma *dma,
					      struct dma_chan_data *chan) { }

static inline void platform_dw_dma_llp_disable(struct dma *dma,
					       struct dma_chan_data *chan) { }

#endif /* __PLATFORM_DRIVERS_DW_DMA_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/dw-dma.h"

#endif /* __SOF_DRIVERS_DW_DMA_H__ */
