// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#include <sof/common.h>
#include <sof/lib/dai.h>
#include <sof/lib/memory.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <ipc/dai.h>
#include <ipc/stream.h>

#include <sof/drivers/afe-dai.h>
#include <mt8365-afe-common.h>

/*	MEMIF specified IRQs set in the Linux driver:
 *
 *	[MT8365_AFE_MEMIF_DL1] = MT8365_AFE_IRQ1,
 *	[MT8365_AFE_MEMIF_DL2] = MT8365_AFE_IRQ2,
 *	[MT8365_AFE_MEMIF_TDM_OUT] = MT8365_AFE_IRQ5,
 *	// [MT8365_AFE_MEMIF_SPDIF_OUT] = MT8365_AFE_IRQ6,
 *	[MT8365_AFE_MEMIF_AWB] = MT8365_AFE_IRQ3,
 *	[MT8365_AFE_MEMIF_VUL] = MT8365_AFE_IRQ4,
 *	[MT8365_AFE_MEMIF_VUL2] = MT8365_AFE_IRQ7,
 *	[MT8365_AFE_MEMIF_VUL3] = MT8365_AFE_IRQ8,
 *	// [MT8365_AFE_MEMIF_SPDIF_IN] = MT8365_AFE_IRQ9,
 *	[MT8365_AFE_MEMIF_TDM_IN] = MT8365_AFE_IRQ10,
 */

static int afe_dai_handshake[MT8365_DAI_NUM] = {
	AFE_HANDSHAKE(MT8365_AFE_IO_INT_ADDA_OUT, MT8365_AFE_IRQ_1, MT8365_MEMIF_DL1),
	AFE_HANDSHAKE(MT8365_AFE_IO_2ND_I2S, MT8365_AFE_IRQ_2, MT8365_MEMIF_DL2),
	AFE_HANDSHAKE(MT8365_AFE_IO_INT_ADDA_IN, MT8365_AFE_IRQ_3, MT8365_MEMIF_AWB),
	AFE_HANDSHAKE(MT8365_AFE_IO_DMIC, MT8365_AFE_IRQ_4, MT8365_MEMIF_VUL),
};

static SHARED_DATA struct dai afe_dai[MT8365_DAI_NUM];

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
