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
 * Author: Zhigang.wu <zhigang.wu@linux.intel.com>
 */

#include <sof/sof.h>
#include <platform/interrupt.h>
#include <platform/memory.h>
#include <platform/dma.h>
#include <sof/spi.h>
#include <string.h>
#include <config.h>

static struct spi spi[] = {
{
	.type  = SOF_SPI_INTEL_SLAVE,
	.index = 0,
	.plat_data = {
		.base		= SPI_BASE,
		.irq		= IRQ_EXT_LP_GPDMA0_LVL5(0, 0),
		.fifo[SPI_TYPE_INTEL_RECEIVE] = {
			.offset		= DR,
			.handshake	= DMA_HANDSHAKE_SPI_RX,
		},
		.fifo[SPI_TYPE_INTEL_TRANSMIT] = {
			.offset		= DR,
			.handshake	= DMA_HANDSHAKE_SPI_TX,
		}
	},
	.ops = &spi_ops,
},
};

struct spi *spi_get(uint32_t type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(spi); i++) {
		if (spi[i].type == type)
			return &spi[i];
	}

	return NULL;
}
