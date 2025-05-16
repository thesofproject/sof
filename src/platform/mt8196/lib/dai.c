// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Darren Ye <darren.ye@mediatek.com>
 */

#include <sof/common.h>
#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <ipc/dai.h>
#include <ipc/stream.h>
#include <sof/drivers/afe-dai.h>
#include <mt8196-afe-common.h>

static int afe_dai_handshake[MT8196_DAI_NUM] = {
	AFE_HANDSHAKE(MT8196_DAI_I2S_OUT4, MT8196_IRQ_12, MT8196_MEMIF_DL_24CH),
	AFE_HANDSHAKE(MT8196_DAI_I2S_OUT6, MT8196_IRQ_1, MT8196_MEMIF_DL1),
	AFE_HANDSHAKE(MT8196_DAI_AP_DMIC, MT8196_IRQ_13, MT8196_MEMIF_UL0),
	AFE_HANDSHAKE(MT8196_DAI_I2S_IN6, MT8196_IRQ_0, MT8196_MEMIF_UL1),
	AFE_HANDSHAKE(MT8196_DAI_AP_DMIC_CH34, MT8196_IRQ_15, MT8196_MEMIF_UL2),
};

static SHARED_DATA struct dai afe_dai[MT8196_DAI_NUM];

const struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_MEDIATEK_AFE,
		.dai_array = afe_dai,
		.num_dais = ARRAY_SIZE(afe_dai),
	},
};

const struct dai_info lib_dai = {
	.dai_type_array = dti,
	.num_dai_types = ARRAY_SIZE(dti),
};

int dai_init(struct sof *sof)
{
	int i;

	/* initialize spin locks early to enable ref counting */
	for (i = 0; i < ARRAY_SIZE(afe_dai); i++) {
		k_spinlock_init(&afe_dai[i].lock);
		afe_dai[i].index = AFE_HS_GET_DAI(afe_dai_handshake[i]);
		afe_dai[i].drv = &afe_dai_driver;
		/* TODO, fifo[0] change to target playback or capture */
		afe_dai[i].plat_data.fifo[0].handshake = afe_dai_handshake[i];
	}

	sof->dai_info = &lib_dai;

	return 0;
}
