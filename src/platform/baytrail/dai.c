/*
 * Copyright (c) 2016, Intel Corporation
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

static struct dai ssp[] = {
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 0,
	.plat_data = {
		.base		= SSP0_BASE,
		.irq		= IRQ_NUM_EXT_SSP0,
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP0_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP0_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP0_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP0_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 1,
	.plat_data = {
		.base		= SSP1_BASE,
		.irq		= IRQ_NUM_EXT_SSP1,
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP1_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP1_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP1_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP1_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 2,
	.plat_data = {
		.base		= SSP2_BASE,
		.irq		= IRQ_NUM_EXT_SSP2,
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP2_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP2_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP2_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP2_RX,
		}
	},
	.ops		= &ssp_ops,
},
#if defined CONFIG_CHERRYTRAIL
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 3,
	.plat_data = {
		.base		= SSP3_BASE,
		.irq		= IRQ_NUM_EXT_SSP0,
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP3_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP3_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP0_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP3_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 4,
	.plat_data = {
		.base		= SSP4_BASE,
		.irq		= IRQ_NUM_EXT_SSP1,
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP4_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP4_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP4_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP4_RX,
		}
	},
	.ops		= &ssp_ops,
},
{
	.type = COMP_TYPE_DAI_SSP,
	.index = 5,
	.plat_data = {
		.base		= SSP5_BASE,
		.irq		= IRQ_NUM_EXT_SSP2,
		.fifo[STREAM_DIRECTION_PLAYBACK] = {
			.offset		= SSP5_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP5_TX,
		},
		.fifo[STREAM_DIRECTION_CAPTURE] = {
			.offset		= SSP5_BASE + SSDR,
			.handshake	= DMA_HANDSHAKE_SSP5_RX,
		}
	},
	.ops		= &ssp_ops,
},
#endif
};

struct dai *dai_get(uint32_t type, uint32_t index)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ssp); i++) {
		if (ssp[i].type == type && ssp[i].index == index)
			return &ssp[i];
	}

	return NULL;
}
