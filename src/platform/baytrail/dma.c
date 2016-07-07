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
{
	.plat_data = {
		.id 		= DMA_ID_DMAC0,
		.base		= DMA0_BASE,
		.irq		= IRQ_NUM_EXT_DMAC0,
		.drv_plat_data	= &dmac0,
	},
	.ops		= &dw_dma_ops,
},
{
	.plat_data = {
		.id 		= DMA_ID_DMAC1,
		.base		= DMA1_BASE,
		.irq		= IRQ_NUM_EXT_DMAC1,
		.drv_plat_data	= &dmac1,
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
