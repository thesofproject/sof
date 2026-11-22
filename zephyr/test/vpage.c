// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sof/boot_test.h>
#include <sof/kernel/lib/vpage.h>

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

/* Freeing a non-last allocation must not orphan the last tracked element */
ZTEST(sof_boot, vpage_free_out_of_order)
{
	void *a = vpage_alloc(1);
	void *b = vpage_alloc(1);
	void *c, *d;

	zassert_not_null(a);
	zassert_not_null(b);

	/* the check below needs a and b to be neighbours, first fit normally does this */
	if ((uint8_t *)b != (uint8_t *)a + CONFIG_MM_DRV_PAGE_SIZE) {
		vpage_free(a);
		vpage_free(b);
		ztest_test_skip();
		return;
	}

	vpage_free(a);

	/* takes a's page; must not overwrite b's tracking element */
	c = vpage_alloc(1);
	zassert_equal(c, a);

	vpage_free(b);
	vpage_free(c);

	/* if b's element was lost its page is leaked and the 2 page range can't be reused */
	d = vpage_alloc(2);
	zassert_equal(d, a, "pages leaked after out of order free");
	vpage_free(d);
}

#define VPAGE_TEST_PAGE_SIZE	CONFIG_MM_DRV_PAGE_SIZE

/*
 * Size in pages of the largest single contiguous allocation that currently
 * succeeds. The allocator has no free page accessor, so this is how the tests
 * detect leaked virtual or physical pages: it must be the same before and after
 * a test has released everything it allocated.
 */
static unsigned int vpage_largest(void)
{
	unsigned int lo = 0, hi = 1;
	void *p;

	/* grow until an allocation fails */
	while (hi <= 0x8000) {
		p = vpage_alloc(hi);
		if (!p)
			break;
		vpage_free(p);
		lo = hi;
		hi <<= 1;
	}

	/* binary search the limit between the last success and the first failure */
	while (hi - lo > 1) {
		unsigned int mid = lo + (hi - lo) / 2;

		p = vpage_alloc(mid);
		if (p) {
			vpage_free(p);
			lo = mid;
		} else {
			hi = mid;
		}
	}

	return lo;
}

static void vpage_fill(void *ptr, unsigned int pages, uint8_t seed)
{
	uint8_t *p = ptr;

	for (size_t i = 0; i < pages * VPAGE_TEST_PAGE_SIZE; i += 61)
		p[i] = (uint8_t)(seed + i);
}

static bool vpage_check(void *ptr, unsigned int pages, uint8_t seed)
{
	uint8_t *p = ptr;

	for (size_t i = 0; i < pages * VPAGE_TEST_PAGE_SIZE; i += 61)
		if (p[i] != (uint8_t)(seed + i))
			return false;

	return true;
}

/* 0 pages is not an allocation */
ZTEST(sof_boot, vpage_alloc_zero)
{
	zassert_is_null(vpage_alloc(0));
}

/* Allocations are page aligned, contiguous, fully mapped and writable */
ZTEST(sof_boot, vpage_contiguous_rw)
{
	const unsigned int pages = 8;
	uint8_t *p = vpage_alloc(pages);

	zassert_not_null(p);
	zassert_true(IS_ALIGNED(p, VPAGE_TEST_PAGE_SIZE));

	/* touch every page, a hole in the mapping would fault */
	for (unsigned int i = 0; i < pages; i++)
		memset(p + i * VPAGE_TEST_PAGE_SIZE, 0xa0 + i, VPAGE_TEST_PAGE_SIZE);

	for (unsigned int i = 0; i < pages; i++)
		for (size_t j = 0; j < VPAGE_TEST_PAGE_SIZE; j += 37)
			zassert_equal(p[i * VPAGE_TEST_PAGE_SIZE + j], (uint8_t)(0xa0 + i));

	vpage_free(p);
}

/* Asking for more than is available must fail cleanly and not leak anything */
ZTEST(sof_boot, vpage_overcommit)
{
	unsigned int max = vpage_largest();
	void *all, *extra;

	zassert_true(max > 0, "no vpages available");

	/* one page more than the largest block, and absurd sizes */
	zassert_is_null(vpage_alloc(max + 1));
	zassert_is_null(vpage_alloc(0x10000));
	zassert_is_null(vpage_alloc(UINT_MAX));

	/* failures must not have consumed anything */
	zassert_equal(vpage_largest(), max, "failed allocation leaked pages");

	/* take everything, then there must be nothing left */
	all = vpage_alloc(max);
	zassert_not_null(all);
	vpage_fill(all, max, 0x11);

	extra = vpage_alloc(1);
	zassert_is_null(extra, "allocated beyond the available pages");

	zassert_true(vpage_check(all, max, 0x11));
	vpage_free(all);

	zassert_equal(vpage_largest(), max, "pages leaked after full allocation");
}

