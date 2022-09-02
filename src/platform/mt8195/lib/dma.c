// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author: YC Hung <yc.hung@mediatek.com>

#include <sof/common.h>
#include <rtos/interrupt.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>

#include <mt8195-afe-regs.h>
#include <mt8195-afe-common.h>

extern struct dma_ops dummy_dma_ops;
extern struct dma_ops memif_ops;

static SHARED_DATA struct dma dma[PLATFORM_NUM_DMACS] = {
{
	.plat_data = {
		.id		= DMA_ID_HOST,
		.dir		= DMA_DIR_HMEM_TO_LMEM | DMA_DIR_LMEM_TO_HMEM,
		.devs		= DMA_DEV_HOST,
		.channels	= 16,
	},
	.ops	= &dummy_dma_ops,
},
{
	.plat_data = {
		.id		= DMA_ID_AFE_MEMIF,
		.dir		= DMA_DIR_MEM_TO_DEV | DMA_DIR_DEV_TO_MEM,
		.devs		= DMA_DEV_AFE_MEMIF,
		.base		= AFE_BASE_ADDR,
		.channels	= MT8195_MEMIF_NUM,
	},
	.ops	= &memif_ops,
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

	sof->dma_info = &lib_dma;

	return 0;
}
