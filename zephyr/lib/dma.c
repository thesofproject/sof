// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022-2024 Intel Corporation.
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
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <zephyr/device.h>

#define DW_DMA_BUFFER_PERIOD_COUNT	0x4
#define HDA_DMA_BUFFER_PERIOD_COUNT	4

SHARED_DATA struct dma dma[] = {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(lpgpdma0), okay)
{	/* Low Power GP DMAC 0 */
	.plat_data = {
		.dir		= SOF_DMA_DIR_MEM_TO_MEM | SOF_DMA_DIR_MEM_TO_DEV |
				  SOF_DMA_DIR_DEV_TO_MEM | SOF_DMA_DIR_DEV_TO_DEV,
		.caps		= SOF_DMA_CAP_GP_LP,
		.devs		= SOF_DMA_DEV_SSP | SOF_DMA_DEV_DMIC |
				  SOF_DMA_DEV_ALH,
		.channels	= 8,
		.period_count	= DW_DMA_BUFFER_PERIOD_COUNT,
	},
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(lpgpdma0)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(lpgpdma1), okay)
{	/* Low Power GP DMAC 1 */
	.plat_data = {
		.dir		= SOF_DMA_DIR_MEM_TO_MEM | SOF_DMA_DIR_MEM_TO_DEV |
				  SOF_DMA_DIR_DEV_TO_MEM | SOF_DMA_DIR_DEV_TO_DEV,
		.caps		= SOF_DMA_CAP_GP_LP,
		.devs		= SOF_DMA_DEV_SSP | SOF_DMA_DEV_DMIC |
				  SOF_DMA_DEV_ALH,
		.channels	= 8,
		.period_count	= DW_DMA_BUFFER_PERIOD_COUNT,
	},
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(lpgpdma1)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(hda_host_in), okay)
{	/* Host In DMAC */
	.plat_data = {
		.dir		= SOF_DMA_DIR_LMEM_TO_HMEM,
		.caps		= SOF_DMA_CAP_HDA,
		.devs		= SOF_DMA_DEV_HOST,
		.channels	= DT_PROP(DT_NODELABEL(hda_host_in), dma_channels),
		.period_count	= HDA_DMA_BUFFER_PERIOD_COUNT,
	},
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(hda_host_in)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(hda_host_out), okay)
{	/* Host out DMAC */
	.plat_data = {
		.dir		= SOF_DMA_DIR_HMEM_TO_LMEM,
		.caps		= SOF_DMA_CAP_HDA,
		.devs		= SOF_DMA_DEV_HOST,
		.channels	= DT_PROP(DT_NODELABEL(hda_host_out), dma_channels),
		.period_count	= HDA_DMA_BUFFER_PERIOD_COUNT,
	},
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(hda_host_out)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(hda_link_in), okay)
{	/* Link In DMAC */
	.plat_data = {
		.dir		= SOF_DMA_DIR_DEV_TO_MEM,
		.caps		= SOF_DMA_CAP_HDA,
#if defined(CONFIG_SOC_INTEL_ACE20_LNL) || defined(CONFIG_SOC_INTEL_ACE30)
		.devs		= SOF_DMA_DEV_HDA | SOF_DMA_DEV_SSP |
					  SOF_DMA_DEV_DMIC | SOF_DMA_DEV_ALH,
#else
		.devs		= SOF_DMA_DEV_HDA,
#endif /* CONFIG_SOC_INTEL_ACE20_LNL || CONFIG_SOC_INTEL_ACE30 */
		.channels	= DT_PROP(DT_NODELABEL(hda_link_in), dma_channels),
		.period_count	= HDA_DMA_BUFFER_PERIOD_COUNT,
	},
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(hda_link_in)),
},
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(hda_link_out), okay)
{	/* Link out DMAC */
	.plat_data = {
		.dir		= SOF_DMA_DIR_MEM_TO_DEV,
		.caps		= SOF_DMA_CAP_HDA,
#if defined(CONFIG_SOC_INTEL_ACE20_LNL) || defined(CONFIG_SOC_INTEL_ACE30)
		.devs		= SOF_DMA_DEV_HDA | SOF_DMA_DEV_SSP |
					  SOF_DMA_DEV_DMIC | SOF_DMA_DEV_ALH,
#else
		.devs		= SOF_DMA_DEV_HDA,
#endif /* CONFIG_SOC_INTEL_ACE20_LNL || CONFIG_SOC_INTEL_ACE30 */
		.channels	= DT_PROP(DT_NODELABEL(hda_link_out), dma_channels),
		.period_count	= HDA_DMA_BUFFER_PERIOD_COUNT,
	},
	.z_dev		= DEVICE_DT_GET(DT_NODELABEL(hda_link_out)),
},
#endif
#ifdef CONFIG_SOC_MIMX9352_A55
{
	.plat_data = {
		.dir = SOF_DMA_DIR_MEM_TO_DEV | SOF_DMA_DIR_DEV_TO_MEM,
		.devs = SOF_DMA_DEV_SAI,
		/* TODO: might be worth using `dma-channels` here
		 * (needs to become a mandatory property)
		 */
		.channels = 64,
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(edma4)),
},
{
	.plat_data = {
		.dir = SOF_DMA_DIR_HMEM_TO_LMEM | SOF_DMA_DIR_LMEM_TO_HMEM,
		.devs = SOF_DMA_DEV_HOST,
		.channels = DT_PROP(DT_NODELABEL(host_dma), dma_channels),
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(host_dma)),
},
#endif /* CONFIG_SOC_MIMX9352_A55 */
#if defined(CONFIG_SOC_MIMX8QM6_ADSP) || defined(CONFIG_SOC_MIMX8QX6_ADSP)
{
	.plat_data = {
		.dir = SOF_DMA_DIR_MEM_TO_DEV | SOF_DMA_DIR_DEV_TO_MEM,
		.devs = SOF_DMA_DEV_SAI | SOF_DMA_DEV_ESAI,
		.channels = 32,
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(edma0)),
},
{
	.plat_data = {
		.dir = SOF_DMA_DIR_HMEM_TO_LMEM | SOF_DMA_DIR_LMEM_TO_HMEM,
		.devs = SOF_DMA_DEV_HOST,
		.channels = DT_PROP(DT_NODELABEL(host_dma), dma_channels),
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(host_dma)),
},
#endif
#if defined(CONFIG_SOC_MIMX8ML8_ADSP)
{
	.plat_data = {
		.dir = SOF_DMA_DIR_MEM_TO_DEV | SOF_DMA_DIR_DEV_TO_MEM,
		.devs = SOF_DMA_DEV_SAI,
		.channels = 32,
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(sdma3)),
},
{
	.plat_data = {
		.dir = SOF_DMA_DIR_HMEM_TO_LMEM | SOF_DMA_DIR_LMEM_TO_HMEM,
		.devs = SOF_DMA_DEV_HOST,
		.channels = 32,
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(host_dma)),
},
#endif /* CONFIG_SOC_MIMX8ML8_ADSP */
#ifdef CONFIG_SOC_MIMX8UD7_ADSP
{
	.plat_data = {
		.dir = SOF_DMA_DIR_MEM_TO_DEV | SOF_DMA_DIR_DEV_TO_MEM,
		.devs = SOF_DMA_DEV_SAI,
		.channels = 32,
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(edma2)),
},
{
	.plat_data = {
		.dir = SOF_DMA_DIR_HMEM_TO_LMEM | SOF_DMA_DIR_LMEM_TO_HMEM,
		.devs = SOF_DMA_DEV_HOST,
		.channels = DT_PROP(DT_NODELABEL(host_dma), dma_channels),
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(host_dma)),
},
#endif /* CONFIG_SOC_MIMX8UD7_ADSP */
#ifdef CONFIG_SOC_MIMX9596_M7
{
	.plat_data = {
		.dir = SOF_DMA_DIR_MEM_TO_DEV | SOF_DMA_DIR_DEV_TO_MEM,
		.devs = SOF_DMA_DEV_SAI,
		.channels = 64,
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(edma2)),
},
{
	.plat_data = {
		.dir = SOF_DMA_DIR_HMEM_TO_LMEM | SOF_DMA_DIR_LMEM_TO_HMEM,
		.devs = SOF_DMA_DEV_HOST,
		.channels = DT_PROP(DT_NODELABEL(host_dma), dma_channels),
		.period_count = 2,
	},
	.z_dev = DEVICE_DT_GET(DT_NODELABEL(host_dma)),
},
#endif /* CONFIG_SOC_MIMX9596_M7 */
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
