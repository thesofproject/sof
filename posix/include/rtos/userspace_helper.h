/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *	   Adrian Warecki <adrian.warecki@intel.com>
 */

/**
 * \brief Userspace support functions.
 */
#ifndef __RTOS_USERSPACE_HELPER_H__
#define __RTOS_USERSPACE_HELPER_H__

#include <stdint.h>
#include <stddef.h>

#include <rtos/alloc.h>

#define APP_TASK_BSS
#define APP_TASK_DATA

struct sys_heap;

#ifdef CONFIG_USERSPACE
/**
 * Initialize private processing module heap.
 * @param N/A.
 * @return pointer to the sys_heap structure.
 *
 * @note
 * Function used only when CONFIG_USERSPACE is set.
 * The private heap is used only for non-privileged modules for all processing module allocations
 * that should be isolated. The heap helps to accumulate all dynamic allocations in single memory
 * region which is then added to modules memory domain.
 */
static inline struct sys_heap *module_driver_heap_init(void)
{
	return NULL;
}

#endif

/**
 * Allocates memory block from private module sys_heap if exists, otherwise call rballoc_align().
 * @param sys_heap  - pointer to the sys_heap structure
 * @param flags     - Flags, see SOF_MEM_FLAG_...
 * @param bytes     - Size in bytes.
 * @param alignment - Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 * @note When CONFIG_USERSPACE not set function calls rballoc_align()
 */
static inline void *module_driver_heap_aligned_alloc(struct sys_heap *mod_drv_heap, uint32_t flags,
						     size_t bytes, uint32_t align)
{
	return rballoc_align(flags, bytes, align);
}

/**
 * Allocates memory block from private module sys_heap if exists, otherwise call rmalloc.
 * @param sys_heap - pointer to the sys_heap structure
 * @param flags    - Flags, see SOF_MEM_FLAG_...
 * @param bytes    - Size in bytes.
 * @return         - Pointer to the allocated memory or NULL if failed.
 *
 *  * @note When CONFIG_USERSPACE not set function calls rmalloc()
 */
static inline void *module_driver_heap_rmalloc(struct sys_heap *mod_drv_heap, uint32_t flags,
					       size_t bytes)
{
	return rmalloc(flags, bytes);
}

/**
 * Similar to user_rmalloc(), guarantees that returned block is zeroed.
 *
 * @note When CONFIG_USERSPACE not set function calls rzalloc()
 */
static inline void *module_driver_heap_rzalloc(struct sys_heap *mod_drv_heap, uint32_t flags,
					       size_t bytes)
{
	return rzalloc(flags, bytes);
}

/**
 * Frees the memory block from private module sys_heap if exists. Otherwise call rfree.
 * @param ptr Pointer to the memory block.
 *
 * @note User should take care to not free memory allocated from sys_heap
 *       with module_driver_heap set to NULL. It will cause exception.
 *
 *       When CONFIG_USERSPACE not set function calls rfree()
 */
static inline void module_driver_heap_free(struct sys_heap *mod_drv_heap, void *mem)
{
	rfree(mem);
}

/**
 * Free private processing module heap.
 * @param sys_heap pointer to the sys_heap structure.
 *
 * @note
 * Function used only when CONFIG_USERSPACE is set.
 * Frees private module heap.
 */
static inline void module_driver_heap_remove(struct sys_heap *mod_drv_heap)
{ }

#endif /* __RTOS_USERSPACE_HELPER_H__ */
