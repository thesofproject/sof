// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/lib/vregion.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

static inline void *z_vrfy_vregion_alloc_align(struct vregion *vr, size_t size, size_t alignment)
{
	if (vregion_verify(vr))
		return z_impl_vregion_alloc_align(vr, size, alignment);

	return NULL;
}
#include <zephyr/syscalls/vregion_alloc_align_mrsh.c>

static inline void *z_vrfy_vregion_alloc_coherent_align(struct vregion *vr,
							size_t size, size_t alignment)
{
	if (vregion_verify(vr))
		return z_impl_vregion_alloc_coherent_align(vr, size, alignment);

	return NULL;
}
#include <zephyr/syscalls/vregion_alloc_coherent_align_mrsh.c>

static inline void z_vrfy_vregion_free(struct vregion *vr, void *ptr)
{
	if (vregion_verify(vr))
		z_impl_vregion_free(vr, ptr);
}
#include <zephyr/syscalls/vregion_free_mrsh.c>

struct vregion *z_vrfy_vregion_get(struct vregion *vr)
{
	if (vregion_verify(vr))
		return z_impl_vregion_get(vr);
	return NULL;
}
#include <zephyr/syscalls/vregion_get_mrsh.c>

struct vregion *z_vrfy_vregion_put(struct vregion *vr)
{
	if (vregion_verify(vr))
		return z_impl_vregion_put(vr);
	return NULL;
}
#include <zephyr/syscalls/vregion_put_mrsh.c>

struct vregion *z_vrfy_vregion_create_map(uintptr_t *vreg_start, size_t *vreg_size)
{
	K_OOPS(K_SYSCALL_MEMORY_WRITE(vreg_start, sizeof(*vreg_start)));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(vreg_size, sizeof(*vreg_size)));
	return z_impl_vregion_create_map(vreg_start, vreg_size);
}
#include <zephyr/syscalls/vregion_create_map_mrsh.c>

void z_vrfy_vregion_set_interim(struct vregion *vr)
{
	if (vregion_verify(vr))
		z_impl_vregion_set_interim(vr);
}
#include <zephyr/syscalls/vregion_set_interim_mrsh.c>
