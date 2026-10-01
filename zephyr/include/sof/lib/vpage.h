// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2025 Intel Corporation.

/* Virtual Page Allocator API */
#ifndef __SOF_LIB_VPAGE_H__
#define __SOF_LIB_VPAGE_H__

#include <zephyr/kernel.h>
#include <zephyr/drivers/mm/system_mm.h>
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

/**
 * @brief Reserve virtual pages
 * Reserves a specified number of contiguous virtual memory pages from the
 * shared virtual page allocator, without mapping any physical memory to
 * them. Intended for callers that need to map the pages themselves, e.g.
 * with per-section permissions.
 *
 * @param[in] pages Number of pages (usually 4kB large) to reserve.
 *
 * @return Pointer to the reserved virtual memory region, or NULL on failure.
 */
void *vpage_reserve(unsigned int pages);

/**
 * @brief Release reserved virtual pages
 * Releases virtual memory pages previously reserved with vpage_reserve().
 * Does not unmap any physical memory - callers that mapped the pages
 * themselves must unmap them first.
 *
 * @param[in] ptr Pointer to the reserved memory pages to release.
 */
void vpage_release(void *ptr);

/**
 * @brief Get the shared virtual page allocator's memory region
 *
 * @return Pointer to the virtual memory region backing the virtual page
 * allocator.
 */
const struct sys_mm_drv_region *vpage_get_region(void);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_VPAGE_H__ */
