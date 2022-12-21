// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>
//         Adrian Bonislawski <adrian.bonislawski@intel.com>
//         Adrian Warecki <adrian.warecki@intel.com>

#include <sof/common.h>
#include <rtos/interrupt.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <zephyr/device.h>

#define DW_DMA_BUFFER_ALIGNMENT		0x4
#define DW_DMA_COPY_ALIGNMENT		0x4
#define DW_DMA_BUFFER_PERIOD_COUNT	0x8

static int dw_dma_get_attribute(struct dma *dma, uint32_t type,
				uint32_t *value)
{
	int ret = 0;

	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
		*value = DW_DMA_BUFFER_ALIGNMENT;
		break;
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = DW_DMA_COPY_ALIGNMENT;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = PLATFORM_DCACHE_ALIGN;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = DW_DMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

const struct dma_ops dw_dma_ops = {
	.get_attribute		= dw_dma_get_attribute,
};

#define HDA_DMA_BUFFER_ALIGNMENT		0x20
#define HDA_DMA_COPY_ALIGNMENT			0x20
#define HDA_DMA_BUFFER_ADDRESS_ALIGNMENT	0x80
#define HDA_DMA_BUFFER_PERIOD_COUNT		4

static int hda_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
		*value = HDA_DMA_BUFFER_ALIGNMENT;
		break;
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = HDA_DMA_COPY_ALIGNMENT;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = HDA_DMA_BUFFER_ADDRESS_ALIGNMENT;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = HDA_DMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

const struct dma_ops hda_dma_ops = {
	.get_attribute		= hda_dma_get_attribute,
};

SHARED_DATA struct dma dma[] = {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(lpgpdma0), okay)
{	/* Low Power GP DMAC 0 */
	.plat_data = {
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV,
		.caps		= DMA_CAP_GP_LP,
		.devs		= DMA_DEV_SSP | DMA_DEV_DMIC |
				  DMA_DEV_ALH,
		.channels	= 8,
	},
	.ops		= &dw_dma_ops,
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(lpgpdma0)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(lpgpdma1), okay)
{	/* Low Power GP DMAC 1 */
	.plat_data = {
		.dir		= DMA_DIR_MEM_TO_MEM | DMA_DIR_MEM_TO_DEV |
				  DMA_DIR_DEV_TO_MEM | DMA_DIR_DEV_TO_DEV,
		.caps		= DMA_CAP_GP_LP,
		.devs		= DMA_DEV_SSP | DMA_DEV_DMIC |
				  DMA_DEV_ALH,
		.channels	= 8,
	},
	.ops		= &dw_dma_ops,
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(lpgpdma1)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(hda_host_in), okay)
{	/* Host In DMAC */
	.plat_data = {
		.dir		= DMA_DIR_LMEM_TO_HMEM,
		.caps		= DMA_CAP_HDA,
		.devs		= DMA_DEV_HOST,
		.channels	= DT_PROP(DT_NODELABEL(hda_host_in), dma_channels),
	},
	.ops		= &hda_dma_ops,
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(hda_host_in)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(hda_host_out), okay)
{	/* Host out DMAC */
	.plat_data = {
		.dir		= DMA_DIR_HMEM_TO_LMEM,
		.caps		= DMA_CAP_HDA,
		.devs		= DMA_DEV_HOST,
		.channels	= DT_PROP(DT_NODELABEL(hda_host_out), dma_channels),
	},
	.ops		= &hda_dma_ops,
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(hda_host_out)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(hda_link_in), okay)
{	/* Link In DMAC */
	.plat_data = {
		.dir		= DMA_DIR_DEV_TO_MEM,
		.caps		= DMA_CAP_HDA,
		.devs		= DMA_DEV_HDA,
		.channels	= DT_PROP(DT_NODELABEL(hda_link_in), dma_channels),
	},
	.ops		= &hda_dma_ops,
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(hda_link_in)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(hda_link_out), okay)
{	/* Link out DMAC */
	.plat_data = {
		.dir		= DMA_DIR_MEM_TO_DEV,
		.caps		= DMA_CAP_HDA,
		.devs		= DMA_DEV_HDA,
		.channels	= DT_PROP(DT_NODELABEL(hda_link_out), dma_channels),
	},
	.ops		= &hda_dma_ops,
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(hda_link_out)),
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

	sof->dma_info = &lib_dma;

	/* early lock initialization for ref counting */
	for (i = 0; i < sof->dma_info->num_dmas; i++)
		k_spinlock_init(&sof->dma_info->dma_array[i].lock);

	return 0;
}