/* Repeated alloc/free of mixed sizes and orders must return all pages */
ZTEST(sof_boot, vpage_no_leak)
{
	const unsigned int sizes[] = { 1, 2, 3, 5, 8, 1, 4 };
	void *p[ARRAY_SIZE(sizes)];
	unsigned int max = vpage_largest();

	zassert_true(max > 0, "no vpages available");

	for (unsigned int round = 0; round < 32; round++) {
		for (unsigned int i = 0; i < ARRAY_SIZE(sizes); i++) {
			p[i] = vpage_alloc(sizes[i]);
			zassert_not_null(p[i]);
			vpage_fill(p[i], sizes[i], round + i);
		}

		for (unsigned int i = 0; i < ARRAY_SIZE(sizes); i++)
			zassert_true(vpage_check(p[i], sizes[i], round + i), "data corrupted");

		/* alternate between forward and reverse release order */
		for (unsigned int i = 0; i < ARRAY_SIZE(sizes); i++)
			vpage_free(p[(round & 1) ? ARRAY_SIZE(sizes) - 1 - i : i]);
	}

	zassert_equal(vpage_largest(), max, "pages leaked");
}

/* Fragmented (sparse) free space: holes are reused and never overlap live pages */
ZTEST(sof_boot, vpage_sparse)
{
	void *p[8];
	void *hole[4];
	void *big, *refill;
	unsigned int max = vpage_largest();
	unsigned int i, h;

	for (i = 0; i < ARRAY_SIZE(p); i++) {
		p[i] = vpage_alloc(1);
		zassert_not_null(p[i]);
		vpage_fill(p[i], 1, 0x30 + i);
	}

	/*
	 * The hole checks below rely on first fit giving back to back pages, which
	 * holds unless other users have left free gaps below.
	 */
	for (i = 1; i < ARRAY_SIZE(p); i++) {
		if ((uint8_t *)p[i] != (uint8_t *)p[i - 1] + VPAGE_TEST_PAGE_SIZE) {
			for (i = 0; i < ARRAY_SIZE(p); i++)
				vpage_free(p[i]);
			ztest_test_skip();
			return;
		}
	}

	/* free every other page, leaving single page holes between live pages */
	for (i = 0, h = 0; i < ARRAY_SIZE(p); i += 2, h++) {
		hole[h] = p[i];
		vpage_free(p[i]);
		p[i] = NULL;
	}

	/* a 2 page block can't fit in a hole and must not overlap a live page */
	big = vpage_alloc(2);
	zassert_not_null(big);
	vpage_fill(big, 2, 0x70);
	for (i = 1; i < ARRAY_SIZE(p); i += 2) {
		zassert_true((uint8_t *)p[i] + VPAGE_TEST_PAGE_SIZE <= (uint8_t *)big ||
			     (uint8_t *)big + 2 * VPAGE_TEST_PAGE_SIZE <= (uint8_t *)p[i],
			     "2 page block overlaps live page %u", i);
	}

	/* every single page allocation must take a hole while holes remain */
	for (h = 0; h < ARRAY_SIZE(hole); h++) {
		bool found = false;

		refill = vpage_alloc(1);
		zassert_not_null(refill);
		for (unsigned int k = 0; k < ARRAY_SIZE(hole); k++)
			if (refill == hole[k]) {
				hole[k] = NULL;
				found = true;
			}
		zassert_true(found, "single page didn't reuse a hole");
		vpage_fill(refill, 1, 0x50 + h);
		/* keep it for release below */
		p[h * 2] = refill;
	}

	/* nothing was overwritten while the holes were being reused */
	for (i = 1; i < ARRAY_SIZE(p); i += 2)
		zassert_true(vpage_check(p[i], 1, 0x30 + i), "live page %u corrupted", i);
	zassert_true(vpage_check(big, 2, 0x70));

	for (i = 0; i < ARRAY_SIZE(p); i++)
		vpage_free(p[i]);
	vpage_free(big);

	zassert_equal(vpage_largest(), max, "pages leaked");
}

