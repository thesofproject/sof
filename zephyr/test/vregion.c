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
	struct vregion *vreg = vregion_create(CONFIG_MM_DRV_PAGE_SIZE - 100,
					      CONFIG_MM_DRV_PAGE_SIZE);

	zassert_not_null(vreg);

	return vreg;
}

static void test_vreg_alloc_lifet(struct vregion *vreg)
{
	void *ptr = vregion_alloc(vreg, VREGION_MEM_TYPE_LIFETIME, 2000);

	zassert_not_null(ptr);

	void *ptr_align = vregion_alloc_align(vreg, VREGION_MEM_TYPE_LIFETIME, 2000, 16);

	zassert_not_null(ptr_align);
	zassert_equal((uintptr_t)ptr_align & 15, 0);

	void *ptr_nomem = vregion_alloc(vreg, VREGION_MEM_TYPE_LIFETIME, 2000);

	zassert_is_null(ptr_nomem);

	vregion_free(vreg, ptr_align);
	vregion_free(vreg, ptr);

	/* Freeing isn't possible with LIFETIME */
	ptr_nomem = vregion_alloc(vreg, VREGION_MEM_TYPE_LIFETIME, 2000);

	zassert_is_null(ptr_nomem);
}

static void test_vreg_alloc_tmp(struct vregion *vreg)
{
	void *ptr = vregion_alloc(vreg, VREGION_MEM_TYPE_INTERIM, 20);

	zassert_not_null(ptr);

	void *ptr_align = vregion_alloc_align(vreg, VREGION_MEM_TYPE_INTERIM, 2000, 16);

	zassert_not_null(ptr_align);
	zassert_equal((uintptr_t)ptr_align & 15, 0);

	void *ptr_nomem = vregion_alloc(vreg, VREGION_MEM_TYPE_INTERIM, 2000);

	zassert_is_null(ptr_nomem);

	vregion_free(vreg, ptr_align);
	vregion_free(vreg, ptr);

	/* Should be possible to allocate again */
	ptr = vregion_alloc(vreg, VREGION_MEM_TYPE_INTERIM, 2000);

	zassert_not_null(ptr);
}

static void test_vreg_destroy(struct vregion *vreg)
{
	vregion_info(vreg);
	vregion_destroy(vreg);
}

ZTEST(sof_boot, vregion)
{
	struct vregion *vreg = test_vreg_create();

	/* Test interim allocations */
	test_vreg_alloc_tmp(vreg);
	/* Test lifetime allocations */
	test_vreg_alloc_lifet(vreg);

	test_vreg_destroy(vreg);
}
