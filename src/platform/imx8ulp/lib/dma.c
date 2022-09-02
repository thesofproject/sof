// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2021 NXP
//
// Author: Peng Zhang <peng.zhang_8@nxp.com>

#include <sof/common.h>
#include <sof/drivers/edma.h>
#include <rtos/interrupt.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>

extern struct dma_ops dummy_dma_ops;
extern struct dma_ops edma_ops;

static const int edma2_ints[IMX8ULP_EDMA2_CHAN_MAX] = {
	[IMX8ULP_EDMA2_CHAN0] = IMX8ULP_EDMA2_CHAN0_IRQ,
	[IMX8ULP_EDMA2_CHAN1] = IMX8ULP_EDMA2_CHAN1_IRQ,
};

static SHARED_DATA struct dma dma[PLATFORM_NUM_DMACS] = {
{
	.plat_data = {
		.id		= DMA_ID_EDMA2,
		.dir		= DMA_DIR_MEM_TO_DEV | DMA_DIR_DEV_TO_MEM,
		.devs		= DMA_DEV_SAI,
		.base		= EDMA2_BASE,
		.chan_size	= EDMA2_SIZE,
		.channels	= 8,
		.drv_plat_data	= edma2_ints,
	},
	.ops	= &edma_ops,
},
{
	.plat_data = {
		.id		= DMA_ID_HOST,
		.dir		= DMA_DIR_HMEM_TO_LMEM | DMA_DIR_LMEM_TO_HMEM,
		.devs		= DMA_DEV_HOST,
		.channels	= 16,
	},
	.ops	= &dummy_dma_ops,
},
};

static const struct dma_info lib_dma = {
	.dma_array = cache_to_uncache_init((struct dma *)dma),
	.num_dmas = ARRAY_SIZE(dma)
};

int dmac_init(struct sof *sof)
{
	int i;

	/* early lock initialization for ref counting */
	for (i = 0; i < ARRAY_SIZE(dma); i++)
		k_spinlock_init(&dma[i].lock);

	platform_shared_commit(dma, sizeof(*dma));

	sof->dma_info = &lib_dma;

	return 0;
}
