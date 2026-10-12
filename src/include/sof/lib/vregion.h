// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2025 Intel Corporation.

/**
 * @file sof/lib/vregion.h
 * @brief Pre-allocated contiguous virtual memory region allocator (lifetime vs interim).
 * @ingroup memory_api
 */

#ifndef __SOF_LIB_VREGION_H__
#define __SOF_LIB_VREGION_H__

#include <stddef.h>
#include <stdint.h>
#include <sof/compiler_attributes.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup memory_api
 *  @{
 */

/**
 * @brief Virtual region descriptor managing lifetime and interim heap partitions.
 */
struct vregion;

/**
 * @brief Memory allocation modes within a virtual memory region.
 *
 * Distinguishes lifetime allocations (bound to pipeline topology, cannot be freed until teardown)
 * from interim allocations (dynamically created and released during runtime operations).
 */
enum vregion_mem_type {
	/** @brief Interim allocation that can be freed dynamically (e.g. large config set/get, controls). */
	VREGION_MEM_TYPE_INTERIM,
	/** @brief Lifetime allocation that persists until region destruction (e.g. topology, module instances). */
	VREGION_MEM_TYPE_LIFETIME,
	/** @brief Invalid memory type indicating initialization error or exhaustion. */
	VREGION_MEM_TYPE_INVALID,
};

#if CONFIG_SOF_VREGIONS

/**
 * @brief Create a new virtual region instance.
 *
 * Create a new virtual region instance with specified memory size.
 * Allocations start in LIFETIME mode.
 *
 * @param[in] memsize Total size of the virtual region memory in bytes.
 * @return Pointer to the new virtual region instance, or NULL on failure.
 */
struct vregion *vregion_create(size_t memsize);

/**
 * @brief Create and map a new virtual region instance accessible by user-space domains.
 *
 * Allocates contiguous virtual memory and configures memory protection domain permissions.
 *
 * @param[out] vreg_start Output pointer receiving the virtual start address of the region.
 * @param[in,out] vreg_size Input pointer specifying the requested size in bytes (must be non-zero);
 *                          updated on return with the actual mapped size in bytes.
 * @return Pointer to the allocated virtual region instance, or NULL on failure.
 */
__syscall struct vregion *vregion_create_map(uintptr_t *vreg_start, size_t *vreg_size);

/**
 * @brief Switch virtual region allocations to interim mode.
 *
 * After this call, all allocations from this vregion will use the interim
 * heap. The interim heap is created lazily from remaining lifetime space.
 * Multiple calls are allowed but log a warning.
 *
 * @param[in] vr Pointer to the virtual region instance.
 */
__syscall void vregion_set_interim(struct vregion *vr);

/**
 * @brief Increment virtual region's user count.
 *
 * The creator of the virtual region is its first user, for any additional users
 * increment the region's use-count.
 *
 * @param[in] vr Pointer to the virtual region instance to release.
 * @return struct vregion* Pointer to the virtual region instance.
 */
__syscall struct vregion *vregion_get(struct vregion *vr);

/**
 * @brief Decrement virtual region's user count or destroy it.
 *
 * Decrement virtual region's user count, when it reaches 0 free all associated
 * resources.
 *
 * @param[in] vr Pointer to the virtual region instance to release.
 * @return struct vregion* Pointer to the virtual region instance or NULL if it has been destroyed.
 */
__syscall struct vregion *vregion_put(struct vregion *vr);

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
__syscall void *vregion_alloc_align(struct vregion *vr, size_t size, size_t alignment);

/**
 * @brief Allocate aligned coherent (uncached) memory from the specified virtual region.
 * @param[in,out] vr Pointer to the virtual region instance.
 * @param[in] size Size of memory to allocate in bytes.
 * @param[in] alignment Alignment of memory to allocate in bytes.
 * @return Pointer to the allocated coherent memory, or NULL on failure.
 */
__syscall void *vregion_alloc_coherent_align(struct vregion *vr, size_t size, size_t alignment);

/**
 * @brief Allocate memory from the specified virtual region.
 *
 * @param[in,out] vr Pointer to the virtual region instance.
 * @param[in] size Size of memory to allocate in bytes.
 * @return Pointer to the allocated memory, or NULL on failure.
 */
static inline void *vregion_alloc(struct vregion *vr, size_t size)
{
	return vregion_alloc_align(vr, size, 0);
}

/**
 * @brief Allocate naturally aligned coherent (uncached) memory from the specified virtual region.
 * @param[in,out] vr Pointer to the virtual region instance.
 * @param[in] size Size of memory to allocate in bytes.
 * @return Pointer to the allocated coherent memory, or NULL on failure.
 */
static inline void *vregion_alloc_coherent(struct vregion *vr, size_t size)
{
	return vregion_alloc_coherent_align(vr, size, 0);
}

/**
 * @brief Free memory allocated from the specified virtual region.
 *
 * Free memory previously allocated from the specified virtual region.
 *
 * @param[in,out] vr Pointer to the virtual region instance.
 * @param[in,out] ptr Pointer to the memory to free.
 */
__syscall void vregion_free(struct vregion *vr, void *ptr);

/**
 * @brief Log virtual region memory usage.
 *
 * @param[in] vr Pointer to the virtual region instance.
 */
void vregion_info(struct vregion *vr);

/**
 * @brief Retrieve virtual region memory base address and size.
 *
 * @param[in] vr Pointer to the virtual region instance.
 * @param[out] size Output pointer receiving total region size in bytes.
 * @param[out] start Output pointer receiving starting virtual address.
 */
void vregion_mem_info(struct vregion *vr, size_t *size, uintptr_t *start);

/**
 * @brief Validate that a virtual region handle is genuine and isolated from user space.
 * @param[in] vr Pointer to virtual region instance to verify.
 * @return True if handle is valid and secure, false otherwise.
 */
bool vregion_verify(struct vregion *vr);

#include <zephyr/syscalls/vregion.h>

#else

struct vregion {
	unsigned int use_count;
};

static inline struct vregion *vregion_create(size_t memsize)
{
	return NULL;
}
static inline struct vregion *vregion_create_map(uintptr_t *vreg_start, size_t *vreg_size)
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
static inline bool vregion_verify(struct vregion *vr) {return false;}

#endif /* CONFIG_SOF_VREGIONS */

/** @}*/

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_VREGION_H__ */
