// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sof/boot_test.h>
#include <sof/lib/vpage.h>

#include <zephyr/logging/log.h>
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
