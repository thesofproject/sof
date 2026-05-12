// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2025 Intel Corporation.

/* Pre Allocated Contiguous Virtual Region */
#ifndef __SOF_LIB_VREGION_H__
#define __SOF_LIB_VREGION_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct vregion;

/**
 * @brief Memory types for virtual region allocations.
 * Used to specify the type of memory allocation within a virtual region.
 *
 * @note
 * - interim: allocation that can be freed i.e. get/set large config, kcontrols.
 * - lifetime: allocation that cannot be freed i.e. init data, pipeline data.
 */
enum vregion_mem_type {
	VREGION_MEM_TYPE_INTERIM,		/* interim allocation that can be freed */
	VREGION_MEM_TYPE_LIFETIME,		/* lifetime allocation */
	VREGION_MEM_TYPE_INVALID,		/* Interim heap initialization failed */
};

#if CONFIG_SOF_VREGIONS

/**
 * @brief Create a new virtual region instance.
 *
 * Create a new virtual region instance with specified memory size.
 * Allocations start in LIFETIME mode.
 *
 * @param[in] memsize Total size of the virtual region memory.
 * @return struct vregion* Pointer to the new virtual region instance, or NULL on failure.
 */
struct vregion *vregion_create(size_t memsize);

/**
 * @brief Switch virtual region allocations to interim mode.
 *
 * After this call, all allocations from this vregion will use the interim
 * heap. The interim heap is created lazily from remaining lifetime space.
 * Multiple calls are allowed but log a warning.
 *
 * @param[in] vr Pointer to the virtual region instance.
 */
void vregion_set_interim(struct vregion *vr);

/**
 * @brief Increment virtual region's user count.
 *
 * The creator of the virtual region is its first user, for any additional users
 * increment the region's use-count.
 *
 * @param[in] vr Pointer to the virtual region instance to release.
 * @return struct vregion* Pointer to the virtual region instance.
 */
struct vregion *vregion_get(struct vregion *vr);

/**
 * @brief Decrement virtual region's user count or destroy it.
 *
 * Decrement virtual region's user count, when it reaches 0 free all associated
 * resources.
 *
 * @param[in] vr Pointer to the virtual region instance to release.
 * @return struct vregion* Pointer to the virtual region instance or NULL if it has been destroyed.
 */
struct vregion *vregion_put(struct vregion *vr);

/**
 * @brief Allocate memory from the specified virtual region.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] size Size of memory to allocate in bytes.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *vregion_alloc(struct vregion *vr, size_t size);

/**
 * @brief like vregion_alloc() but allocates coherent memory
 */
void *vregion_alloc_coherent(struct vregion *vr, size_t size);

/**
 * @brief Allocate aligned memory from the specified virtual region.
 *
 * Allocate aligned memory from the specified virtual region using the
 * current allocation mode (lifetime or interim).
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] size Size of memory to allocate in bytes.
 * @param[in] alignment Alignment of memory to allocate in bytes.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *vregion_alloc_align(struct vregion *vr, size_t size, size_t alignment);

/**
 * @brief like vregion_alloc_align() but allocates coherent memory
 */
void *vregion_alloc_coherent_align(struct vregion *vr, size_t size, size_t alignment);

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

/**
 * @brief Get virtual region memory start and size.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[in] size Pointer to size
 * @param[in] start Pointer to start
 */
void vregion_mem_info(struct vregion *vr, size_t *size, uintptr_t *start);

#else /* CONFIG_SOF_VREGIONS */

struct vregion {
	unsigned int use_count;
};

static inline struct vregion *vregion_create(size_t memsize)
{
	return NULL;
}
static inline void vregion_set_interim(struct vregion *vr) {}
static inline struct vregion *vregion_get(struct vregion *vr)
{
	return vr;
}
static inline struct vregion *vregion_put(struct vregion *vr)
{
	return vr;
}
static inline void *vregion_alloc(struct vregion *vr, size_t size)
{
	return NULL;
}
static inline void *vregion_alloc_coherent(struct vregion *vr, size_t size)
{
	return NULL;
}
static inline void *vregion_alloc_align(struct vregion *vr, size_t size, size_t alignment)
{
	return NULL;
}
static inline void *vregion_alloc_coherent_align(struct vregion *vr, size_t size,
						 size_t alignment)
{
	return NULL;
}
static inline void vregion_free(struct vregion *vr, void *ptr) {}
static inline void vregion_info(struct vregion *vr) {}
static inline void vregion_mem_info(struct vregion *vr, size_t *size, uintptr_t *start)
{
	if (size)
		*size = 0;
}

#endif /* CONFIG_SOF_VREGIONS */

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_VREGION_H__ */
