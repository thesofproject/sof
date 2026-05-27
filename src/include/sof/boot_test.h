/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_BOOT_TEST_H__
#define __SOF_BOOT_TEST_H__

#if !CONFIG_LIBRARY
#include <zephyr/logging/log.h>
#else
#define LOG_ERR(...) do {} while (0)
#endif
#include <stdbool.h>

struct k_thread;

#if CONFIG_SOF_BOOT_TEST
#define TEST_RUN_ONCE(fn, ...) do { \
	static bool once; \
	if (!once) { \
		once = true; \
		fn(__VA_ARGS__); \
	} \
} while (0)
#else
#define TEST_RUN_ONCE(fn, ...) do {} while (0)
#endif

#define TEST_CHECK_RET(ret, testname) do { \
	if ((ret) < 0) { \
		LOG_ERR(testname " failed: %d", (ret)); \
		ztest_test_fail(); \
	} else { \
		ztest_test_pass(); \
	} \
} while (0)

void sof_run_boot_tests(void);

/**
 * Mark a boot-test thread as expected to trigger a fatal error.
 *
 * @param thread Thread that is allowed to fault once, or NULL to clear.
 */
#if CONFIG_SOF_BOOT_TEST
void sof_boot_test_set_fault_valid(struct k_thread *thread);
#else
static inline void sof_boot_test_set_fault_valid(struct k_thread *thread)
{
	(void)thread;
}
#endif

#endif
