// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2025 Intel Corporation.

/* Pre Allocated Contiguous Virtual Region */
#ifndef __SOF_LIB_PACOVR_H__
#define __SOF_LIB_PACOVR_H__

#include <zephyr/kernel.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pacovr;

/**
 * @brief Create a new PACOVR instance.
 *
 * Create a new PACOVR instance with specified batch and scratch sizes. Total
 * size is the sum of batch and scratch sizes.
 *
 * @param[in] batch_size Size of the PACOVR batch region.
 * @param[in] scratch_size Size of the scratch heap.
 * @return struct pacovr* Pointer to the new PACOVR instance, or NULL on failure.
 */
struct pacovr *pacovr_create(size_t batch_size, size_t scratch_size);

/**
 * @brief Destroy a PACOVR instance.
 *
 * Free all associated resources and deallocate the PACOVR instance.
 *
 * @param[in] p Pointer to the PACOVR instance to destroy.
 */
void pacovr_destroy(struct pacovr *p);

/**
 * @brief Allocate memory from the PACOVR dynamic heap.
 *
 * Allocate memory from the PACOVR dynamic heap. Intended use for temporary
 * allocations during audio processing. i.e. change of parameters or kcontrols.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] size Size of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *pacovr_dynamic_alloc(struct pacovr *p, size_t size);

/**
 * @brief Allocate memory with alignment from the PACOVR dynamic heap.
 *
 * Allocate memory with alignment from the PACOVR dynamic heap. Intended use for
 * temporary allocations during audio processing. i.e. change of parameters or
 * kcontrols.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] size Size of the allocation.
 * @param[in] align Alignment of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *pacovr_dynamic_alloc_align(struct pacovr *p, size_t size, size_t align);

/**
 * @brief Free memory from the PACOVR dynamic heap.
 *
 * Free memory from the PACOVR dynamic heap. Intended use for temporary
 * allocations during audio processing. i.e. change of parameters or
 * kcontrols.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] ptr Pointer to the memory to free.
 */
void pacovr_dynamic_free(struct pacovr *p, void *ptr);

/**
 * @brief Allocate memory from the PACOVR static allocator.
 *
 * Allocate memory from the PACOVR static allocator. Intended use for
 * allocations that persist for the lifetime of the audio processing pipeline.
 * i.e. component data, buffers, etc.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] size Size of the allocation.
 * @param[in] align Alignment of the allocation.
 * @return void* Pointer to the allocated memory, or NULL on failure.
 */
void *pacovr_static_alloc(struct pacovr *p, size_t size);

/**
 * @brief Free memory from the PACOVR static allocator.
 *
 * Free memory from the PACOVR static allocator. This is a no-op and is
 * intended for tuning and tracking only. Static allocations are freed
 * when the PACOVR instance is destroyed. Any call to this function usually
 * means the allocation should have been from the dynamic heap.
 *
 * @param[in] p Pointer to the PACOVR instance.
 * @param[in] ptr Pointer to the memory to free.
 */
void pacovr_static_free(struct pacovr *p, void *ptr);

/**
 * @brief Log PACOVR memory usage.
 *
 * @param[in] p Pointer to the PACOVR instance.
 */
void pacovr_info(struct pacovr *p);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_LIB_PACOVR_H__ */
