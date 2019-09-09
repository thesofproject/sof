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
#include <sof/spinlock.h>

extern struct dma_ops dummy_dma_ops;
extern struct dma_ops edma_ops;

struct dma dma[PLATFORM_NUM_DMACS] = {
{
	.plat_data = {
		.id		= DMA_ID_EDMA0,
		.dir		= DMA_DIR_MEM_TO_DEV | DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV,
		.devs		= DMA_DEV_ESAI,
		.base		= EDMA0_BASE,
		.chan_size	= EDMA0_SIZE,
		.channels	= 32,
		.irq		= IRQ_NUM_IRQSTR_DSP6,
	},
	.ops	= &edma_ops,
},
{
	.plat_data = {
		.id		= DMA_ID_HOST,
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_HMEM_TO_LMEM | DMA_DIR_LMEM_TO_HMEM,
		.devs		= DMA_DEV_HOST,
		.channels	= 16,
	},
	.ops	= &dummy_dma_ops,
},
};

int edma_init(void)
{
	int i;

	/* early lock initialization for ref counting */
	for (i = 0; i < ARRAY_SIZE(dma); i++)
		spinlock_init(&dma[i].lock);

	/* tell the lib DMAs are ready to use */
	dma_install(dma, ARRAY_SIZE(dma));

	return 0;
}
