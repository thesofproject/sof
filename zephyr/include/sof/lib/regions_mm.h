/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@intel.com>
 */

#ifndef ZEPHYR_LIB_REGIONS_MM_H_
#define ZEPHYR_LIB_REGIONS_MM_H_

#include <adsp_memory.h>
#include <adsp_memory_regions.h>
#include <zephyr/drivers/mm/system_mm.h>
#include <zephyr/sys/mem_blocks.h>
#include <zephyr/init.h>
#include <ipc/topology.h>

/*
 * Struct containing information on virtual memory heap.
 * Information about allocated physical blocks is stored in
 * physical_blocks_allocators variable and uses zephyr memblocks api.
 */
struct virtual_memory_heap {
	/* zephyr provided virtual region */
	struct sys_mm_drv_region *virtual_region;
	/* physical pages allocators represented in memblocks */
	struct sys_multi_mem_blocks physical_blocks_allocators;
	/* SOF memory capability */
	uint32_t memory_caps;
};

/* Available externaly array containing all information on virtual heaps
 * Used to control physical allocations and overall virtual to physicall
 * mapping on sof side (zephyr handles the actual assigning physical memory
 * sof only requests it).
 */
extern struct virtual_memory_heap
	vm_heaps[CONFIG_MP_MAX_NUM_CPUS + VIRTUAL_REGION_COUNT];

#endif /* ZEPHYR_LIB_REGIONS_MM_H_ */
