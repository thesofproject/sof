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
#ifndef __ZEPHYR_LIB_USERSPACE_HELPER_H__
#define __ZEPHYR_LIB_USERSPACE_HELPER_H__

#ifndef CONFIG_USERSPACE
#define APP_TASK_BSS
#define APP_TASK_DATA
#else

#include <zephyr/app_memory/app_memdomain.h>

#define DRV_HEAP_SIZE	ALIGN_UP(CONFIG_SOF_ZEPHYR_USERSPACE_MODULE_HEAP_SIZE, \
				 CONFIG_MM_DRV_PAGE_SIZE)
#define APP_TASK_BSS	K_APP_BMEM(common_partition)
#define APP_TASK_DATA	K_APP_DMEM(common_partition)

struct processing_module;
struct userspace_context;

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
struct sys_heap *module_driver_heap_init(void);

/**
 * Add DP scheduler created thread to module memory domain.
 * @param thread_id - id of thread to be added to memory domain.
 * @param module    - processing module structure
 *
 * @return 0 for success, error otherwise.
 *
 * @note
 * Function used only when CONFIG_USERSPACE is set.
 */
int user_memory_init_shared(k_tid_t thread_id, struct processing_module *mod);

#endif

/**
 * Allocates memory block from private module sys_heap if exists, otherwise call rballoc_align().
 * @param sys_heap - pointer to the sys_heap structure
 * @param flags    - Flags, see SOF_MEM_FLAG_...
 * @param bytes     - Size in bytes.
 * @param alignment - Alignment in bytes.
 * @return Pointer to the allocated memory or NULL if failed.
 *
 * @note When CONFIG_USERSPACE not set function calls rballoc_align()
 */
void *module_driver_heap_aligned_alloc(struct sys_heap *mod_drv_heap, uint32_t flags, size_t bytes,
				       uint32_t align);

/**
 * Allocates memory block from private module sys_heap if exists, otherwise call rmalloc.
 * @param sys_heap - pointer to the sys_heap structure
 * @param flags    - Flags, see SOF_MEM_FLAG_...
 * @param bytes    - Size in bytes.
 * @return         - Pointer to the allocated memory or NULL if failed.
 *
 *  * @note When CONFIG_USERSPACE not set function calls rmalloc()
 */
void *module_driver_heap_rmalloc(struct sys_heap *mod_drv_heap, uint32_t flags, size_t bytes);

/**
 * Similar to user_rmalloc(), guarantees that returned block is zeroed.
 *
 * @note When CONFIG_USERSPACE not set function calls rzalloc()
 */
void *module_driver_heap_rzalloc(struct sys_heap *mod_drv_heap, uint32_t flags, size_t bytes);

/**
 * Frees the memory block from private module sys_heap if exists. Otherwise call rfree.
 * @param ptr Pointer to the memory block.
 *
 * @note User should take care to not free memory allocated from sys_heap
 *       with mod_drv_heap set to NULL. It will cause exception.
 *
 *       When CONFIG_USERSPACE not set function calls rfree()
 */
void module_driver_heap_free(struct sys_heap *mod_drv_heap, void *mem);

/**
 * Free private processing module heap.
 * @param sys_heap pointer to the sys_heap structure.
 *
 * @note
 * Function used only when CONFIG_USERSPACE is set.
 * Frees private module heap.
 */
void module_driver_heap_remove(struct sys_heap *mod_drv_heap);

#endif /* __ZEPHYR_LIB_USERSPACE_HELPER_H__ */
