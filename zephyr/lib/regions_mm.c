// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@intel.com>
 */

#include <sof/lib/regions_mm.h>

struct virtual_memory_heap
	vm_heaps[CONFIG_MP_MAX_NUM_CPUS + VIRTUAL_REGION_COUNT];

/**
 * @brief Fill vm_heaps array with information from zephyr.
 *
 * Virtual memory regions calculated in zephyr are translated here
 * to a struct that will keep all information on current allocations
 * and virtual to physical mappings that are related to heaps.
 * System heap is not a part of this information. It only refers to
 * virtual first heaps.
 * Has to be initialized after calculations for regions is done in zephyr.
 */
static int virtual_heaps_init(void)
{
	struct sys_mm_drv_region *virtual_memory_regions =
		(struct sys_mm_drv_region *)sys_mm_drv_query_memory_regions();

	for (size_t i = 0;
		i < CONFIG_MP_MAX_NUM_CPUS + VIRTUAL_REGION_COUNT;
		i++) {
		vm_heaps[i].virtual_region = &virtual_memory_regions[i];

		switch (virtual_memory_regions[i].attr)	{
		case MEM_REG_ATTR_CORE_HEAP:
			vm_heaps[i].memory_caps =
				SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_HP | SOF_MEM_CAPS_CACHE;
			break;
		case MEM_REG_ATTR_SHARED_HEAP:
			vm_heaps[i].memory_caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_HP;
			break;
		case MEM_REG_ATTR_OPPORTUNISTIC_MEMORY:
			vm_heaps[i].memory_caps = SOF_MEM_CAPS_RAM | SOF_MEM_CAPS_HP;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

SYS_INIT(virtual_heaps_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
