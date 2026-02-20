// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <rtos/alloc.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

static inline void *z_vrfy_sof_heap_alloc(struct k_heap *heap, uint32_t flags,
					  size_t bytes, size_t alignment)
{
	return z_impl_sof_heap_alloc(heap, flags, bytes, alignment);
}
#include <zephyr/syscalls/sof_heap_alloc_mrsh.c>

static inline void z_vrfy_sof_heap_free(struct k_heap *heap, void *addr)
{
	z_impl_sof_heap_free(heap, addr);
}
#include <zephyr/syscalls/sof_heap_free_mrsh.c>
