// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2025 Intel Corporation.

/* Pre Allocated Contiguous Virtual Region */
#ifndef __SOF_LIB_VREGION_H__
#define __SOF_LIB_VREGION_H__

#include <zephyr/kernel.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vregion;

/**
 * @brief Create a new virtual region instance.
 *
 * Create a new virtual region instance with specified static, dynamic, and shared static sizes
 * plus an optional read-only text partition and optional shared static partition.
 * Total size is the sum of static, dynamic, shared static, and text sizes.
 *
 * @param[in] lifetime_size Size of the virtual region lifetime partition.
 * @param[in] interim_size Size of the virtual region interim partition.
 * @param[in] lifetime_shared_size Size of the virtual region shared lifetime partition.
 * @param[in] interim_shared_size Size of the virtual region shared interim partition.
 * @param[in] text_size Size of the optional read-only text partition.
 * @return struct vregion* Pointer to the new virtual region instance, or NULL on failure.
 */
struct vregion *vregion_create(size_t lifetime_size, size_t interim_size,
			       size_t lifetime_shared_size, size_t interim_shared_size,
			       size_t text_size);

/**
 * @brief Destroy a virtual region instance.
 *
 * Free all associated resources and deallocate the virtual region instance.
 *
 * @param[in] vr Pointer to the virtual region instance to destroy.
 */
void vregion_destroy(struct vregion *vr);

/**
 * @brief Memory types for virtual region allocations.
 * Used to specify the type of memory allocation within a virtual region.
 */
enum vregion_mem_type {
	VREGION_MEM_TYPE_INTERIM,		/* interim allocation that can be freed */
	VREGION_MEM_TYPE_LIFETIME,		/* lifetime allocation */
	VREGION_MEM_TYPE_INTERIM_SHARED,	/* shared interim allocation */
	VREGION_MEM_TYPE_LIFETIME_SHARED	/* shared lifetime allocation */
};
void *vregion_alloc(struct vregion *vr, enum vregion_mem_type type, size_t size);

/**
 * @brief Allocate aligned memory from the specified virtual region.
 *
 * Allocate aligned memory from the specified virtual region based on the memory type.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] type Type of memory to allocate (static, dynamic, or shared static).
 * @param[in] size Size of memory to allocate in bytes.
 * @param[in] alignment Alignment of memory to allocate in bytes.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *vregion_alloc_align(struct vregion *vr, enum vregion_mem_type type, size_t size, size_t alignment);

/**
 * @brief Free memory allocated from the specified virtual region.
 *
 * Free memory previously allocated from the specified virtual region.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] ptr Pointer to the memory to free.
 */
void vregion_free(struct vregion *vr, void *ptr);

/**
 * @brief Log virtual region memory usage.
 *
 * @param[in] vr Pointer to the virtual region instance.
 */
void vregion_info(struct vregion *vr);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_VREGION_H__ */
