// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <sof/dai.h>
#include <sof/dma.h>
#include <sof/drivers/interrupt.h>
#include <sof/memory.h>
#include <sof/ssp.h>
#include <ipc/dai.h>
#include <ipc/stream.h>
#include <config.h>

static struct dai ssp[] = {
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
{
	.index = 2,
	.plat_data = {
		.base		= SSP2_BASE,
		.irq		= IRQ_NUM_EXT_SSP2,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP2_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP2_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP2_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP2_RX,
		}
	},
	.drv		= &ssp_driver,
},
#if defined CONFIG_CHERRYTRAIL
{
	.index = 3,
	.plat_data = {
		.base		= SSP3_BASE,
		.irq		= IRQ_NUM_EXT_SSP0,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP3_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP3_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP0_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP3_RX,
		}
	},
	.drv		= &ssp_driver,
},
{
	.index = 4,
	.plat_data = {
		.base		= SSP4_BASE,
		.irq		= IRQ_NUM_EXT_SSP1,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP4_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP4_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP4_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP4_RX,
		}
	},
	.drv		= &ssp_driver,
},
{
	.index = 5,
	.plat_data = {
		.base		= SSP5_BASE,
		.irq		= IRQ_NUM_EXT_SSP2,
		.fifo[SOF_IPC_STREAM_PLAYBACK] = {
			.offset		= SSP5_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP5_TX,
		},
		.fifo[SOF_IPC_STREAM_CAPTURE] = {
			.offset		= SSP5_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP5_RX,
		}
	},
	.drv		= &ssp_driver,
},
#endif
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
