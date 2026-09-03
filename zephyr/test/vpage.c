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
