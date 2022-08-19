/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __PLATFORM_DRIVERS_DW_DMA_H__

#ifndef __CAVS_LIB_DW_DMA_H__
#define __CAVS_LIB_DW_DMA_H__

#include <rtos/bit.h>
#include <sof/lib/dma.h>
#include <sof/lib/shim.h>

#include <stdint.h>

/* number of supported DW-DMACs */
#if CONFIG_SUECREEK
#define PLATFORM_NUM_DW_DMACS	3
#else
#define PLATFORM_NUM_DW_DMACS	2
#endif

/* index of the first DW-DMAC in the array */
#define PLATFORM_DW_DMA_INDEX	0

/* DMA treats PHY addresses as host address unless within DSP region */
#define PLATFORM_DW_DMA_HOST_MASK	0x00000000

/* CTL_HI */
#define DW_CTLH_CLASS(x)	SET_BITS(31, 29, x)
#define DW_CTLH_WEIGHT(x)	SET_BITS(28, 18, x)
#define DW_CTLH_DONE(x)		SET_BIT(17, x)
#define DW_CTLH_BLOCK_TS_MASK	MASK(16, 0)

/* CFG_LO */
#define DW_CFG_RELOAD_DST	BIT(31)
#define DW_CFG_RELOAD_SRC	BIT(30)
#define DW_CFG_CTL_HI_UPD_EN	BIT(5)

/* CFG_HI */
#define DW_CFGH_DST_PER_EXT(x)		SET_BITS(31, 30, x)
#define DW_CFGH_SRC_PER_EXT(x)		SET_BITS(29, 28, x)
#define DW_CFGH_DST_PER(x)		SET_BITS(7, 4, x)
#define DW_CFGH_SRC_PER(x)		SET_BITS(3, 0, x)
#define DW_CFGH_DST(x) \
	(DW_CFGH_DST_PER_EXT((x) >> 4) | DW_CFGH_DST_PER(x))
#define DW_CFGH_SRC(x) \
	(DW_CFGH_SRC_PER_EXT((x) >> 4) | DW_CFGH_SRC_PER(x))

/* default initial setup register values */
#define DW_CFG_LOW_DEF		0x3
#define DW_CFG_HIGH_DEF		0x0

/* LLPC address */
#define DW_CHLLPC(dma, chan) \
	SHIM_GPDMA_CHLLPC((dma)->plat_data.id, (chan)->index)

#define DW_CHLLPL(dma, chan) \
	SHIM_GPDMA_CHLLPL((dma)->plat_data.id, (chan)->index)

#define DW_CHLLPU(dma, chan) \
	SHIM_GPDMA_CHLLPU((dma)->plat_data.id, (chan)->index)

#define platform_dw_dma_set_class(chan, lli, class) \
	(lli->ctrl_hi |= DW_CTLH_CLASS(class))

#define platform_dw_dma_set_transfer_size(chan, lli, size) \
	(lli->ctrl_hi |= (size & DW_CTLH_BLOCK_TS_MASK))

static inline void platform_dw_dma_llp_config(struct dma *dma,
					      struct dma_chan_data *chan,
					      uint32_t config)
{
	shim_write(DW_CHLLPC(dma, chan), SHIM_GPDMA_CHLLPC_DHRS(config));
}

static inline void platform_dw_dma_llp_enable(struct dma *dma,
					      struct dma_chan_data *chan)
{
	uint32_t val;

	val = shim_read(DW_CHLLPC(dma, chan));
	if (!(val & SHIM_GPDMA_CHLLPC_EN))
		shim_write(DW_CHLLPC(dma, chan), val | SHIM_GPDMA_CHLLPC_EN);
}

static inline void platform_dw_dma_llp_disable(struct dma *dma,
					       struct dma_chan_data *chan)
{
	shim_write(DW_CHLLPC(dma, chan),
		   shim_read(DW_CHLLPC(dma, chan)) & ~SHIM_GPDMA_CHLLPC_EN);
}

static inline void platform_dw_dma_llp_read(struct dma *dma,
					    struct dma_chan_data *chan,
					    uint32_t *llp_l,
					    uint32_t *llp_u)
{
	*llp_l = shim_read(DW_CHLLPL(dma, chan));
	*llp_u = shim_read(DW_CHLLPU(dma, chan));
}

static inline struct dw_lli *platform_dw_dma_lli_get(struct dw_lli *lli)
{
	return cache_to_uncache(lli);
}

#endif /* __CAVS_LIB_DW_DMA_H__ */

#else

#error "This file shouldn't be included from outside of platform/drivers/dw-dma.h"

#endif /* __PLATFORM_DRIVERS_DW_DMA_H__ */
