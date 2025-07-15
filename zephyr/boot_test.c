// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#include <zephyr/logging/log.h>
#include <sof/boot_test.h>

#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(sof_boot_test, LOG_LEVEL_DBG);

ZTEST_SUITE(sof_boot, NULL, NULL, NULL, NULL, NULL);

int sys_run_boot_tests(void)
{
	ztest_run_all(NULL, false, 1, 1);
	return 0;
}
SYS_INIT(sys_run_boot_tests, APPLICATION, 99);

