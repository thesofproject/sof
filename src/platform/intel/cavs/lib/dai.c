// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/hda.h>
#include <rtos/interrupt.h>
#include <sof/drivers/mn.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <rtos/spinlock.h>
#include <ipc/dai.h>
#include <ipc/stream.h>

#if CONFIG_INTEL_SSP

#include <sof/drivers/ssp.h>

static SHARED_DATA struct dai ssp_shared[DAI_NUM_SSP_BASE + DAI_NUM_SSP_EXT];

#endif

#if CONFIG_INTEL_DMIC

#include <sof/drivers/dmic.h>

static SHARED_DATA struct dai dmic_shared[2] = {
	/* Testing idea if DMIC FIFOs A and B to access the same microphones
	 * with two different sample rate and PCM format could be presented
	 * similarly as SSP0..N. The difference however is that the DMIC
	 * programming is global and not per FIFO.
	 */

	/* Primary FIFO A */
	{
		.index = 0,
		.plat_data = {
			.base = DMIC_BASE,
			.irq = IRQ_EXT_DMIC_LVL5(0),
			.irq_name = irq_name_level5,
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
				.offset = 0, /* No playback */
				.handshake = 0,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
				.offset = DMIC_BASE + OUTDATA0,
				.handshake = DMA_HANDSHAKE_DMIC_CH0,
			}
		},
		.drv = &dmic_driver,
	},
	/* Secondary FIFO B */
	{
		.index = 1,
		.plat_data = {
			.base = DMIC_BASE,
			.irq = IRQ_EXT_DMIC_LVL5(1),
			.irq_name = irq_name_level5,
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
				.offset = 0, /* No playback */
				.handshake = 0,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
				.offset = DMIC_BASE + OUTDATA1,
				.handshake = DMA_HANDSHAKE_DMIC_CH1,
			}
		},
		.drv = &dmic_driver,
	}
};

#endif

#if CONFIG_INTEL_ALH

#include <sof/drivers/alh.h>

static SHARED_DATA struct dai alh_shared[DAI_NUM_ALH_BI_DIR_LINKS];
#endif

static SHARED_DATA struct dai hda_shared[DAI_NUM_HDA_OUT + DAI_NUM_HDA_IN];

const struct dai_type_info dti[] = {
#if CONFIG_INTEL_SSP
	{
		.type = SOF_DAI_INTEL_SSP,
		.dai_array = cache_to_uncache_init((struct dai *)ssp_shared),
		.num_dais = ARRAY_SIZE(ssp_shared)
	},
#endif
#if CONFIG_INTEL_DMIC
	{
		.type = SOF_DAI_INTEL_DMIC,
		.dai_array = cache_to_uncache_init((struct dai *)dmic_shared),
		.num_dais = ARRAY_SIZE(dmic_shared)
	},
#endif
	{
		.type = SOF_DAI_INTEL_HDA,
		.dai_array = cache_to_uncache_init((struct dai *)hda_shared),
		.num_dais = ARRAY_SIZE(hda_shared)
	},
#if CONFIG_INTEL_ALH
	{
		.type = SOF_DAI_INTEL_ALH,
		.dai_array = cache_to_uncache_init((struct dai *)alh_shared),
		.num_dais = ARRAY_SIZE(alh_shared)
	}
#endif
};

const struct dai_info lib_dai = {
	.dai_type_array = dti,
	.num_dai_types = ARRAY_SIZE(dti)
};

int dai_init(struct sof *sof)
{
	struct dai *dai;
#if CONFIG_INTEL_SSP
	const struct dai_type_info *ssp = NULL;
#endif
#if CONFIG_INTEL_ALH
	const struct dai_type_info *alh = NULL;
#endif
#if CONFIG_INTEL_DMIC
	const struct dai_type_info *dmic = NULL;
#endif
	const struct dai_type_info *hda = NULL;
	int i;

	sof->dai_info = &lib_dai;

	for (i = 0; i < ARRAY_SIZE(dti); i++)
		switch (dti[i].type) {
#if CONFIG_INTEL_ALH
		case SOF_DAI_INTEL_ALH:
			alh = dti + i;
			break;
#endif
#if CONFIG_INTEL_DMIC
		case SOF_DAI_INTEL_DMIC:
			dmic = dti + i;
			break;
#endif
#if CONFIG_INTEL_SSP
		case SOF_DAI_INTEL_SSP:
			ssp = dti + i;
			break;
#endif
		case SOF_DAI_INTEL_HDA:
			hda = dti + i;
			break;
		}

#if CONFIG_INTEL_SSP
	if (ssp) {
		dai = ssp->dai_array;

		/* init ssp */
		for (i = 0; i < ssp->num_dais; i++) {
			dai[i].index = i;
			dai[i].drv = &ssp_driver;
			dai[i].plat_data.base = SSP_BASE(i);
			dai[i].plat_data.irq = IRQ_EXT_SSPx_LVL5(i);
			dai[i].plat_data.irq_name = irq_name_level5;
			dai[i].plat_data.fifo[SOF_IPC_STREAM_PLAYBACK].offset =
				SSP_BASE(i) + SSDR;
			dai[i].plat_data.fifo[SOF_IPC_STREAM_PLAYBACK].handshake =
				DMA_HANDSHAKE_SSP0_TX + 2 * i;
			dai[i].plat_data.fifo[SOF_IPC_STREAM_CAPTURE].offset =
				SSP_BASE(i) + SSDR;
			dai[i].plat_data.fifo[SOF_IPC_STREAM_CAPTURE].handshake =
				DMA_HANDSHAKE_SSP0_RX + 2 * i;
			/* initialize spin locks early to enable ref counting */
			k_spinlock_init(&dai[i].lock);
		}
	}
#endif

#if CONFIG_INTEL_MCLK
	mn_init(sof);
#endif

	if (hda) {
		dai = hda->dai_array;

		/* init hd/a, note that size depends on the platform caps */
		for (i = 0; i < hda->num_dais; i++) {
			dai[i].index = i;
			dai[i].drv = &hda_driver;
			k_spinlock_init(&dai[i].lock);
		}
	}

#if (CONFIG_INTEL_DMIC)
	if (dmic) {
		dai = dmic->dai_array;

		/* init dmic */
		for (i = 0; i < dmic->num_dais; i++)
			k_spinlock_init(&dai[i].lock);
	}
#endif

#if CONFIG_INTEL_ALH
	if (alh) {
		dai = alh->dai_array;

		for (i = 0; i < alh->num_dais; i++) {
			dai[i].index = (i / DAI_NUM_ALH_BI_DIR_LINKS_GROUP) << 8 |
				(i % DAI_NUM_ALH_BI_DIR_LINKS_GROUP);
			dai[i].drv = &alh_driver;

			/* set burst length to align with DMAT value in the
			 * Audio Link Hub.
			 */
			dai[i].plat_data.fifo[SOF_IPC_STREAM_PLAYBACK].depth =
				ALH_GPDMA_BURST_LENGTH;
			dai[i].plat_data.fifo[SOF_IPC_STREAM_CAPTURE].depth =
				ALH_GPDMA_BURST_LENGTH;
			k_spinlock_init(&dai[i].lock);
		}
	}
#endif

	return 0;
}
