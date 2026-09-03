// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2023 NXP
 */

#include <rtos/sof.h>
#include <sof/drivers/sai.h>
#include <sof/drivers/edma.h>
#include <sof/lib/memory.h>

static struct dai sai[] = {
	{
		.index = 3, /* SOF uses SAI3 */
		.plat_data = {
			.base = SAI3_BASE,
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
				.offset = SAI3_BASE + REG_SAI_TDR0,
				.depth = 128, /* number of 32-bit words */
				.watermark = 64, /* needs to be half the depth */
				.handshake = EDMA_HANDSHAKE(EDMA2_SAI3_CHAN_TX_IRQ,
							    EDMA2_SAI3_CHAN_TX,
							    EDMA2_SAI3_TX_MUX),
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
				.offset = SAI3_BASE + REG_SAI_RDR0,
				.depth = 128,
				.watermark = 64,
				.handshake = EDMA_HANDSHAKE(EDMA2_SAI3_CHAN_RX_IRQ,
							    EDMA2_SAI3_CHAN_RX,
							    EDMA2_SAI3_RX_MUX),
			},
		},
		.drv = &sai_driver,
	},
};

static const struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_IMX_SAI,
		.dai_array = sai,
		.num_dais = ARRAY_SIZE(sai),
	},
};

static const struct dai_info lib_dai = {
	.dai_type_array = dti,
	.num_dai_types = ARRAY_SIZE(dti)
};

int dai_init(struct sof *sof)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sai); i++)
		k_spinlock_init(&dti[0].dai_array[i].lock);

	sof->dai_info = &lib_dai;

	return 0;
}
