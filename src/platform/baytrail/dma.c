/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/dma.h>
#include <reef/byt-dma.h>
#include <platform/memory.h>
#include <platform/dma.h>
#include <stdint.h>
#include <string.h>

static struct dma dma[2] = {
{
	.base		= DMA0_BASE,
	.ops		= &byt_dma_ops,
},
{
	.base		= DMA1_BASE,
	.ops		= &byt_dma_ops,
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