/* Invalid frees must be rejected without disturbing valid allocations */
ZTEST(sof_boot, vpage_invalid_free)
{
	unsigned int max = vpage_largest();
	uint8_t *p = vpage_alloc(2);

	zassert_not_null(p);
	vpage_fill(p, 2, 0x42);

	/* NULL, unaligned, inside an allocation but not its start */
	vpage_free(NULL);
	vpage_free(p + 1);
	vpage_free(p + VPAGE_TEST_PAGE_SIZE);

	zassert_true(vpage_check(p, 2, 0x42), "allocation disturbed by invalid free");

	vpage_free(p);

	/* double free must not release anything else or corrupt the tracking */
	vpage_free(p);

	zassert_equal(vpage_largest(), max, "invalid free changed the available pages");
}

/* Running out of tracking elements must fail cleanly and recover on free */
ZTEST(sof_boot, vpage_max_allocs)
{
	void *p[CONFIG_SOF_VPAGE_MAX_ALLOCS + 1];
	unsigned int max = vpage_largest();
	unsigned int n;

	for (n = 0; n < ARRAY_SIZE(p); n++) {
		p[n] = vpage_alloc(1);
		if (!p[n])
			break;
	}

	/* either the elements or the pages must have run out before this many */
	zassert_true(n < ARRAY_SIZE(p), "allocated more than %u elements",
		     CONFIG_SOF_VPAGE_MAX_ALLOCS);
	zassert_true(n > 0);

	/* the failure must be sticky until something is freed */
	zassert_is_null(vpage_alloc(1));

	for (unsigned int i = 0; i < n; i++)
		vpage_free(p[i]);

	zassert_equal(vpage_largest(), max, "pages leaked after exhausting allocations");

	/* the allocator works again */
	p[0] = vpage_alloc(1);
	zassert_not_null(p[0]);
	vpage_free(p[0]);
}

/* Check that a range is inside the virtual page region */
static void vpage_check_in_region(void *ptr, unsigned int pages)
{
	const struct sys_mm_drv_region *region = vpage_get_region();
	uintptr_t start = POINTER_TO_UINT(region->addr);

	zassert_true(IS_ALIGNED(ptr, VPAGE_TEST_PAGE_SIZE), "%p not page aligned", ptr);
	zassert_true(POINTER_TO_UINT(ptr) >= start &&
		     POINTER_TO_UINT(ptr) + pages * VPAGE_TEST_PAGE_SIZE <= start + region->size,
		     "%p outside of the region", ptr);
}

/* Allocated pages are mapped, reserved pages aren't */
ZTEST(sof_boot, vpage_reserve)
{
	unsigned int max = vpage_largest();
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
	zassert_not_ok(sys_mm_drv_page_phys_get((uint8_t *)p + VPAGE_TEST_PAGE_SIZE, &phys),
		       "reserved page %p mapped", (uint8_t *)p + VPAGE_TEST_PAGE_SIZE);

	/* neither the whole range nor a part of it can be reserved again */
	zassert_not_ok(vpage_reserve_at(p, 2));
	zassert_not_ok(vpage_reserve_at((uint8_t *)p + VPAGE_TEST_PAGE_SIZE, 1));

	vpage_release(p);

	/* the whole range is free again after release */
	zassert_ok(vpage_reserve_at(p, 2));
	vpage_release(p);

	zassert_equal(vpage_largest(), max, "pages leaked after reserve/release");
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
	zassert_equal(vpage_reserve_at(start - VPAGE_TEST_PAGE_SIZE, 1), -EINVAL);
	zassert_equal(vpage_reserve_at(end, 1), -EINVAL);
	zassert_equal(vpage_reserve_at(end + VPAGE_TEST_PAGE_SIZE, 1), -EINVAL);
	zassert_equal(vpage_reserve_at(end - VPAGE_TEST_PAGE_SIZE, 2), -EINVAL);

	/* CHECKIF() asserts or skips the check in some configurations */
	if (!IS_ENABLED(CONFIG_ASSERT_ON_ERRORS) && !IS_ENABLED(CONFIG_NO_RUNTIME_CHECKS))
		zassert_equal(vpage_reserve_at(start + 1, 1), -EINVAL);
}
