// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sof/boot_test.h>
#include <sof/lib/vpage.h>

#include <zephyr/drivers/mm/system_mm.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

LOG_MODULE_DECLARE(sof_boot_test, CONFIG_SOF_LOG_LEVEL);

ZTEST(sof_boot, vpage)
{
	void *p1 = vpage_alloc(1);

	zassert_not_null(p1);

	void *p2 = vpage_alloc(2);

	zassert_not_null(p2);

	vpage_free(p1);
	vpage_free(p2);

	p1 = vpage_alloc(2);

	zassert_not_null(p1);

	vpage_free(p1);
}

#define PAGE_SZ CONFIG_MM_DRV_PAGE_SIZE

/* Check that a range is inside the virtual page region */
static void vpage_check_in_region(void *ptr, unsigned int pages)
{
	const struct sys_mm_drv_region *region = vpage_get_region();
	uintptr_t start = POINTER_TO_UINT(region->addr);

	zassert_true(IS_ALIGNED(ptr, PAGE_SZ), "%p not page aligned", ptr);
	zassert_true(POINTER_TO_UINT(ptr) >= start &&
		     POINTER_TO_UINT(ptr) + pages * PAGE_SZ <= start + region->size,
		     "%p outside of the region", ptr);
}

/*
 * Freeing an allocation, that isn't the last one, moves the last allocation
 * element into its place. Free the first of three allocations, allocate
 * again and check that the third allocation is still tracked, i.e. that its
 * pages are really freed by vpage_free().
 */
ZTEST(sof_boot, vpage_free_order)
{
	void *a = vpage_alloc(1);
	void *b = vpage_alloc(1);
	void *c = vpage_alloc(1);
	void *d;

	zassert_not_null(a);
	zassert_not_null(b);
	zassert_not_null(c);

	vpage_free(a);

	d = vpage_alloc(1);
	zassert_not_null(d);

	vpage_free(c);

	/* only succeeds if the pages of c have been freed */
	zassert_ok(vpage_reserve_at(c, 1), "pages at %p not freed", c);
	vpage_release(c);

	vpage_free(b);
	vpage_free(d);
}

/* Allocated pages are mapped, reserved pages aren't */
ZTEST(sof_boot, vpage_reserve)
{
	uintptr_t phys;
	void *p;

	zassert_not_null(vpage_get_region());
	zassert_is_null(vpage_reserve(0));

	p = vpage_alloc(1);
	zassert_not_null(p);
	vpage_check_in_region(p, 1);
	zassert_ok(sys_mm_drv_page_phys_get(p, &phys), "allocated page %p not mapped", p);
	vpage_free(p);

	p = vpage_reserve(2);
	zassert_not_null(p);
	vpage_check_in_region(p, 2);
	zassert_not_ok(sys_mm_drv_page_phys_get(p, &phys), "reserved page %p mapped", p);
	zassert_not_ok(sys_mm_drv_page_phys_get((uint8_t *)p + PAGE_SZ, &phys),
		       "reserved page %p mapped", (uint8_t *)p + PAGE_SZ);

	/* neither the whole range nor a part of it can be reserved again */
	zassert_not_ok(vpage_reserve_at(p, 2));
	zassert_not_ok(vpage_reserve_at((uint8_t *)p + PAGE_SZ, 1));

	vpage_release(p);

	/* the whole range is free again after release */
	zassert_ok(vpage_reserve_at(p, 2));
	vpage_release(p);
}

/* vpage_reserve_at() only accepts page aligned ranges within the region */
ZTEST(sof_boot, vpage_reserve_at_invalid)
{
	const struct sys_mm_drv_region *region = vpage_get_region();
	uint8_t *start, *end;

	zassert_not_null(region);

	start = region->addr;
	end = start + region->size;

	zassert_ok(vpage_reserve_at(start, 0));
	zassert_equal(vpage_reserve_at(start - PAGE_SZ, 1), -EINVAL);
	zassert_equal(vpage_reserve_at(end, 1), -EINVAL);
	zassert_equal(vpage_reserve_at(end - PAGE_SZ, 2), -EINVAL);

	/* CHECKIF() asserts or skips the check in some configurations */
	if (!IS_ENABLED(CONFIG_ASSERT_ON_ERRORS) && !IS_ENABLED(CONFIG_NO_RUNTIME_CHECKS))
		zassert_equal(vpage_reserve_at(start + 1, 1), -EINVAL);
}
