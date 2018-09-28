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

/* TODO: ops should be declared by their respective dai headers */
extern const struct dai_ops hda_ops;

static struct dai ssp[] = {
{
	.type = SOF_DAI_INTEL_SSP,
	.index = 0,
	.plat_data = {
		.base		= SSP_BASE(0),
		.irq		= IRQ_EXT_SSP0_LVL5(0),
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP_BASE(0) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP0_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP_BASE(0) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP0_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = SOF_DAI_INTEL_SSP,
	.index = 1,
	.plat_data = {
		.base		= SSP_BASE(1),
		.irq		= IRQ_EXT_SSP1_LVL5(0),
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP_BASE(1) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP1_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP_BASE(1) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP1_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = SOF_DAI_INTEL_SSP,
	.index = 2,
	.plat_data = {
		.base		= SSP_BASE(2),
		.irq		= IRQ_EXT_SSP2_LVL5(0),
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP_BASE(2) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP2_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP_BASE(2) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP2_RX,
		}
	},
	.ops		= &ssp_ops,
},
#if defined(CONFIG_APOLLOLAKE)
{
	.type = SOF_DAI_INTEL_SSP,
	.index = 3,
	.plat_data = {
		.base		= SSP_BASE(3),
		.irq		= IRQ_EXT_SSP3_LVL5(0),
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP_BASE(3) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP3_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP_BASE(3) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP3_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = SOF_DAI_INTEL_SSP,
	.index = 4,
	.plat_data = {
		.base		= SSP_BASE(4),
		.irq		= IRQ_EXT_SSP4_LVL5(0),
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset 	= SSP_BASE(4) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP4_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset 	= SSP_BASE(4) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP4_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = SOF_DAI_INTEL_SSP,
	.index = 5,
	.plat_data = {
		.base		= SSP_BASE(5),
		.irq		= IRQ_EXT_SSP5_LVL5(0),
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset 	= SSP_BASE(5) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP5_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset 	= SSP_BASE(5) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP5_RX,
		}
	},
	.ops		= &ssp_ops,
},
#endif
};

#if defined CONFIG_DMIC

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
			.irq = IRQ_EXT_DMIC_LVL5(0),
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
			.irq = IRQ_EXT_DMIC_LVL5(0),
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

/* TODO: numbers to be defined and shared with HD/A dmas */
static struct dai hda[(6 + 7)] = {
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 0,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 1,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 2,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 3,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 4,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 5,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 6,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 7,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 8,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 9,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 10,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 11,
		.ops = &hda_ops
	},
	{
		.type = SOF_DAI_INTEL_HDA,
		.index = 12,
		.ops = &hda_ops
	}
};

static struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_INTEL_SSP,
		.dai_array = ssp,
		.num_dais = ARRAY_SIZE(ssp)
	},
#if defined CONFIG_DMIC
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

	/* init SSP ports */
	trace_point(TRACE_BOOT_PLATFORM_SSP);
	for (i = 0; i < DAI_NUM_SSP_BASE + DAI_NUM_SSP_EXT; i++)
		dai_probe(ssp + i);

	/* Init DMIC. Note that the two PDM controllers and four microphones
	 * supported max. those are available in platform are handled by dmic0.
	 */
	trace_point(TRACE_BOOT_PLATFORM_DMIC);

	dai_probe(dmic + 0);

	dai_install(dti, ARRAY_SIZE(dti));
	return 0;
}
