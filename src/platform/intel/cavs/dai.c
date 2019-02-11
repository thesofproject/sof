/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#include <sof/sof.h>
#include <sof/dai.h>
#include <sof/ssp.h>
#include <sof/dmic.h>
#include <sof/hda.h>
#include <sof/stream.h>
#include <sof/audio/component.h>
#include <platform/platform.h>
#include <platform/memory.h>
#include <platform/interrupt.h>
#include <platform/dma.h>
#include <platform/dai.h>
#include <stdint.h>
#include <string.h>
#include <config.h>

static struct dai ssp[(DAI_NUM_SSP_BASE + DAI_NUM_SSP_EXT)];

#if defined CONFIG_CAVS_DMIC

static struct dai dmic[2] = {
	/* Testing idea if DMIC FIFOs A and B to access the same microphones
	 * with two different sample rate and PCM format could be presented
	 * similarly as SSP0..N. The difference however is that the DMIC
	 * programming is global and not per FIFO.
	 */

	/* Primary FIFO A */
	{
		.type = SOF_DAI_INTEL_DMIC,
		.index = 0,
		.plat_data = {
			.base = DMIC_BASE,
			.irq = IRQ_EXT_DMIC_LVL5(0, 0),
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
				.offset = 0, /* No playback */
				.handshake = 0,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
				.offset = DMIC_BASE + OUTDATA0,
				.handshake = DMA_HANDSHAKE_DMIC_CH0,
			}
		},
		.ops = &dmic_ops,
	},
	/* Secondary FIFO B */
	{
		.type = SOF_DAI_INTEL_DMIC,
		.index = 1,
		.plat_data = {
			.base = DMIC_BASE,
			.irq = IRQ_EXT_DMIC_LVL5(1, 0),
			.fifo[SOF_IPC_STREAM_PLAYBACK] = {
				.offset = 0, /* No playback */
				.handshake = 0,
			},
			.fifo[SOF_IPC_STREAM_CAPTURE] = {
				.offset = DMIC_BASE + OUTDATA1,
				.handshake = DMA_HANDSHAKE_DMIC_CH1,
			}
		},
		.ops = &dmic_ops,
	}
};

#endif

static struct dai hda[(DAI_NUM_HDA_OUT + DAI_NUM_HDA_IN)];

static struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_INTEL_SSP,
		.dai_array = ssp,
		.num_dais = ARRAY_SIZE(ssp)
	},
#if defined CONFIG_CAVS_DMIC
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
	}
};

int dai_init(void)
{
	int i;

	/* init ssp */
	for (i = 0; i < ARRAY_SIZE(ssp); i++) {
		ssp[i].type = SOF_DAI_INTEL_SSP;
		ssp[i].index = i;
		ssp[i].ops = &ssp_ops;
		ssp[i].plat_data.base = SSP_BASE(i);
		ssp[i].plat_data.irq = IRQ_EXT_SSPx_LVL5(i, 0);
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

	/* init hd/a, note that size depends on the platform caps */
	for (i = 0; i < ARRAY_SIZE(hda); i++) {
		hda[i].type = SOF_DAI_INTEL_HDA;
		hda[i].index = i;
		hda[i].ops = &hda_ops;
		spinlock_init(&hda[i].lock);
	}

#if defined(CONFIG_CAVS_DMIC)
	/* init dmic */
	for (i = 0; i < ARRAY_SIZE(dmic); i++)
		spinlock_init(&dmic[i].lock);
#endif
	dai_install(dti, ARRAY_SIZE(dti));
	return 0;
}
