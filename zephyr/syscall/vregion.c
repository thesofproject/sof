// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.

#include <sof/lib/vregion.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

/*
 * Every validator pins the vregion with vregion_verify_pin() for the
 * duration of the call, so a concurrent vregion_put() from another thread
 * cannot free the vregion while the implementation uses it. The pin is
 * counted separately from references, so such a vregion_put() can not
 * consume it either.
 */

static inline void *z_vrfy_vregion_alloc_align(struct vregion *vr, size_t size, size_t alignment)
{
	void *ptr = NULL;

	if (vregion_verify_pin(vr)) {
		ptr = z_impl_vregion_alloc_align(vr, size, alignment);
		vregion_unpin(vr);
	}

	return ptr;
}
#include <zephyr/syscalls/vregion_alloc_align_mrsh.c>

static inline void *z_vrfy_vregion_alloc_coherent_align(struct vregion *vr,
							size_t size, size_t alignment)
{
	void *ptr = NULL;

	if (vregion_verify_pin(vr)) {
		ptr = z_impl_vregion_alloc_coherent_align(vr, size, alignment);
		vregion_unpin(vr);
	}

	return ptr;
}
#include <zephyr/syscalls/vregion_alloc_coherent_align_mrsh.c>

static inline void z_vrfy_vregion_free(struct vregion *vr, void *ptr)
{
	if (vregion_verify_pin(vr)) {
		z_impl_vregion_free(vr, ptr);
		vregion_unpin(vr);
	}
}
#include <zephyr/syscalls/vregion_free_mrsh.c>

struct vregion *z_vrfy_vregion_get(struct vregion *vr)
{
	bool ok;

	if (!vregion_verify_pin(vr))
		return NULL;

	ok = vregion_pinned_get(vr);
	vregion_unpin(vr);

	return ok ? vr : NULL;
}
#include <zephyr/syscalls/vregion_get_mrsh.c>

struct vregion *z_vrfy_vregion_put(struct vregion *vr)
{
	int ret;

	if (!vregion_verify_pin(vr))
		return NULL;

	/* drop the caller's reference, freeing is left to the unpin */
	ret = vregion_pinned_put(vr);
	vregion_unpin(vr);

	/* releasing a reference the caller does not hold */
	K_OOPS(ret < 0);

	return ret ? vr : NULL;
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
	if (vregion_verify_pin(vr)) {
		z_impl_vregion_set_interim(vr);
		vregion_unpin(vr);
	}
}
#include <zephyr/syscalls/vregion_set_interim_mrsh.c>
