// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/dw-dma.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <sof/spinlock.h>

const struct dw_drv_plat_data dmac0 = {
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

const struct dw_drv_plat_data dmac1 = {
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

#if defined CONFIG_CHERRYTRAIL_EXTRA_DW_DMA
const struct dw_drv_plat_data dmac2 = {
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
#endif

SHARED_DATA struct dma dma[PLATFORM_NUM_DMACS] = {
{
	.plat_data = {
		.id		= DMA_ID_DMAC0,
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV |
				  DMA_DIR_HMEM_TO_LMEM | DMA_DIR_LMEM_TO_HMEM,
		.caps		= DMA_CAP_GP_HP,
		.devs		= DMA_DEV_SSP | DMA_DEV_DMIC | DMA_DEV_HOST,
		.base		= DMA0_BASE,
		.channels	= 8,
		.irq		= IRQ_NUM_EXT_DMAC0,
		.drv_plat_data	= &dmac0,
	},
	.ops		= &dw_dma_ops,
},
{
	.plat_data = {
		.id		= DMA_ID_DMAC1,
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV |
				  DMA_DIR_HMEM_TO_LMEM | DMA_DIR_LMEM_TO_HMEM,
		.caps		= DMA_CAP_GP_HP,
		.devs		= DMA_DEV_SSP | DMA_DEV_DMIC | DMA_DEV_HOST,
		.base		= DMA1_BASE,
		.channels	= 8,
		.irq		= IRQ_NUM_EXT_DMAC1,
		.drv_plat_data	= &dmac1,
	},
	.ops		= &dw_dma_ops,
},
#if defined CONFIG_CHERRYTRAIL_EXTRA_DW_DMA
{
	.plat_data = {
		.id		= DMA_ID_DMAC2,
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV |
				  DMA_DIR_HMEM_TO_LMEM | DMA_DIR_LMEM_TO_HMEM,
		.caps		= DMA_CAP_GP_HP,
		.devs		= DMA_DEV_SSP | DMA_DEV_DMIC | DMA_DEV_HOST,
		.base		= DMA2_BASE,
		.channels	= 8,
		.irq		= IRQ_NUM_EXT_DMAC2,
		.drv_plat_data	= &dmac2,
	},
	.ops		= &dw_dma_ops,
},
#endif
};

const struct dma_info lib_dma = {
	.dma_array = dma,
	.num_dmas = ARRAY_SIZE(dma)
};

/* Initialize all platform DMAC's */
int dmac_init(struct sof *sof)
{
	int i;
	/* no probing before first use */

	/* TODO: dynamic init based on platform settings */

	/* early lock initialization for ref counting */
	for (i = 0; i < ARRAY_SIZE(dma); i++)
		spinlock_init(&dma[i].lock);

	sof->dma_info = &lib_dma;

	return 0;
}
