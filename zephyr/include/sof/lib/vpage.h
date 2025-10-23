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
 * Allocates a specified number of contiguous virtual memory pages by mapping
 * physical pages.
 *
 * @param[in] pages Number of pages (usually 4kB large) to allocate.
 *
 * @return Pointer to the allocated virtual memory region, or NULL on failure.
 */
void *vpage_alloc(unsigned int pages);

/**
 * @brief Free virtual pages
 * Frees previously allocated virtual memory pages and unmaps them.
 *
 * @param[in] ptr Pointer to the memory pages to free.
 */
void vpage_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_VPAGE_H__ */
