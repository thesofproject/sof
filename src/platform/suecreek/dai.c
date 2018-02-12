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
#include <config.h>

static struct dai ssp[4] = {
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
