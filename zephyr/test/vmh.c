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

static int vmh_test(void)
{
	struct vmh_heap *h = vmh_init_heap(NULL, MEM_REG_ATTR_CORE_HEAP, 0, false);

	if (!h)
		return -ENOMEM;

	char *buf = vmh_alloc(h, 1616);

	if (!buf)
		return -ENOMEM;

	buf[0] = 0;
	buf[1615] = 15;

	vmh_free(h, buf);

	return vmh_free_heap(h);
}

ZTEST(sof_boot, virtual_memory_heap)
{
	int ret = vmh_test();

	TEST_CHECK_RET(ret, "virtual_memory_heap");
}
