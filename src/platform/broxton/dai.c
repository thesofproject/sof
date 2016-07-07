/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/reef.h>
#include <reef/dai.h>
#include <reef/ssp.h>
#include <reef/stream.h>
#include <reef/audio/component.h>
#include <platform/memory.h>
#include <platform/interrupt.h>
#include <platform/dma.h>
#include <stdint.h>
#include <string.h>

static struct dai ssp[4] = {
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 0,
	.plat_data = {
		.base		= SSP_BASE(0),
		.irq		= IRQ_EXT_SSP0_LVL5(0),
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP_BASE(0) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP0_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP_BASE(0) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP0_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 1,
	.plat_data = {
		.base		= SSP_BASE(1),
		.irq		= IRQ_EXT_SSP1_LVL5(0),
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP_BASE(1) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP1_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP_BASE(1) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP1_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 2,
	.plat_data = {
		.base		= SSP_BASE(2),
		.irq		= IRQ_EXT_SSP2_LVL5(0),
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP_BASE(2) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP2_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP_BASE(2) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP2_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 3,
	.plat_data = {
		.base		= SSP_BASE(3),
		.irq		= IRQ_EXT_SSP3_LVL5(0),
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP_BASE(3) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP3_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP_BASE(3) + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP3_RX,
		}
	},
	.ops		= &ssp_ops,
},};

struct dai *dai_get(uint32_t type, uint32_t index)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ssp); i++) {
		if (ssp[i].type == type && ssp[i].index == index)
			return &ssp[i];
	}

	return NULL;
}
