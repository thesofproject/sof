/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_DRIVERS_DW_DMA_H__
#define __SOF_DRIVERS_DW_DMA_H__

#include <platform/drivers/dw-dma.h>
#include <rtos/bit.h>
#include <sof/common.h>
#include <sof/lib/dma.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

/* channel registers */
#define DW_MAX_CHAN		8
#define DW_FIFO_SIZE		0x80
#define DW_CHAN_SIZE		0x58
#define DW_CHAN_OFFSET(chan)	(DW_CHAN_SIZE * (chan))

#define DW_SAR(chan)		(0x00 + DW_CHAN_OFFSET(chan))
#define DW_DAR(chan)		(0x08 + DW_CHAN_OFFSET(chan))
#define DW_LLP(chan)		(0x10 + DW_CHAN_OFFSET(chan))
#define DW_CTRL_LOW(chan)	(0x18 + DW_CHAN_OFFSET(chan))
#define DW_CTRL_HIGH(chan)	(0x1C + DW_CHAN_OFFSET(chan))
#define DW_CFG_LOW(chan)	(0x40 + DW_CHAN_OFFSET(chan))
#define DW_CFG_HIGH(chan)	(0x44 + DW_CHAN_OFFSET(chan))
#define DW_DSR(chan)		(0x50 + DW_CHAN_OFFSET(chan))

/* common registers */
#define DW_RAW_TFR		0x2C0
#define DW_RAW_BLOCK		0x2C8
#define DW_RAW_SRC_TRAN		0x2D0
#define DW_RAW_DST_TRAN		0x2D8
#define DW_RAW_ERR		0x2E0
#define DW_STATUS_TFR		0x2E8
#define DW_STATUS_BLOCK		0x2F0
#define DW_STATUS_SRC_TRAN	0x2F8
#define DW_STATUS_DST_TRAN	0x300
#define DW_STATUS_ERR		0x308
#define DW_MASK_TFR		0x310
#define DW_MASK_BLOCK		0x318
#define DW_MASK_SRC_TRAN	0x320
#define DW_MASK_DST_TRAN	0x328
#define DW_MASK_ERR		0x330
#define DW_CLEAR_TFR		0x338
#define DW_CLEAR_BLOCK		0x340
#define DW_CLEAR_SRC_TRAN	0x348
#define DW_CLEAR_DST_TRAN	0x350
#define DW_CLEAR_ERR		0x358
#define DW_INTR_STATUS		0x360
#define DW_DMA_CFG		0x398
#define DW_DMA_CHAN_EN		0x3A0
#define DW_FIFO_PART0_LO	0x400
#define DW_FIFO_PART0_HI	0x404
#define DW_FIFO_PART1_LO	0x408
#define DW_FIFO_PART1_HI	0x40C

/* channel bits */
#define DW_CHAN_WRITE_EN_ALL	MASK(2 * DW_MAX_CHAN - 1, DW_MAX_CHAN)
#define DW_CHAN_WRITE_EN(chan)	BIT((chan) + DW_MAX_CHAN)
#define DW_CHAN_ALL		MASK(DW_MAX_CHAN - 1, 0)
#define DW_CHAN(chan)		BIT(chan)
#define DW_CHAN_MASK_ALL	DW_CHAN_WRITE_EN_ALL
#define DW_CHAN_MASK(chan)	DW_CHAN_WRITE_EN(chan)
#define DW_CHAN_UNMASK_ALL	(DW_CHAN_WRITE_EN_ALL | DW_CHAN_ALL)
#define DW_CHAN_UNMASK(chan)	(DW_CHAN_WRITE_EN(chan) | DW_CHAN(chan))

/* CFG_LO */
#define DW_CFGL_DRAIN		BIT(10)
#define DW_CFGL_FIFO_EMPTY	BIT(9)
#define DW_CFGL_SUSPEND		BIT(8)

