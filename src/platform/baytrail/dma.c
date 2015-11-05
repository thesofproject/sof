/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/dma.h>
#include <reef/dw-dma.h>
#include <platform/memory.h>
#include <platform/interrupt.h>
#include <platform/dma.h>
#include <stdint.h>
#include <string.h>

static struct dma dma[] = {
{
	.plat_data = {
		.base		= DMA0_BASE,
		.irq		= IRQ_NUM_EXT_DMAC0,
	},
	.ops		= &dw_dma_ops1,
},
{
	.plat_data = {
		.base		= DMA1_BASE,
		.irq		= IRQ_NUM_EXT_DMAC1,
	},
	.ops		= &dw_dma_ops1,
},};

struct dma *dmac_get(int dmac_id)
{
	switch (dmac_id) {
	case DMA_ID_DMAC0:
		return &dma[0];
	case DMA_ID_DMAC1:
		return &dma[1];
	default:
		return NULL;
	}
}
