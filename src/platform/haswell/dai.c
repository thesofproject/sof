// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/sof.h>
#include <sof/dai.h>
#include <sof/ssp.h>
#include <sof/stream.h>
#include <sof/audio/component.h>
#include <sof/memory.h>
#include <sof/drivers/interrupt.h>
#include <sof/dma.h>
#include <stdint.h>
#include <sof/string.h>
#include <config.h>

static struct dai ssp[2] = {
{
	.index = 0,
	.plat_data = {
		.base		= SSP0_BASE,
		.irq		= IRQ_NUM_EXT_SSP0,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP0_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP0_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP0_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP0_RX,
		}
	},
	.drv		= &ssp_driver,
},
{
	.index = 1,
	.plat_data = {
		.base		= SSP1_BASE,
		.irq		= IRQ_NUM_EXT_SSP1,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP1_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP1_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP1_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP1_RX,
		}
	},
	.drv		= &ssp_driver,
},
};

static struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_INTEL_SSP,
		.dai_array = ssp,
		.num_dais = ARRAY_SIZE(ssp)
	}
};

int dai_init(void)
{
	dai_install(dti, ARRAY_SIZE(dti));
	return 0;
}
