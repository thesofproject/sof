// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sof/boot_test.h>
#include <sof/lib/vregion.h>

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_DECLARE(sof_boot_test, CONFIG_SOF_LOG_LEVEL);

static struct vregion *test_vreg_create(void)
{
	/*
	 * Use a 3-page vregion so that after two lifetime allocations there is still
	 * enough remaining space to initialize and exercise the interim heap.
	 */
	struct vregion *vreg = vregion_create(3 * CONFIG_MM_DRV_PAGE_SIZE);

	zassert_not_null(vreg);

	return vreg;
}

static void test_vreg_alloc_lifet(struct vregion *vreg)
{
	void *ptr = vregion_alloc(vreg, 6000);

	zassert_not_null(ptr);

	void *ptr_align = vregion_alloc_align(vreg, 5000, 16);

	zassert_not_null(ptr_align);
	zassert_equal((uintptr_t)ptr_align & 15, 0);

	/*
	 * Seal lifetime, switch to interim. The interim heap is created
	 * lazily from the remaining ~1 page, so a 6000-byte alloc won't fit.
	 */
	vregion_set_interim(vreg);

	void *ptr_nomem = vregion_alloc(vreg, 6000);

	zassert_is_null(ptr_nomem);

	/* Lifetime frees are no-ops; re-alloc from interim still fails */
	vregion_free(vreg, ptr_align);
	vregion_free(vreg, ptr);

	ptr_nomem = vregion_alloc(vreg, 6000);

	zassert_is_null(ptr_nomem);
}

static void test_vreg_alloc_tmp(struct vregion *vreg)
{
	void *ptr = vregion_alloc(vreg, 20);

	zassert_not_null(ptr);

	void *ptr_align = vregion_alloc_align(vreg, 1000, 16);

	zassert_not_null(ptr_align);
	zassert_equal((uintptr_t)ptr_align & 15, 0);

	void *ptr_nomem = vregion_alloc(vreg, 2000);

	zassert_is_null(ptr_nomem);

	vregion_free(vreg, ptr_align);
	vregion_free(vreg, ptr);

	/* Should be possible to allocate again */
	ptr = vregion_alloc(vreg, 1000);

	zassert_not_null(ptr);
}

static void test_vreg_destroy(struct vregion *vreg)
{
	vregion_info(vreg);
	vregion_put(vreg);
}

ZTEST(sof_boot, vregion)
{
	struct vregion *vreg = test_vreg_create();

	/* Test lifetime allocations (initial mode), then seal */
	test_vreg_alloc_lifet(vreg);
	/* Test interim allocations (already switched by lifet test) */
	test_vreg_alloc_tmp(vreg);

	test_vreg_destroy(vreg);
}
