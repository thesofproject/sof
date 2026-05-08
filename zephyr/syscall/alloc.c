// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <rtos/alloc.h>
#include <sof/schedule/ll_schedule_domain.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

static inline void *z_vrfy_sof_heap_alloc(struct k_heap *heap, uint32_t flags,
					  size_t bytes, size_t alignment)
{
	/* only allow flags that are safe for user-space heap isolation */
	static const uint32_t allowed_flags =
		SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT | SOF_MEM_FLAG_DMA;

	K_OOPS(flags & ~allowed_flags);

	/* user-space use of sof_heap_alloc() limited to this single heap */
	K_OOPS(!zephyr_ll_user_heap_verify(heap));

	return z_impl_sof_heap_alloc(heap, flags, bytes, alignment);
}
#include <zephyr/syscalls/sof_heap_alloc_mrsh.c>

static inline void z_vrfy_sof_heap_free(struct k_heap *heap, void *addr)
{
	/* user-space use of sof_heap_alloc() limited to this single heap */
	K_OOPS(!zephyr_ll_user_heap_verify(heap));

	if (addr) {
		uintptr_t start = (uintptr_t)heap->heap.init_mem;
		uintptr_t addr_uc = (uintptr_t)sys_cache_uncached_ptr_get(addr);
		K_OOPS(addr_uc < start || addr_uc >= start + heap->heap.init_bytes);
		K_OOPS(K_SYSCALL_MEMORY_WRITE(addr, 1));
	}
	z_impl_sof_heap_free(heap, addr);
}
#include <zephyr/syscalls/sof_heap_free_mrsh.c>
