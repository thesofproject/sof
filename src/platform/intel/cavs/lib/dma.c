// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/dw-dma.h>
#include <sof/drivers/hda-dma.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/spinlock.h>
#include <config.h>

#if CONFIG_APOLLOLAKE
#define DMAC0_CLASS 1
#define DMAC1_CLASS 2
#define DMAC_HOST_OUT_CHANNELS_COUNT 6
#define DMAC_LINK_IN_CHANNELS_COUNT 8
#define DMAC_LINK_OUT_CHANNELS_COUNT 8
#define CAVS_PLATFORM_NUM_DMACS		6
#elif CONFIG_CANNONLAKE || CONFIG_ICELAKE
#define DMAC0_CLASS 6
#define DMAC1_CLASS 7
#define DMAC_HOST_OUT_CHANNELS_COUNT 9
#define DMAC_LINK_IN_CHANNELS_COUNT 9
#define DMAC_LINK_OUT_CHANNELS_COUNT 7
#define CAVS_PLATFORM_NUM_DMACS		6
#elif CONFIG_SUECREEK
#define DMAC0_CLASS 6
#define DMAC1_CLASS 7
#define CAVS_PLATFORM_NUM_DMACS		3
#endif

static struct dw_drv_plat_data dmac0 = {
	.chan[0] = {
		.class	= DMAC0_CLASS,
		.weight = 0,
	},
	.chan[1] = {
		.class	= DMAC0_CLASS,
		.weight = 0,
	},
	.chan[2] = {
		.class	= DMAC0_CLASS,
		.weight = 0,
	},
	.chan[3] = {
		.class	= DMAC0_CLASS,
		.weight = 0,
	},
	.chan[4] = {
		.class	= DMAC0_CLASS,
		.weight = 0,
	},
	.chan[5] = {
		.class	= DMAC0_CLASS,
		.weight = 0,
	},
	.chan[6] = {
		.class	= DMAC0_CLASS,
		.weight = 0,
	},
	.chan[7] = {
		.class	= DMAC0_CLASS,
		.weight = 0,
	},
};

static struct dw_drv_plat_data dmac1 = {
	.chan[0] = {
		.class	= DMAC1_CLASS,
		.weight = 0,
	},
	.chan[1] = {
		.class	= DMAC1_CLASS,
		.weight = 0,
	},
	.chan[2] = {
		.class	= DMAC1_CLASS,
		.weight = 0,
	},
	.chan[3] = {
		.class	= DMAC1_CLASS,
		.weight = 0,
	},
	.chan[4] = {
		.class	= DMAC1_CLASS,
		.weight = 0,
	},
	.chan[5] = {
		.class	= DMAC1_CLASS,
		.weight = 0,
	},
	.chan[6] = {
		.class	= DMAC1_CLASS,
		.weight = 0,
	},
	.chan[7] = {
		.class	= DMAC1_CLASS,
		.weight = 0,
	},
};

#if CONFIG_SUECREEK
static struct dma dma[CAVS_PLATFORM_NUM_DMACS] = {
{	/* LP GP DMAC 0 */
	.plat_data = {
		.id		= DMA_GP_LP_DMAC0,
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV,
		.caps		= DMA_CAP_GP_LP,
		.devs		= DMA_DEV_SSP | DMA_DEV_SSI | DMA_DEV_DMIC,
		.base		= LP_GP_DMA_BASE(0),
		.channels	= 8,
		.irq		= IRQ_EXT_LP_GPDMA0_LVL5(0),
		.irq_name	= irq_name_level5,
		.drv_plat_data	= &dmac0,
	},
	.ops		= &dw_dma_ops,
},
{	/* LP GP DMAC 1 */
	.plat_data = {
		.id		= DMA_GP_LP_DMAC1,
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV,
		.caps		= DMA_CAP_GP_LP,
		.devs		= DMA_DEV_SSP | DMA_DEV_DMIC,
		.base		= LP_GP_DMA_BASE(1),
		.channels	= 8,
		.irq		= IRQ_EXT_LP_GPDMA1_LVL5(0),
		.irq_name	= irq_name_level5,
		.drv_plat_data	= &dmac1,
	},
	.ops		= &dw_dma_ops,
},
{	/* LP GP DMAC 2 */
	.plat_data = {
		.id		= DMA_GP_LP_DMAC2,
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV,
		.caps		= DMA_CAP_GP_LP,
		.devs		= DMA_DEV_SSP | DMA_DEV_DMIC,
		.base		= LP_GP_DMA_BASE(1),
		.channels	= 8,
		.irq		= IRQ_EXT_LP_GPDMA1_LVL5(0),
		.irq_name	= irq_name_level5,
		.drv_plat_data	= &dmac1,
	},
	.ops		= &dw_dma_ops,
},
};

