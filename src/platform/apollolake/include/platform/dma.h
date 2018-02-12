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

#ifndef __PLATFORM_DMA_H__
#define __PLATFORM_DMA_H__

#include <stdint.h>
#include <reef/io.h>
#include <arch/cache.h>

/* available DMACs */
#define DMA_GP_LP_DMAC0		0
#define DMA_GP_LP_DMAC1		1
#define DMA_GP_HP_DMAC0		2
#define DMA_GP_HP_DMAC1		3
#define DMA_HOST_IN_DMAC	4
#define DMA_HOST_OUT_DMAC	5
#define DMA_LINK_IN_DMAC	6
#define DMA_LINK_OUT_DMAC	7

/* apollolake specific registers */
/* Gateway Stream Registers */
#define DGCS		0x00
#define DGBBA	0x04
#define DGBS		0x08
#define DGBFPI	0x0c /* firmware need to update this when DGCS.FWCB=1 */
#define DGBRP	0x10 /* Read Only, read pointer */
#define DGBWP	0x14 /* Read Only, write pointer */
#define DGBSP	0x18
#define DGMBS	0x1c
#define DGLLPI	0x24
#define DGLPIBI	0x28

/* DGCS */
#define DGCS_GEN		(1 << 26)
#define DGCS_BSC			(1 << 11)
#define DGCS_BF                        (1 << 9) /* buffer full */
#define DGCS_BNE                        (1 << 8) /* buffer not empty */

/* DGBBA */
#define DGBBA_MASK		0xffff80

/* DGBS */
#define DGBS_MASK		0xfffff0


/* apollolake specific registers */
/* CTL_LO */
#define DW_CTLL_S_GATH_EN		(1 << 17)
#define DW_CTLL_D_SCAT_EN		(1 << 18)
/* CTL_HI */
#define DW_CTLH_DONE			0x00020000
#define DW_CTLH_BLOCK_TS_MASK		0x0001ffff
#define DW_CTLH_CLASS(x)		(x << 29)
#define DW_CTLH_WEIGHT(x)		(x << 18)
/* CFG_LO */
#define DW_CFG_CH_DRAIN			0x400
/* CFG_HI */
#define DW_CFGH_SRC_PER(x)		(x << 0)
#define DW_CFGH_DST_PER(x)		(x << 4)
/* FIFO Partition */
#define DW_FIFO_PARTITION
#define DW_FIFO_PART0_LO		0x0400
#define DW_FIFO_PART0_HI		0x0404
#define DW_FIFO_PART1_LO		0x0408
#define DW_FIFO_PART1_HI		0x040C
#define DW_CH_SAI_ERR			0x0410
#define DW_DMA_GLB_CFG		0x0418

/* default initial setup register values */
#define DW_CFG_LOW_DEF	0x00000003
#define DW_CFG_HIGH_DEF	0x0

#define DW_REG_MAX			DW_DMA_GLB_CFG

/* mappings - TODO improve API to get type */
#define DMA_ID_DMAC0	DMA_HOST_IN_DMAC
#define DMA_ID_DMAC1	DMA_GP_LP_DMAC0
#define DMA_ID_DMAC2	DMA_HOST_OUT_DMAC
#define DMA_ID_DMAC3	DMA_GP_HP_DMAC0
#define DMA_ID_DMAC4	DMA_GP_LP_DMAC1
#define DMA_ID_DMAC5	DMA_GP_HP_DMAC1
#define DMA_ID_DMAC6	DMA_LINK_IN_DMAC
#define DMA_ID_DMAC7	DMA_LINK_OUT_DMAC

/* handshakes */
#define DMA_HANDSHAKE_DMIC_CH0	0
#define DMA_HANDSHAKE_DMIC_CH1	1
#define DMA_HANDSHAKE_SSP0_TX	2
#define DMA_HANDSHAKE_SSP0_RX	3
#define DMA_HANDSHAKE_SSP1_TX	4
#define DMA_HANDSHAKE_SSP1_RX	5
#define DMA_HANDSHAKE_SSP2_TX	6
#define DMA_HANDSHAKE_SSP2_RX	7
#define DMA_HANDSHAKE_SSP3_TX	8
#define DMA_HANDSHAKE_SSP3_RX	9
#define DMA_HANDSHAKE_SSP4_TX	10
#define DMA_HANDSHAKE_SSP4_RX	11
#define DMA_HANDSHAKE_SSP5_TX	12
#define DMA_HANDSHAKE_SSP5_RX	13

/* DMA descriptor used by HW version 2 */
struct host_dma_config {
	uint32_t cs; /* DSP Gateway Control & Status */
	uint32_t bba; /* Buffer Base Address */
	uint32_t bs; /* Buffer Size */
	uint32_t bfpi; /* Buffer Firmware Pointer Increment */
	uint32_t bsp; /* Buffer Segment Pointer */
	uint32_t mbs; /* minimum Buffer Size, in samples */
	uint32_t llpi; /* linear link position increment */
	uint32_t lpibi; /* link position in buffer increment */
} __attribute__ ((packed));

static inline uint32_t host_dma_reg_read(uint32_t is_out,
	uint32_t stream_id, uint32_t reg)
{
	uint32_t base;

	base = is_out ? \
		GTW_HOST_OUT_STREAM_BASE(stream_id) :
		GTW_HOST_IN_STREAM_BASE(stream_id);

	/* invalidate cached reg setting */
	dcache_invalidate_region((void *)(base + reg), sizeof(uint32_t));

	return io_reg_read(base + reg);
}

static inline void host_dma_reg_write(uint32_t is_out,
	uint32_t stream_id, uint32_t reg, uint32_t val)
{
	uint32_t base;

	base = is_out ? \
		GTW_HOST_OUT_STREAM_BASE(stream_id) :
		GTW_HOST_IN_STREAM_BASE(stream_id);

	io_reg_write(base + reg, val);

/* writeback reg setting */
dcache_writeback_region((void *)(base + reg), sizeof(uint32_t));

}

#endif
