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
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_DW_DMA_H__
#define __SOF_DW_DMA_H__

#include <stdint.h>

#define DW_DMA_MAX_NR_CHANNELS	8

/* TODO: add FIFO sizes */
struct dw_chan_data {
	uint16_t class;
	uint16_t weight;
};

struct dw_drv_plat_data {
	struct dw_chan_data chan[DW_DMA_MAX_NR_CHANNELS];
};

/* DMA descriptor used by the HW version 1 */
struct dw_lli1 {
	uint32_t sar;
	uint32_t dar;
	uint32_t llp;
	uint32_t ctrl_lo;
	uint32_t ctrl_hi;
} __attribute__ ((packed));

/* DMA descriptor used by HW version 2 */
struct dw_lli2 {
	uint32_t sar;
	uint32_t dar;
	uint32_t llp;
	uint32_t ctrl_lo;
	uint32_t ctrl_hi;
	uint32_t sstat;
	uint32_t dstat;
} __attribute__ ((packed));

extern const struct dma_ops dw_dma_ops;

#endif
