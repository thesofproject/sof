// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2020 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/common.h>
#include <sof/drivers/sai.h>
#include <sof/drivers/micfil.h>

#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
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
			.watermark = 64, /* half the depth */
			/* Handshake is SDMA hardware event */
			.handshake = 1,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset = SAI_1_BASE + REG_SAI_RDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			.handshake = 0,
		},
	},
	.drv = &sai_driver,
},

{
	.index = 2,
	.plat_data = {
		.base = SAI_2_BASE,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset = SAI_2_BASE + REG_SAI_TDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			/* Handshake is SDMA hardware event */
			.handshake = 3,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset = SAI_2_BASE + REG_SAI_RDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			.handshake = 2,
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
			.watermark = 64, /* half the depth */
			/* Handshake is SDMA hardware event */
			.handshake = 5,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset = SAI_3_BASE + REG_SAI_RDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			.handshake = 4,
		},
	},
	.drv = &sai_driver,
},

{
	.index = 5,
	.plat_data = {
		.base = SAI_5_BASE,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset = SAI_5_BASE + REG_SAI_TDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			/* Handshake is SDMA hardware event */
			.handshake = 9,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset = SAI_5_BASE + REG_SAI_RDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			.handshake = 8,
		},
	},
	.drv = &sai_driver,
},

{
	.index = 6,
	.plat_data = {
		.base = SAI_6_BASE,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset = SAI_6_BASE + REG_SAI_TDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			/* Handshake is SDMA hardware event */
			.handshake = 11,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset = SAI_6_BASE + REG_SAI_RDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			.handshake = 10,
		},
	},
	.drv = &sai_driver,
},

{
	.index = 7,
	.plat_data = {
		.base = SAI_7_BASE,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset = SAI_7_BASE + REG_SAI_TDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			/* Handshake is SDMA hardware event */
			.handshake = 13,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset = SAI_7_BASE + REG_SAI_RDR0,
			.depth = 128, /* in 4 bytes words */
			.watermark = 64, /* half the depth */
			.handshake = 12,
		},
	},
	.drv = &sai_driver,
},

};

static SHARED_DATA struct dai micfil[] = {
{
	.index = 2,
	.plat_data = {
		.base = MICFIL_BASE,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset = 0, /* No playback */
			.handshake = 0,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset = MICFIL_BASE + REG_MICFIL_DATACH0,
			.handshake = 24,
		},
	},

	.drv = &micfil_driver,
},
};

const struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_IMX_SAI,
		.dai_array = cache_to_uncache_init((struct dai *)sai),
		.num_dais = ARRAY_SIZE(sai)
	},
	{
		.type = SOF_DAI_IMX_MICFIL,
		.dai_array = cache_to_uncache_init((struct dai *)micfil),
		.num_dais = ARRAY_SIZE(micfil)
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
		k_spinlock_init(&dti[0].dai_array[i].lock);

	sof->dai_info = &lib_dai;

	return 0;
}
