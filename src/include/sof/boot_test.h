/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_BOOT_TEST_H__
#define __SOF_BOOT_TEST_H__

#include <stdbool.h>

#define TEST_RUN_ONCE(fn, ...) do { \
	static bool once; \
	if (!once) { \
		once = true; \
		fn(__VA_ARGS__); \
	} \
} while (0)

#define TEST_CHECK_RET(ret, testname) do { \
	if ((ret) < 0) { \
		LOG_ERR(testname " failed: %d", (ret)); \
		ztest_test_fail(); \
	} else { \
		ztest_test_pass(); \
	} \
} while (0)

#endif