/* CTL_LO */
#define DW_CTLL_RELOAD_DST	BIT(31)
#define DW_CTLL_RELOAD_SRC	BIT(30)
#define DW_CTLL_LLP_S_EN	BIT(28)
#define DW_CTLL_LLP_D_EN	BIT(27)
#define DW_CTLL_SMS(x)		SET_BIT(25, x)
#define DW_CTLL_DMS(x)		SET_BIT(23, x)
#define DW_CTLL_FC_P2P		SET_BITS(21, 20, 3)
#define DW_CTLL_FC_P2M		SET_BITS(21, 20, 2)
#define DW_CTLL_FC_M2P		SET_BITS(21, 20, 1)
#define DW_CTLL_FC_M2M		SET_BITS(21, 20, 0)
#define DW_CTLL_D_SCAT_EN	BIT(18)
#define DW_CTLL_S_GATH_EN	BIT(17)
#define DW_CTLL_SRC_MSIZE(x)	SET_BITS(16, 14, x)
#define DW_CTLL_DST_MSIZE(x)	SET_BITS(13, 11, x)
#define DW_CTLL_SRC_FIX		SET_BITS(10, 9, 2)
#define DW_CTLL_SRC_DEC		SET_BITS(10, 9, 1)
#define DW_CTLL_SRC_INC		SET_BITS(10, 9, 0)
#define DW_CTLL_DST_FIX		SET_BITS(8, 7, 2)
#define DW_CTLL_DST_DEC		SET_BITS(8, 7, 1)
#define DW_CTLL_DST_INC		SET_BITS(8, 7, 0)
#define DW_CTLL_SRC_WIDTH(x)	SET_BITS(6, 4, x)
#define DW_CTLL_DST_WIDTH(x)	SET_BITS(3, 1, x)
#define DW_CTLL_INT_EN		BIT(0)
#define DW_CTLL_SRC_WIDTH_MASK	MASK(6, 4)
#define DW_CTLL_SRC_WIDTH_SHIFT	4
#define DW_CTLL_DST_WIDTH_MASK	MASK(3, 1)
#define DW_CTLL_DST_WIDTH_SHIFT	1

/* DSR */
#define DW_DSR_DSC(x)		SET_BITS(31, 20, x)
#define DW_DSR_DSI(x)		SET_BITS(19, 0, x)

/* FIFO_PART */
#define DW_FIFO_UPD		BIT(26)
#define DW_FIFO_CHx(x)		SET_BITS(25, 13, x)
#define DW_FIFO_CHy(x)		SET_BITS(12, 0, x)

/* number of tries to wait for reset */
#define DW_DMA_CFG_TRIES	10000

/* channel drain timeout in microseconds */
#define DW_DMA_TIMEOUT	1333

/* min number of elems for config with irq disabled */
#define DW_DMA_CFG_NO_IRQ_MIN_ELEMS	3

/* linked list item address */
#define DW_DMA_LLI_ADDRESS(lli, dir) \
	(((dir) == DMA_DIR_MEM_TO_DEV) ? ((lli)->sar) : ((lli)->dar))

#define DW_DMA_BUFFER_ALIGNMENT 0x4
#define DW_DMA_COPY_ALIGNMENT	0x4

/* TODO: add FIFO sizes */
struct dw_chan_data {
	uint16_t class;
	uint16_t weight;
};

struct dw_drv_plat_data {
	struct dw_chan_data chan[DW_MAX_CHAN];
};

/* DMA descriptor used by HW */
struct dw_lli {
	uint32_t sar;
	uint32_t dar;
	uint32_t llp;
	uint32_t ctrl_lo;
	uint32_t ctrl_hi;
	uint32_t sstat;
	uint32_t dstat;

	/* align to 32 bytes to not cross cache line
	 * in case of more than two items
	 */
	uint32_t reserved;
} __packed;

extern const struct dma_ops dw_dma_ops;

#endif /* __SOF_DRIVERS_DW_DMA_H__ */
