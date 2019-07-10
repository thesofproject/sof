/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __PLATFORM_DMA_H__
#define __PLATFORM_DMA_H__

#include <stdint.h>
#include <sof/dma.h>

#define PLATFORM_NUM_DMACS	2

#define DMA_ID_EDMA0	0
#define DMA_ID_HOST	1

int dmac_init(void);

#endif /* __PLATFORM_DMA_H__ */
