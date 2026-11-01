// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2025 Intel Corporation.

/**
 * @file sof/lib/vpage.h
 * @brief Virtual Page Allocator API for mapping contiguous memory pages.
 * @ingroup memory_api
 */

#ifndef __SOF_LIB_VPAGE_H__
#define __SOF_LIB_VPAGE_H__

#include <zephyr/kernel.h>
#include <zephyr/drivers/mm/system_mm.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup memory_api
 *  @{
 */

/**
 * @brief Allocate contiguous virtual pages.
 *
 * Allocates a specified number of contiguous virtual memory pages by mapping
 * physical backing pages into the DSP page table.
 *
 * @param[in] pages Number of pages (typically 4KB per page) to allocate.
 * @return Pointer to the allocated virtual memory region, or NULL on failure.
 */
void *vpage_alloc(unsigned int pages);

/**
 * @brief Free virtual pages.
 *
 * Frees previously allocated virtual memory pages and unmaps them from the page table.
 *
 * @param[in,out] ptr Pointer to the virtual memory pages to free.
 */
void vpage_free(void *ptr);

/**
 * @brief Reserve contiguous virtual pages.
 *
 * Reserves a specified number of contiguous virtual memory pages without
 * mapping any physical pages to them. For users that need to map the pages
 * themselves, e.g. with different permissions for parts of the range.
 *
 * @param[in] pages Number of pages (typically 4KB per page) to reserve.
 * @return Pointer to the reserved virtual memory region, or NULL on failure.
 */
void *vpage_reserve(unsigned int pages);

/**
 * @brief Reserve contiguous virtual pages at a given address.
 *
 * Like vpage_reserve() but for a range at a fixed address, e.g. for users that
 * had the range reserved before the DSP was powered down and are restored at
 * the same address.
 *
 * @param[in] ptr Page aligned address of the first page to reserve.
 * @param[in] pages Number of pages (typically 4KB per page) to reserve.
 * @return 0 on success or a negative error code if any of the pages is already
 *	   in use or outside of the virtual page region.
 */
int vpage_reserve_at(void *ptr, unsigned int pages);

/**
 * @brief Release reserved virtual pages.
 *
 * Releases virtual memory pages, reserved with vpage_reserve() or
 * vpage_reserve_at(). The caller has to unmap any pages it has mapped first.
 *
 * @param[in] ptr Pointer to the first reserved page.
 */
void vpage_release(void *ptr);

/**
 * @brief Get the virtual memory region managed by the allocator.
 *
 * Users of vpage_reserve() need it to map pages in the reserved range.
 *
 * @return The region, or NULL if no virtual page region could be created.
 */
const struct sys_mm_drv_region *vpage_get_region(void);

/** @}*/

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_VPAGE_H__ */
