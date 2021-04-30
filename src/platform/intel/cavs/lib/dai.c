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
#include <sof/drivers/interrupt.h>
#include <sof/drivers/mn.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <ipc/dai.h>
#include <ipc/stream.h>

#if CONFIG_INTEL_SSP

#include <sof/drivers/ssp.h>

static SHARED_DATA struct dai ssp[DAI_NUM_SSP_BASE + DAI_NUM_SSP_EXT];

#endif

#if CONFIG_INTEL_DMIC

#include <sof/drivers/dmic.h>

static SHARED_DATA struct dai dmic[2] = {
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

static SHARED_DATA struct dai alh[DAI_NUM_ALH_BI_DIR_LINKS];
#endif

static SHARED_DATA struct dai hda[DAI_NUM_HDA_OUT + DAI_NUM_HDA_IN];

const struct dai_type_info dti[] = {
#if CONFIG_INTEL_SSP
	{
		.type = SOF_DAI_INTEL_SSP,
		.dai_array = cache_to_uncache((struct dai *)ssp),
		.num_dais = ARRAY_SIZE(ssp)
	},
#endif
#if CONFIG_INTEL_DMIC
	{
		.type = SOF_DAI_INTEL_DMIC,
		.dai_array = cache_to_uncache((struct dai *)dmic),
		.num_dais = ARRAY_SIZE(dmic)
	},
#endif
	{
		.type = SOF_DAI_INTEL_HDA,
		.dai_array = cache_to_uncache((struct dai *)hda),
		.num_dais = ARRAY_SIZE(hda)
	},
#if CONFIG_INTEL_ALH
	{
		.type = SOF_DAI_INTEL_ALH,
		.dai_array = cache_to_uncache((struct dai *)alh),
		.num_dais = ARRAY_SIZE(alh)
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
	int i;

	sof->dai_info = &lib_dai;

#if CONFIG_INTEL_SSP
	dai = cache_to_uncache((struct dai *)ssp);

	/* init ssp */
	for (i = 0; i < ARRAY_SIZE(ssp); i++) {
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
		spinlock_init(&dai[i].lock);
	}

#endif

#if CONFIG_INTEL_MCLK
	mn_init(sof);
#endif

	dai = cache_to_uncache((struct dai *)hda);

	/* init hd/a, note that size depends on the platform caps */
	for (i = 0; i < ARRAY_SIZE(hda); i++) {
		dai[i].index = i;
		dai[i].drv = &hda_driver;
		spinlock_init(&dai[i].lock);
	}

#if (CONFIG_INTEL_DMIC)
	dai = cache_to_uncache((struct dai *)dmic);

	/* init dmic */
	for (i = 0; i < ARRAY_SIZE(dmic); i++)
		spinlock_init(&dai[i].lock);

#endif

#if CONFIG_INTEL_ALH
	dai = cache_to_uncache((struct dai *)alh);

	for (i = 0; i < ARRAY_SIZE(alh); i++) {
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
		spinlock_init(&dai[i].lock);
	}

#endif

	return 0;
}
