// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
 */

#include <rtos/interrupt.h>
#include <rtos/spinlock.h>
#include <sof/common.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <rtos/sof.h>

extern const struct dma_ops dummy_dma_ops;

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
