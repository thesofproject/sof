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
 * @return pointer to the k_heap structure.
 *
 * @note
 * Function used only when CONFIG_USERSPACE is set.
 * The private heap is used only for non-privileged modules for all processing module allocations
 * that should be isolated. The heap helps to accumulate all dynamic allocations in single memory
 * region which is then added to modules memory domain.
 */
static inline struct k_heap *module_driver_heap_init(void)
{
	return NULL;
}
#endif

/**
 * Free private processing module heap.
 * @param sys_heap pointer to the sys_heap structure.
 *
 * @note
 * Function used only when CONFIG_USERSPACE is set.
 * Frees private module heap.
 */
static inline void module_driver_heap_remove(struct k_heap *mod_drv_heap)
{ }

#endif /* __RTOS_USERSPACE_HELPER_H__ */
