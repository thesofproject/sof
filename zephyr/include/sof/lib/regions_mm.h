/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2022 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@intel.com>
 */

/**
 * @file sof/lib/regions_mm.h
 * @brief Virtual Memory Heap (VMH) and multi-block region allocator API.
 * @ingroup memory_api
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

/** \addtogroup memory_api
 *  @{
 *
 * @note **Re-entrancy Contract**: This API is **NOT re-entrant**. While runtime
 *       allocation and deallocation operations (vmh_alloc(), vmh_free()) synchronize
 *       internally via the heap mutex, heap lifecycle management operations (such as
 *       vmh_free_heap()) do not use mutual exclusion. It is assumed that only system
 *       management code performs heap lifecycle operations, and concurrent heap
 *       management with runtime allocations must be avoided.
 */

/** @brief Region attribute for shared virtual memory heap. */
#define VIRTUAL_REGION_SHARED_HEAP_ATTR 1U
/** @brief Region attribute for loadable LLEXT libraries. */
#define VIRTUAL_REGION_LLEXT_LIBRARIES_ATTR 2U
/** @brief Region attribute for virtual page allocator. */
#define VIRTUAL_REGION_VPAGES_ATTR 3U

/**
 * @brief Maximum amount of memory block allocators in one heap.
 *
 * Since minimum realistic block is cache line size and block sizes
 * must be powers of 2 (e.g. 64, 128, 256, 512, 1024, 2048, 4096, 8192).
 */
#define MAX_MEMORY_ALLOCATORS_COUNT 10

/**
 * @brief Default number of power-of-2 block allocators partitioned across a region.
 */
#define DEFAULT_CONFIG_ALOCATORS_COUNT 5

/**
 * @brief Descriptor for a power-of-2 block size bundle in a virtual memory heap.
 */
struct vmh_block_bundle_descriptor {
	size_t block_size; /**< Size of memory block in bytes. */
	size_t number_of_blocks; /**< Number of memory blocks in this bundle. */
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
 */
struct vmh_heap_config {
	struct vmh_block_bundle_descriptor block_bundles_table[MAX_MEMORY_ALLOCATORS_COUNT];
};

/**
 * @brief Initialize a virtual memory heap with the given bundle configuration.
 * @param[in] cfg Pointer to heap bundle configuration.
 * @param[in] allocating_continuously If true, allows allocations to span a contiguous run
 *                                   of VMH block slots within a bundle; does not guarantee
 *                                   contiguous physical backing pages.
 * @return Pointer to initialized vmh_heap, or NULL on failure.
 */
struct vmh_heap *vmh_init_heap(const struct vmh_heap_config *cfg, bool allocating_continuously);

/**
 * @brief Allocate a memory block from the virtual memory heap.
 * @param[in,out] heap Pointer to target vmh_heap instance.
 * @param[in] alloc_size Allocation size in bytes.
 * @return Pointer to allocated buffer, or NULL on failure.
 */
void *vmh_alloc(struct vmh_heap *heap, uint32_t alloc_size);

/**
 * @brief Destroy a virtual memory heap and unmap all backing pages.
 * @param[in,out] heap Pointer to vmh_heap instance to destroy.
 * @return 0 on success, negative error code on failure.
 */
int vmh_free_heap(struct vmh_heap *heap);

/**
 * @brief Free a memory block previously allocated from a virtual memory heap.
 * @param[in,out] heap Pointer to vmh_heap instance.
 * @param[in,out] ptr Pointer to memory block to free.
 * @return 0 on success, negative error code on failure.
 */
int vmh_free(struct vmh_heap *heap, void *ptr);

/**
 * @brief Generate a default heap partition configuration for a memory driver region.
 * @param[in] region Driver region describing base and size.
 * @param[out] cfg Destination configuration structure to populate.
 */
void vmh_get_default_heap_config(const struct sys_mm_drv_region *region,
		struct vmh_heap_config *cfg);
#ifdef CONFIG_SYS_MEM_BLOCKS_RUNTIME_STATS
/**
 * @brief Log aggregate runtime statistics for the virtual memory heap.
 * @param[in] heap Pointer to vmh_heap instance.
 */
void vmh_log_stats(struct vmh_heap *heap);

/**
 * @brief Log per-bundle block allocation statistics for the virtual memory heap.
 * @param[in] heap Pointer to vmh_heap instance.
 */
void vmh_log_alloc_stats(struct vmh_heap *heap);
#endif
/**
 * @brief Checks if ptr is in range of given memory range.
 *
 * @param[in] ptr Checked pointer address.
 * @param[in] range_start Start of checked memory range.
 * @param[in] range_size Size of checked memory range in bytes.
 *
 * @retval false if no overlap detected.
 * @retval true if any overlap detected.
 */
static inline bool vmh_is_ptr_in_memory_range(uintptr_t ptr, uintptr_t range_start,
	size_t range_size)
{
	return ptr >= range_start && ptr < range_start + range_size;
}

/** @}*/

#endif /* ZEPHYR_LIB_REGIONS_MM_H_ */
