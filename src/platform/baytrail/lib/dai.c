// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/ssp.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <ipc/dai.h>
#include <ipc/stream.h>


static SHARED_DATA struct dai ssp[] = {
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

const struct dai_type_info dti[] = {
	{
		.type = SOF_DAI_INTEL_SSP,
		.dai_array = ssp,
		.num_dais = ARRAY_SIZE(ssp)
	}
};

const struct dai_info lib_dai = {
	.dai_type_array = dti,
	.num_dai_types = ARRAY_SIZE(dti)
};

int dai_init(struct sof *sof)
{
	int i;

	/* initialize spin locks early to enable ref counting */
	for (i = 0; i < ARRAY_SIZE(ssp); i++)
		spinlock_init(&ssp[i].lock);


	sof->dai_info = &lib_dai;

	return 0;
}
