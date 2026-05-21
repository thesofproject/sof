// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/lib/vregion.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

static inline void *z_vrfy_vregion_alloc(struct vregion *vr,
					 enum vregion_mem_type type, size_t size)
{
	size_t vr_size = 0;
	uintptr_t vr_start;

	vregion_mem_info(vr, &vr_size, &vr_start);
	if (vr_size)
		K_OOPS(K_SYSCALL_MEMORY_WRITE((void *)vr_start, vr_size));

	return z_impl_vregion_alloc(vr, type, size);
}
#include <zephyr/syscalls/vregion_alloc_mrsh.c>

static inline void *z_vrfy_vregion_alloc_coherent(struct vregion *vr,
						  enum vregion_mem_type type, size_t size)
{
	size_t vr_size = 0;
	uintptr_t vr_start;

	vregion_mem_info(vr, &vr_size, &vr_start);
	if (vr_size)
		K_OOPS(K_SYSCALL_MEMORY_WRITE((void *)vr_start, vr_size));

	return z_impl_vregion_alloc_coherent(vr, type, size);
}
#include <zephyr/syscalls/vregion_alloc_coherent_mrsh.c>

static inline void *z_vrfy_vregion_alloc_align(struct vregion *vr,
					       enum vregion_mem_type type,
					       size_t size, size_t alignment)
{
	size_t vr_size = 0;
	uintptr_t vr_start;

	vregion_mem_info(vr, &vr_size, &vr_start);
	if (vr_size)
		K_OOPS(K_SYSCALL_MEMORY_WRITE((void *)vr_start, vr_size));

	return z_impl_vregion_alloc_align(vr, type, size, alignment);
}
#include <zephyr/syscalls/vregion_alloc_align_mrsh.c>

static inline void *z_vrfy_vregion_alloc_coherent_align(struct vregion *vr,
							enum vregion_mem_type type,
							size_t size, size_t alignment)
{
	size_t vr_size = 0;
	uintptr_t vr_start;

	vregion_mem_info(vr, &vr_size, &vr_start);
	if (vr_size)
		K_OOPS(K_SYSCALL_MEMORY_WRITE((void *)vr_start, vr_size));

	return z_impl_vregion_alloc_coherent_align(vr, type, size, alignment);
}
#include <zephyr/syscalls/vregion_alloc_coherent_align_mrsh.c>

static inline void z_vrfy_vregion_free(struct vregion *vr, void *ptr)
{
	size_t vr_size = 0;
	uintptr_t vr_start;

	vregion_mem_info(vr, &vr_size, &vr_start);
	if (vr_size)
		K_OOPS(K_SYSCALL_MEMORY_WRITE((void *)vr_start, vr_size));

	z_impl_vregion_free(vr, ptr);
}
#include <zephyr/syscalls/vregion_free_mrsh.c>
