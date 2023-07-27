// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Guennadi Liakhovetski <guennadi.liakhovetski@linux.intel.com>
 */

#include <errno.h>
#include <stdbool.h>

#include <adsp_memory_regions.h>
#include <sof/boot_test.h>
#include <sof/lib/regions_mm.h>

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_DECLARE(sof_boot_test, CONFIG_SOF_LOG_LEVEL);

#define ALLOC_SIZE1 1616
#define ALLOC_SIZE2 26

static int vmh_test_single(bool span)
{
	struct vmh_heap *h = vmh_init_heap(NULL, MEM_REG_ATTR_CORE_HEAP, 0, span);

	if (!h)
		return -EINVAL;

	char *buf = vmh_alloc(h, ALLOC_SIZE1);
	int ret1;

	if (buf) {
		buf[0] = 0;
		buf[ALLOC_SIZE1 - 1] = 15;

		ret1 = vmh_free(h, buf);
		if (ret1 < 0)
			goto out;
	} else if (span) {
		ret1 = -ENOMEM;
		LOG_ERR("Failed to allocate %u in contiguous mode", ALLOC_SIZE1);
		goto out;
	} else {
		LOG_WRN("Ignoring failure to allocate %u in non-contiguous mode",
			ALLOC_SIZE1);
	}

	buf = vmh_alloc(h, ALLOC_SIZE2);

	if (!buf) {
		ret1 = -ENOMEM;
		LOG_ERR("Failed to allocate %u", ALLOC_SIZE2);
		goto out;
	}

	buf[0] = 0;
	buf[ALLOC_SIZE2 - 1] = 15;

	ret1 = vmh_free(h, buf);
	if (ret1 < 0)
		LOG_ERR("Free error %d", ret1);

out:
	int ret2 = vmh_free_heap(h);

	if (ret2 < 0)
		LOG_ERR("Free heap error %d", ret2);

	if (!ret1)
		ret1 = ret2;

	return ret1;
}

static int vmh_test(void)
{
	int ret = vmh_test_single(false);

	if (ret < 0) {
		LOG_ERR("Non-contiguous test error %d", ret);
		return ret;
	}

	ret = vmh_test_single(true);
	if (ret < 0)
		LOG_ERR("Contiguous test error %d", ret);

	return ret;
}

ZTEST(sof_boot, virtual_memory_heap)
{
	int ret = vmh_test();

	TEST_CHECK_RET(ret, "virtual_memory_heap");
}
