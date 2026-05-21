/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_CTX_ALLOC_H__
#define __SOF_CTX_ALLOC_H__

#include <sof/audio/component.h>
#include <sof/lib/vregion.h>
#include <rtos/alloc.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Allocate memory from a mod_alloc_ctx context.
 *
 * When the context has a vregion, allocates from the vregion interim
 * partition. Coherent memory is used when SOF_MEM_FLAG_COHERENT is set
 * in flags. Falls back to sof_heap_alloc() otherwise.
 *
 * @param ctx		Allocation context (heap + optional vregion).
 * @param flags		Allocation flags (SOF_MEM_FLAG_*).
 * @param size		Size in bytes.
 * @param alignment	Required alignment in bytes.
 * @return Pointer to allocated memory or NULL on failure.
 */
static inline void *sof_ctx_alloc(struct mod_alloc_ctx *ctx, uint32_t flags,
				  size_t size, size_t alignment)
{
	if (!ctx || !ctx->vreg)
		return sof_heap_alloc(ctx ? ctx->heap : NULL, flags, size, alignment);

	if (flags & SOF_MEM_FLAG_COHERENT)
		return vregion_alloc_coherent_align(ctx->vreg, VREGION_MEM_TYPE_INTERIM,
						    size, alignment);

	return vregion_alloc_align(ctx->vreg, VREGION_MEM_TYPE_INTERIM, size, alignment);
}

/**
 * Allocate zero-initialized memory from a mod_alloc_ctx context.
 * @param ctx	Allocation context.
 * @param size	Size in bytes.
 * @return Pointer to allocated memory or NULL on failure.
 */
static inline void *sof_ctx_zalloc(struct mod_alloc_ctx *ctx, size_t size)
{
	return sof_ctx_alloc(ctx, 0, size, 0);
}

/**
 * Free memory allocated from a mod_alloc_ctx context.
 * @param ctx	Allocation context.
 * @param ptr	Pointer to free.
 */
static inline void sof_ctx_free(struct mod_alloc_ctx *ctx, void *ptr)
{
	if (!ptr)
		return;

	if (ctx && ctx->vreg)
		vregion_free(ctx->vreg, ptr);
	else
		sof_heap_free(ctx ? ctx->heap : NULL, ptr);
}

#endif /* __SOF_CTX_ALLOC_H__ */
