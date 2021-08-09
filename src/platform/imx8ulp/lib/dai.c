// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2021 NXP
//
// Author: Zhang Peng <peng.zhang_8@nxp.com>

#include <sof/common.h>
#include <sof/drivers/edma.h>
#include <sof/drivers/sai.h>
#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/dai.h>
#include <ipc/stream.h>

static SHARED_DATA struct dai sai[] = {
{
	.index = 5,
	.plat_data = {
		.base = SAI_5_BASE,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SAI_5_BASE + REG_SAI_TDR0,
			/* use depth to model the HW FIFO size:
			 * each channel includes a 16 x 32 bit FIFO
			 * that can be accessed using Transmit or
			 * Receive Data Registers
			 */
			.depth		= 16,  /* in 4 bytes words */
			.watermark      = 8,
			.handshake	= EDMA_HANDSHAKE(IMX8ULP_EDMA2_CHAN0_IRQ,
							 IMX8ULP_EDMA2_CHAN0),
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SAI_5_BASE + REG_SAI_RDR3,
			.depth		= 16,  /* in 4 bytes words */
			.watermark      = 8,
			.handshake	= EDMA_HANDSHAKE(IMX8ULP_EDMA2_CHAN1_IRQ,
							 IMX8ULP_EDMA2_CHAN1),
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

	platform_shared_commit(sai, sizeof(*sai));

	sof->dai_info = &lib_dai;

	return 0;
}
