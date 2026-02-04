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

#define USER_MOD_HEAP_SIZE	ALIGN_UP(CONFIG_SOF_ZEPHYR_USERSPACE_MODULE_HEAP_SIZE, \
					 CONFIG_MM_DRV_PAGE_SIZE)
#define APP_TASK_BSS	K_APP_BMEM(common_partition)
#define APP_TASK_DATA	K_APP_DMEM(common_partition)

struct processing_module;
struct userspace_context;

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
struct k_heap *module_driver_heap_init(void);

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

/**
 * Attach common userspace memory partition to a module memory domain.
 * @param dom - memory domain to attach the common partition to.
 *
 * @return 0 for success, error otherwise.
 *
 * @note
 * Function used only when CONFIG_USERSPACE is set.
 * The common partition contains shared objects required by user-space modules.
 */
int user_memory_attach_common_partition(struct k_mem_domain *dom);

#endif

/**
 * Allocates thread stack memory.
 * @param stack_size Required stack size.
 * @param options Stack configuration options
 *        K_USER - when creating user thread
 *        0      - when creating kernel thread
 * @return pointer to the stack or NULL if not created.
 *
 * When CONFIG_USERSPACE not set function calls rballoc_align(),
 * otherwise it uses k_thread_stack_alloc() routine.
 *
 */
void *user_stack_allocate(size_t stack_size, uint32_t options);

/**
 * Free thread stack memory.
 * @param p_stack Pointer to the stack.
 *
 * @return 0 for success, error otherwise.
 *
 * @note
 * When CONFIG_USERSPACE not set function calls rfree(),
 * otherwise it uses k_thread_stack_free() routine.
 *
 */
int user_stack_free(void *p_stack);

/**
 * Free private processing module heap.
 * @param sys_heap pointer to the sys_heap structure.
 *
 * @note
 * Function used only when CONFIG_USERSPACE is set.
 * Frees private module heap.
 */
void module_driver_heap_remove(struct k_heap *mod_drv_heap);

#ifdef CONFIG_USERSPACE

/**
 * Add access to mailbox.h interface to a user-space thread.
 *
 * @param domain memory domain to add the mailbox partitions to
 * @param thread_id user-space thread for which access is added
 */
int user_access_to_mailbox(struct k_mem_domain *domain, k_tid_t thread_id);

#else

static inline int user_access_to_mailbox(struct k_mem_domain *domain, k_tid_t thread_id)
{
	return 0;
}

#endif /* CONFIG_USERSPACE */

#endif /* __ZEPHYR_LIB_USERSPACE_HELPER_H__ */
