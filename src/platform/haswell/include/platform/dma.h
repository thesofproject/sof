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

#ifndef __PLATFORM_DMA_H__
#define __PLATFORM_DMA_H__

#include <stdint.h>

#define DMA_ID_DMAC0	0
#define DMA_ID_DMAC1	1

/* haswell/broadwell specific registers */
/* CTL_HI */
#define DW_CTLH_DONE					0x00001000
#define DW_CTLH_BLOCK_TS_MASK			0x00000fff
/* CFG_LO */
#define DW_CFG_CLASS(x)                (x << 5)
/* CFG_HI */
#define DW_CFGH_SRC_PER(x)		(x << 7)
#define DW_CFGH_DST_PER(x)		(x << 11)

/* default initial setup register values */
#define DW_CFG_LOW_DEF	0x0
#define DW_CFG_HIGH_DEF	0x4

#define DMA_HANDSHAKE_SSP1_RX	0
#define DMA_HANDSHAKE_SSP1_TX	1
#define DMA_HANDSHAKE_SSP0_RX	2
#define DMA_HANDSHAKE_SSP0_TX	3
#define DMA_HANDSHAKE_OBFF_0	4
#define DMA_HANDSHAKE_OBFF_1		5
#define DMA_HANDSHAKE_OBFF_2		6
#define DMA_HANDSHAKE_OBFF_3	7
#define DMA_HANDSHAKE_OBFF_4	8
#define DMA_HANDSHAKE_OBFF_5	9
#define DMA_HANDSHAKE_OBFF_6	10
#define DMA_HANDSHAKE_OBFF_7		11
#define DMA_HANDSHAKE_OBFF_8	12
#define DMA_HANDSHAKE_OBFF_9	13
#define DMA_HANDSHAKE_OBFF_10	14
#define DMA_HANDSHAKE_OBFF_11	15

#endif
