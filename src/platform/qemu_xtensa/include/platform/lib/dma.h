/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#ifndef __PLATFORM_LIB_DMA_H__
#define __PLATFORM_LIB_DMA_H__

/* Dummy dma header for qemu_xtensa */
struct dma;

struct sof_dma {
	const struct device *z_dev;
};

#endif /* __PLATFORM_LIB_DMA_H__ */
