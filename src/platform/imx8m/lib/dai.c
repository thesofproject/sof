// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2020 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/common.h>
#include <sof/drivers/sai.h>
#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/dai.h>
#include <ipc/stream.h>

static SHARED_DATA struct dai sai[] = {
{
	.index = 1,
	.plat_data = {
		.base = SAI_1_BASE,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset = SAI_1_BASE + REG_SAI_TDR0,
			.depth = 128, /* in 4 bytes words */
			/* Handshake is SDMA hardware event */
			.handshake = 1,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset = SAI_1_BASE + REG_SAI_RDR0,
			.depth = 128, /* in 4 bytes words */
			.handshake = 0,
		},
	},
	.drv = &sai_driver,
},
{
	.index = 3,
	.plat_data = {
		.base = SAI_3_BASE,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset = SAI_3_BASE + REG_SAI_TDR0,
			.depth = 128, /* in 4 bytes words */
			/* Handshake is SDMA hardware event */
			.handshake = 5,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset = SAI_3_BASE + REG_SAI_RDR0,
			.depth = 128, /* in 4 bytes words */
			.handshake = 4,
		},
	},
	.drv = &sai_driver,
},
};

const struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_IMX_SAI,
		.dai_array = sai,
		.num_dais = ARRAY_SIZE(sai)
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
	for (i = 0; i < ARRAY_SIZE(sai); i++)
		spinlock_init(&sai[i].lock);

	sof->dai_info = &lib_dai;

	return 0;
}
