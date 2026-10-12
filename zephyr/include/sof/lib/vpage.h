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

/** @}*/

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_VPAGE_H__ */
