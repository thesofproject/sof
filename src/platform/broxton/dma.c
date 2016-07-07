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

static struct dw_drv_plat_data dmac0 = {
	.chan[0] = {
		.class	= 6,
		.weight = 0,
	},
	.chan[1] = {
		.class	= 6,
		.weight = 0,
	},
	.chan[2] = {
		.class	= 6,
		.weight = 0,
	},
	.chan[3] = {
		.class	= 6,
		.weight = 0,
	},
	.chan[4] = {
		.class	= 6,
		.weight = 0,
	},
	.chan[5] = {
		.class	= 6,
		.weight = 0,
	},
	.chan[6] = {
		.class	= 6,
		.weight = 0,
	},
	.chan[7] = {
		.class	= 6,
		.weight = 0,
	},
};

static struct dw_drv_plat_data dmac1 = {
	.chan[0] = {
		.class	= 7,
		.weight = 0,
	},
	.chan[1] = {
		.class	= 7,
		.weight = 0,
	},
	.chan[2] = {
		.class	= 7,
		.weight = 0,
	},
	.chan[3] = {
		.class	= 7,
		.weight = 0,
	},
	.chan[4] = {
		.class	= 7,
		.weight = 0,
	},
	.chan[5] = {
		.class	= 7,
		.weight = 0,
	},
	.chan[6] = {
		.class	= 7,
		.weight = 0,
	},
	.chan[7] = {
		.class	= 7,
		.weight = 0,
	},
};

static struct dma dma[] = {
{	/* Low Power GP DMAC 0 */
	.plat_data = {
		.id		= DMA_GP_LP_DMAC0,
		.base		= LP_GP_DMA_BASE(0),
		.irq		= IRQ_EXT_LP_GPDMA0_LVL5(0),
		.drv_plat_data	= &dmac0,
	},
	.ops		= &dw_dma_ops,
},
{	/* Low Power GP DMAC 1 */
	.plat_data = {
		.id		= DMA_GP_LP_DMAC1,
		.base		= LP_GP_DMA_BASE(1),
		.irq		= IRQ_EXT_LP_GPDMA1_LVL5(0),
		.drv_plat_data	= &dmac1,
	},
	.ops		= &dw_dma_ops,
},
{	/* High Performance GP DMAC 0 */
	.plat_data = {
		.id		= DMA_GP_HP_DMAC0,
		.base		= HP_GP_DMA_BASE(0),
		.irq		= IRQ_EXT_HOST_DMA_IN_LVL3(0),
	},
	.ops		= &dw_dma_ops,
},
{	/* High Performance GP DMAC 1 */
	.plat_data = {
		.id		= DMA_GP_HP_DMAC1,
		.base		= HP_GP_DMA_BASE(1),
		.irq		= IRQ_EXT_HOST_DMA_OUT_LVL3(0),
	},
	.ops		= &dw_dma_ops,
},
{	/* Host In DMAC */
	.plat_data = {
		.id		= DMA_HOST_IN_DMAC,
		.base		= GTW_HOST_IN_STREAM_BASE(0),
		.irq		= IRQ_EXT_HOST_DMA_IN_LVL3(0),
	},
	.ops		= &dw_dma_ops,
},
{	/* Host out DMAC */
	.plat_data = {
		.id		= DMA_HOST_IN_DMAC,
		.base		= GTW_HOST_OUT_STREAM_BASE(0),
		.irq		= IRQ_EXT_HOST_DMA_OUT_LVL3(0),
	},
	.ops		= &dw_dma_ops,
},
{	/* Link In DMAC */
	.plat_data = {
		.id		= DMA_LINK_IN_DMAC,
		.base		= GTW_LINK_IN_STREAM_BASE(0),
		.irq		= IRQ_EXT_LINK_DMA_IN_LVL4(0),
	},
	.ops		= &dw_dma_ops,
},
{	/* Link out DMAC */
	.plat_data = {
		.id		= DMA_LINK_IN_DMAC,
		.base		= GTW_LINK_OUT_STREAM_BASE(0),
		.irq		= IRQ_EXT_LINK_DMA_IN_LVL4(0),
	},
	.ops		= &dw_dma_ops,
},};

struct dma *dma_get(int dmac_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dma); i++) {
		if (dma[i].plat_data.id == dmac_id)
			return &dma[i];
	}

	return NULL;
}
