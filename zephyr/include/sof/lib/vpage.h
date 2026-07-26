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
 * @brief Snapshot of virtual page allocator global statistics.
 */
struct vpage_stats {
	void *region_base;
	size_t region_size;
	unsigned int total_pages;
	unsigned int free_pages;
	unsigned int num_elems_in_use;
	unsigned int max_allocs;
};

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
 * @brief Get a snapshot of virtual page allocator statistics.
 *
 * @param[out] stats Filled with current allocator state.
 */
void vpage_get_stats(struct vpage_stats *stats);

/**
 * @brief Iterate over all currently in-use virtual page allocations.
 *
 * The allocator lock is held for the duration of the walk; @p cb must not
 * block or call back into the vpage allocator.
 *
 * @param[in] cb  Callback invoked once per allocation element.
 * @param[in] ctx Opaque context passed through to @p cb.
 */
void vpage_for_each_alloc(void (*cb)(unsigned int idx, unsigned int vpage,
				     unsigned int pages, void *ctx),
			  void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_VPAGE_H__ */
