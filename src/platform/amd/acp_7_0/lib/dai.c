// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2024 AMD. All rights reserved.
//
//Author:	SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>

#include <sof/common.h>
#include <sof/drivers/acp_dai_dma.h>
#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <ipc/dai.h>
#include <ipc/stream.h>

static struct dai acp_dmic_dai[] = {
	{
		.index = 0,
		.plat_data = {
			.base = DMA0_BASE,
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset         = DMA0_BASE,
			.depth          = 8,
			.handshake      = 0,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= DMA0_BASE,
			.depth		= 8,
			.handshake	= 1,
			},
		},
		.drv = &acp_dmic_dai_driver,
	},
};

static struct dai hsdai[] = {
	{
		.index = 0,
		.plat_data = {
			.base = DAI_BASE_REM,
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset         = DAI_BASE_REM + HS_TX_FIFO_OFFST,
			.depth          = 8,
			.handshake      = 1,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset         = DAI_BASE_REM + HS_RX_FIFO_OFFST,
			.depth          = 8,
			.handshake      = 0,
			},
		},
		.drv = &acp_hsdai_driver,
	},
	{
		.index = 1,
		.plat_data = {
			.base = DAI_BASE_REM,
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset         = DAI_BASE_REM + HS_TX_FIFO_OFFST,
			.depth          = 8,
			.handshake      = 1,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset         = DAI_BASE_REM + HS_RX_FIFO_OFFST,
			.depth          = 8,
			.handshake      = 0,
			},
		},
		.drv = &acp_hsdai_driver,
	}
};

#ifdef ACP_SP_ENABLE
static struct dai spdai[] = {
	{
		.index = 0,
		.plat_data = {
			.base = DAI_BASE,
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
				.offset         = DAI_BASE + BT_TX_FIFO_OFFST,
				.depth          = 8,
				.handshake      = 5,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
				.offset         = DAI_BASE + BT_RX_FIFO_OFFST,
				.depth          = 8,
				.handshake      = 4,
			},
		},
		.drv = &acp_spdai_driver,
	}
};

static struct dai sp_virtual_dai[] = {
	{
		.index = 1,
		.plat_data = {
			.base = DAI_BASE,
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
				.offset         = DAI_BASE + BT_TX_FIFO_OFFST,
				.depth          = 8,
				.handshake      = 5,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
				.offset         = DAI_BASE + BT_RX_FIFO_OFFST,
				.depth          = 8,
				.handshake      = 4,
			},
		},
		.drv = &acp_sp_virtual_dai_driver,
	}
};
#endif
#ifdef ACP_BT_ENABLE
static struct dai btdai[] = {
	{
		.index = 0,
		.plat_data = {
			.base = DAI_BASE,
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
				.offset		= DAI_BASE + BT_TX_FIFO_OFFST,
				.depth		= 8,
				.handshake	= 3,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
				.offset		= DAI_BASE + BT_RX_FIFO_OFFST,
				.depth		= 8,
				.handshake	= 2,
			},
		},
		.drv = &acp_btdai_driver,
	},
};
#endif

const struct dai_type_info dti[] = {
	{
		.type		= SOF_DAI_AMD_DMIC,
		.dai_array	= acp_dmic_dai,
		.num_dais	= ARRAY_SIZE(acp_dmic_dai)
	},
	{
		.type		= SOF_DAI_AMD_HS,
		.dai_array	= hsdai,
		.num_dais	= ARRAY_SIZE(hsdai)
	},
#ifdef ACP_SP_ENABLE
	{
		.type		= SOF_DAI_AMD_SP,
		.dai_array	= spdai,
		.num_dais	= ARRAY_SIZE(spdai)
	},
	{
		.type		= SOF_DAI_AMD_SP_VIRTUAL,
		.dai_array	= sp_virtual_dai,
		.num_dais	= ARRAY_SIZE(sp_virtual_dai)
	},
#endif
#ifdef ACP_BT_ENABLE
	{
		.type = SOF_DAI_AMD_BT,
		.dai_array = btdai,
		.num_dais = ARRAY_SIZE(btdai)
	},
#endif
};

const struct dai_info lib_dai = {
	.dai_type_array = dti,
	.num_dai_types = ARRAY_SIZE(dti)
};

int dai_init(struct sof *sof)
{
	int i;

	/* initialize spin locks early to enable ref counting */
	for (i = 0; i < ARRAY_SIZE(acp_dmic_dai); i++)
		k_spinlock_init(&acp_dmic_dai[i].lock);
	for (i = 0; i < ARRAY_SIZE(hsdai); i++)
		k_spinlock_init(&hsdai[i].lock);
#ifdef ACP_SP_ENABLE
	for (i = 0; i < ARRAY_SIZE(spdai); i++)
		k_spinlock_init(&spdai[i].lock);
	/* initialize spin locks early to enable ref counting */
	for (i = 0; i < ARRAY_SIZE(sp_virtual_dai); i++)
		k_spinlock_init(&sp_virtual_dai[i].lock);
#endif
#ifdef ACP_BT_ENABLE
	/* initialize spin locks early to enable ref counting */
	for (i = 0; i < ARRAY_SIZE(btdai); i++)
		k_spinlock_init(&btdai[i].lock);
#endif
	sof->dai_info = &lib_dai;
	return 0;
}
