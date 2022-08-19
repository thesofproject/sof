/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 AMD. All rights reserved.
 *
 * Author:	Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *		Anup Kulkarni<anup.kulkarni@amd.com>
 *		Bala Kishore <balakishore.pati@amd.com>
 */
#ifndef __SOF_DRIVERS_ACPDMA_H__
#define __SOF_DRIVERS_ACPDMA_H__

#include <ipc/dai.h>
#include <ipc/dai-amd.h>
#include <sof/lib/dai.h>
#include <rtos/bit.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#if CONFIG_AMD_BT
	#define ACP_BT_ENABLE
#endif

#if CONFIG_AMD_SP
	#define ACP_SP_ENABLE
#endif

int acp_dma_init(struct sof *sof);

#define ACP_DMA_BUFFER_PERIOD_COUNT	2

/* default max number of channels supported */
#define ACP_DEFAULT_NUM_CHANNELS	2

/* default sample rate */
#define ACP_DEFAULT_SAMPLE_RATE		48000

#define ACP_DMA_BUFFER_ALIGN		64
#define ACP_DMA_BUFFER_ALIGN_128	128
#define ACP_DMA_TRANS_SIZE		64
#define ACP_DMA_TRANS_SIZE_128		128
#define ACP_DAI_DMA_BUFFER_PERIOD_COUNT 2
#define ACP_DRAM_ADDRESS_MASK		0x0FFFFFFF


extern const struct dai_driver acp_spdai_driver;
extern const struct dai_driver acp_btdai_driver;
extern const struct dai_driver acp_dmic_dai_driver;
extern const struct dai_driver acp_hsdai_driver;

/* ACP private data */
struct acp_pdata {
	struct sof_ipc_dai_config config;
	struct sof_ipc_dai_acpdmic_params dmic_params;
	struct sof_ipc_dai_acp_params params;
};
#endif /* __SOF_DRIVERS_ACPDMA_H__ */
