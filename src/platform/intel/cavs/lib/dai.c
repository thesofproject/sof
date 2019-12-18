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
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/spinlock.h>
#include <ipc/dai.h>
#include <ipc/stream.h>
#include <config.h>

#if CONFIG_CAVS_SSP

#include <sof/drivers/ssp.h>

static struct dai ssp[(DAI_NUM_SSP_BASE + DAI_NUM_SSP_EXT)];

#endif

#if CONFIG_CAVS_DMIC

#include <sof/drivers/dmic.h>

static struct dai dmic[2] = {
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

#if CONFIG_CAVS_ALH

#include <sof/drivers/alh.h>

static struct dai alh[DAI_NUM_ALH_BI_DIR_LINKS];
#endif

static struct dai hda[(DAI_NUM_HDA_OUT + DAI_NUM_HDA_IN)];

static struct dai_type_info dti[] = {
#if CONFIG_CAVS_SSP
	{
		.type = SOF_DAI_INTEL_SSP,
		.dai_array = ssp,
		.num_dais = ARRAY_SIZE(ssp)
	},
#endif
#if CONFIG_CAVS_DMIC
	{
		.type = SOF_DAI_INTEL_DMIC,
		.dai_array = dmic,
		.num_dais = ARRAY_SIZE(dmic)
	},
#endif
	{
		.type = SOF_DAI_INTEL_HDA,
		.dai_array = hda,
		.num_dais = ARRAY_SIZE(hda)
	},
#if CONFIG_CAVS_ALH
	{
		.type = SOF_DAI_INTEL_ALH,
		.dai_array = alh,
		.num_dais = ARRAY_SIZE(alh)
	}
#endif
};

int dai_init(void)
{
	int i;
#if CONFIG_CAVS_SSP
	/* init ssp */
	for (i = 0; i < ARRAY_SIZE(ssp); i++) {
		ssp[i].index = i;
		ssp[i].drv = &ssp_driver;
		ssp[i].plat_data.base = SSP_BASE(i);
		ssp[i].plat_data.irq = IRQ_EXT_SSPx_LVL5(i);
		ssp[i].plat_data.irq_name = irq_name_level5;
		ssp[i].plat_data.fifo[SOF_IPC_STREAM_PLAYBACK].offset =
			SSP_BASE(i) + SSDR;
		ssp[i].plat_data.fifo[SOF_IPC_STREAM_PLAYBACK].handshake =
			DMA_HANDSHAKE_SSP0_TX + 2 * i;
		ssp[i].plat_data.fifo[SOF_IPC_STREAM_CAPTURE].offset =
			SSP_BASE(i) + SSDR;
		ssp[i].plat_data.fifo[SOF_IPC_STREAM_CAPTURE].handshake =
			DMA_HANDSHAKE_SSP0_RX + 2 * i;
		/* initialize spin locks early to enable ref counting */
		spinlock_init(&ssp[i].lock);
	}
#endif
	/* init hd/a, note that size depends on the platform caps */
	for (i = 0; i < ARRAY_SIZE(hda); i++) {
		hda[i].index = i;
		hda[i].drv = &hda_driver;
		spinlock_init(&hda[i].lock);
	}

#if (CONFIG_CAVS_DMIC)
	/* init dmic */
	for (i = 0; i < ARRAY_SIZE(dmic); i++)
		spinlock_init(&dmic[i].lock);
#endif

#if CONFIG_CAVS_ALH
	for (i = 0; i < ARRAY_SIZE(alh); i++) {
		alh[i].index = (i / DAI_NUM_ALH_BI_DIR_LINKS_GROUP) << 8 |
			(i % DAI_NUM_ALH_BI_DIR_LINKS_GROUP);
		alh[i].drv = &alh_driver;

		/* set burst length to align with DMAT value in the
		 * Audio Link Hub.
		 */
		alh[i].plat_data.fifo[SOF_IPC_STREAM_PLAYBACK].depth =
			ALH_GPDMA_BURST_LENGTH;
		alh[i].plat_data.fifo[SOF_IPC_STREAM_CAPTURE].depth =
			ALH_GPDMA_BURST_LENGTH;
		spinlock_init(&alh[i].lock);
	}
#endif

	dai_install(dti, ARRAY_SIZE(dti));
	return 0;
}
