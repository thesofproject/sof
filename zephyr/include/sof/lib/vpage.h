// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2025 Intel Corporation.

/* Virtual Page Allocator API */
#ifndef __SOF_LIB_VPAGE_H__
#define __SOF_LIB_VPAGE_H__

#include <zephyr/kernel.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate virtual pages
 * Allocates a specified number of contiguous virtual memory pages.
 *
 * @param[in] pages Number of 4kB pages to allocate.
 *
 * @return Pointer to the allocated virtual memory region, or NULL on failure.
 */
void *vpage_alloc(uint32_t pages);

/**
 * @brief Free virtual pages
 * Frees previously allocated virtual memory pages and unmaps them.
 *
 * @param[in] ptr Pointer to the memory pages to free.
 */
void vpage_free(void *ptr);

/**
 * @brief Initialize virtual page allocator
 *
 * Initializes a virtual page allocator that manages a virtual memory region
 * using a page table and block structures.
 *
 * @retval 0 if successful.
 * @retval -ENOMEM on creation failure.
 */
int vpage_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_VPAGE_H__ */
