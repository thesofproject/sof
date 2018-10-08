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

#include <sof/sof.h>
#include <sof/dai.h>
#include <sof/ssp.h>
#include <sof/stream.h>
#include <sof/audio/component.h>
#include <platform/memory.h>
#include <platform/interrupt.h>
#include <platform/dma.h>
#include <platform/dai.h>
#include <stdint.h>
#include <string.h>
#include <config.h>

static struct dai ssp[2] = {
{
	.type = SOF_DAI_INTEL_SSP,
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
	.ops		= &ssp_ops,
},
{
	.type = SOF_DAI_INTEL_SSP,
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
	.ops		= &ssp_ops,
}};

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
