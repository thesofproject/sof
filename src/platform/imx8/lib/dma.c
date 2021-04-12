// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/common.h>
#include <sof/drivers/edma.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <sof/spinlock.h>

extern struct dma_ops dummy_dma_ops;
extern struct dma_ops edma_ops;

const int edma0_ints[EDMA0_CHAN_MAX] = {
	[EDMA0_ESAI_CHAN_RX] = EDMA0_ESAI_CHAN_RX_IRQ,
	[EDMA0_ESAI_CHAN_TX] = EDMA0_ESAI_CHAN_TX_IRQ,
	[EDMA0_SAI_CHAN_RX] = EDMA0_SAI_CHAN_RX_IRQ,
	[EDMA0_SAI_CHAN_TX] = EDMA0_SAI_CHAN_TX_IRQ,
};

SHARED_DATA struct dma dma[PLATFORM_NUM_DMACS] = {
{
	.plat_data = {
		.id		= DMA_ID_EDMA0,
		.dir		= DMA_DIR_MEM_TO_DEV | DMA_DIR_DEV_TO_MEM,
		.devs		= DMA_DEV_ESAI | DMA_DEV_SAI,
		.base		= EDMA0_BASE,
		.chan_size	= EDMA0_SIZE,
		.channels	= 32,
		.drv_plat_data	= edma0_ints,
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

const struct dma_info lib_dma = {
	.dma_array = dma,
	.num_dmas = ARRAY_SIZE(dma)
};

int dmac_init(struct sof *sof)
{
	int i;

	/* early lock initialization for ref counting */
	for (i = 0; i < ARRAY_SIZE(dma); i++)
		spinlock_init(&dma[i].lock);

	sof->dma_info = &lib_dma;

	return 0;
}