#else
static struct dma dma[CAVS_PLATFORM_NUM_DMACS] = {
{	/* Low Power GP DMAC 0 */
	.plat_data = {
		.id		= DMA_GP_LP_DMAC0,
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV,
		.caps		= DMA_CAP_GP_LP,
		.devs		= DMA_DEV_SSP | DMA_DEV_DMIC |
				  DMA_DEV_ALH,
		.base		= LP_GP_DMA_BASE(0),
		.channels	= 8,
		.irq		= IRQ_EXT_LP_GPDMA0_LVL5(0),
		.irq_name	= irq_name_level5,
		.drv_plat_data	= &dmac0,
	},
	.ops		= &dw_dma_ops,
},
{	/* Low Power GP DMAC 1 */
	.plat_data = {
		.id		= DMA_GP_LP_DMAC1,
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV,
		.caps		= DMA_CAP_GP_LP,
		.devs		= DMA_DEV_SSP | DMA_DEV_DMIC |
				  DMA_DEV_ALH,
		.base		= LP_GP_DMA_BASE(1),
		.channels	= 8,
		.irq		= IRQ_EXT_LP_GPDMA1_LVL5(0),
		.irq_name	= irq_name_level5,
		.drv_plat_data	= &dmac1,
	},
	.ops		= &dw_dma_ops,
},
{	/* Host In DMAC */
	.plat_data = {
		.id		= DMA_HOST_IN_DMAC,
		.dir		= DMA_DIR_LMEM_TO_HMEM,
		.caps		= DMA_CAP_HDA,
		.devs		= DMA_DEV_HOST,
		.base		= GTW_HOST_IN_STREAM_BASE(0),
		.channels	= 7,
		.irq		= IRQ_EXT_HOST_DMA_IN_LVL3(0),
		.irq_name	= irq_name_level3,
		.chan_size	= GTW_HOST_IN_STREAM_SIZE,
	},
	.ops		= &hda_host_dma_ops,
},
{	/* Host out DMAC */
	.plat_data = {
		.id		= DMA_HOST_OUT_DMAC,
		.dir		= DMA_DIR_HMEM_TO_LMEM,
		.caps		= DMA_CAP_HDA,
		.devs		= DMA_DEV_HOST,
		.base		= GTW_HOST_OUT_STREAM_BASE(0),
		.channels	= DMAC_HOST_OUT_CHANNELS_COUNT,
		.irq		= IRQ_EXT_HOST_DMA_OUT_LVL3(0),
		.irq_name	= irq_name_level3,
		.chan_size	= GTW_HOST_OUT_STREAM_SIZE,
	},
	.ops		= &hda_host_dma_ops,
},
{	/* Link In DMAC */
	.plat_data = {
		.id		= DMA_LINK_IN_DMAC,
		.dir		= DMA_DIR_DEV_TO_MEM,
		.caps		= DMA_CAP_HDA,
		.devs		= DMA_DEV_HDA,
		.base		= GTW_LINK_IN_STREAM_BASE(0),
		.channels	= DMAC_LINK_IN_CHANNELS_COUNT,
		.irq		= IRQ_EXT_LINK_DMA_IN_LVL4(0),
		.irq_name	= irq_name_level4,
		.chan_size	= GTW_LINK_IN_STREAM_SIZE,
	},
	.ops		= &hda_link_dma_ops,
},
{	/* Link out DMAC */
	.plat_data = {
		.id		= DMA_LINK_OUT_DMAC,
		.dir		= DMA_DIR_MEM_TO_DEV,
		.caps		= DMA_CAP_HDA,
		.devs		= DMA_DEV_HDA,
		.base		= GTW_LINK_OUT_STREAM_BASE(0),
		.channels	= DMAC_LINK_OUT_CHANNELS_COUNT,
		.irq		= IRQ_EXT_LINK_DMA_OUT_LVL4(0),
		.irq_name	= irq_name_level4,
		.chan_size	= GTW_LINK_OUT_STREAM_SIZE,
	},
	.ops		= &hda_link_dma_ops,
},};
#endif

/* Initialize all platform DMAC's */
int dmac_init(void)
{
	int i;
	/* no probing before first use */

	/* TODO: dynamic init based on platform settings */

	/* early lock initialization for ref counting */
	for (i = 0; i < ARRAY_SIZE(dma); i++)
		spinlock_init(&dma[i].lock);

	/* tell the lib DMAs are ready to use */
	dma_install(dma, ARRAY_SIZE(dma));

	return 0;
}
