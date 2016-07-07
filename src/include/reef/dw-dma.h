/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __REEF_DW_DMA_H__
#define __REEF_DW_DMA_H__

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
