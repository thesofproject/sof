/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2022 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@intel.com>
 */

#ifndef ZEPHYR_LIB_REGIONS_MM_H_
#define ZEPHYR_LIB_REGIONS_MM_H_

#include <adsp_memory.h>
#include <adsp_memory_regions.h>
#include <ipc/topology.h>
#include <rtos/alloc.h>
#include <sof/list.h>
#include <zephyr/drivers/mm/system_mm.h>
#include <zephyr/init.h>
#include <zephyr/sys/mem_blocks.h>

/* Dependency on ipc/topology.h created due to memory capability definitions
 * that are defined there
 */

/* API is NOT re-entry safe.
 * This is due to our assumption that only management code will ever handle
 * memory operations on heaps themselves.
 */

/* Defines maximum amount of memory block allocators in one heap
 * since minimum realistic block should be cache line size
 * and block sizes in allocator must be powers of 2
 * so logically it gives limited block sizes
 * eg allocator of block size 64 128 256 512 2048 4096 1024 8192
 * would end up in 8 allocators.
 * It is expected that allocations bigger than that would
 * either be spanned on specifically configured heap or have
 * individual configs with bigger block sizes.
 */
#define MAX_MEMORY_ALLOCATORS_COUNT 10

/* vmh_get_default_heap_config() function will try to split the region
 * down the given count. Only applicable when API client did not
 * use its config.
 */
#define DEFAULT_CONFIG_ALOCATORS_COUNT 5

/** @struct vmh_block_bundle_descriptor
 *
 *  @brief This is a struct describing one bundle of blocks
 *  used as base for allocators blocks.
 *
 *  @var block_size size of memory block.
 *  @var number_of_blocks number of memory blocks.
 */
struct vmh_block_bundle_descriptor {
	size_t block_size;
	size_t number_of_blocks;
};

/*
 * Maybe this heap config should have small bundles first going up to max
 * size or there should be a sorting mechanism for those ?
 * Should we assume that bundles are given from smallest to biggest ?
 */

/** @struct vmh_heap_config
 *
 *  @brief This is a struct that aggregates vmh_block_bundle_descriptor to
 *  create one cfg that can be passed to heap initiation.
 *  Provided config size must be physical page aligned so it
 *  will not overlap in physical space with other heaps during mapping.
 *  So every block has to have its overall size aligned to CONFIG_MM_DRV_PAGE_SIZE
 *
 *  @vmh_block_bundle_descriptor[] aggregation of bundle descriptors.
 */
struct vmh_heap_config {
	struct vmh_block_bundle_descriptor block_bundles_table[MAX_MEMORY_ALLOCATORS_COUNT];
};

struct vmh_heap *vmh_init_heap(const struct vmh_heap_config *cfg,
		int memory_region_attribute, int core_id, bool allocating_continuously);
void *vmh_alloc(struct vmh_heap *heap, uint32_t alloc_size);
int vmh_free_heap(struct vmh_heap *heap);
int vmh_free(struct vmh_heap *heap, void *ptr);
struct vmh_heap *vmh_reconfigure_heap(struct vmh_heap *heap,
		struct vmh_heap_config *cfg, int core_id, bool allocating_continuously);
void vmh_get_default_heap_config(const struct sys_mm_drv_region *region,
		struct vmh_heap_config *cfg);
struct vmh_heap *vmh_get_heap_by_attribute(uint32_t attr, uint32_t core_id);
/**
 * @brief Checks if ptr is in range of given memory range
 *
 * @param ptr checked ptr
 * @param range_start start of checked memory range
 * @param range_size size of checked memory range.
 *
 * @retval false if no overlap detected.
 * @retval true if any overlap detected.
 */
static inline bool vmh_is_ptr_in_memory_range(uintptr_t ptr, uintptr_t range_start,
	size_t range_size)
{
	return ptr >= range_start && ptr < range_start + range_size;
}

#endif /* ZEPHYR_LIB_REGIONS_MM_H_ */
