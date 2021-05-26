// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2021 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Anup Kulkarni <anup.kulkarni@amd.com>
//              Bala Kishore <balakishore.pati@amd.com>

#include <sof/common.h>
#include <sof/drivers/acp_dai_dma.h>
#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/dai.h>
#include <ipc/stream.h>

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

const struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_AMD_BT,
		.dai_array = btdai,
		.num_dais = ARRAY_SIZE(btdai)
	},
};

const struct dai_info lib_dai = {
	.dai_type_array = dti,
	.num_dai_types = ARRAY_SIZE(dti)
};

int dai_init(struct sof *sof)
{
	int i;

	 /* initialize spin locks early to enable ref counting */
	for (i = 0; i < ARRAY_SIZE(btdai); i++)
		spinlock_init(&btdai[i].lock);

	sof->dai_info = &lib_dai;
	return 0;
}
