/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <sof/boot_test.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* define qemu boot tests if any qemu target is defined, add targets to end */
#if defined(CONFIG_BOARD_QEMU_XTENSA_DC233C) ||\
    defined(CONFIG_BOARD_QEMU_XTENSA_DC233C_MMU)
#define QEMU_BOOT_TESTS
#endif

/**
 * Should be included from sof/schedule/task.h
 * but triggers include chain issue
 * FIXME
 */
int sof_main(int argc, char *argv[]);

/**
 * TODO: Here comes SOF initialization
 */

static int sof_app_main(void)
{
	int ret;

	LOG_INF("SOF on %s", CONFIG_BOARD);

	/* sof_main is actually SOF initialization */
	ret = sof_main(0, NULL);
	if (ret) {
		LOG_ERR("SOF initialization failed");
	}

	LOG_INF("SOF initialized");

#ifdef CONFIG_ARCH_POSIX_LIBFUZZER
	/* Workaround for an apparent timing bug in libfuzzer+asan.
	 * If the initial/main thread is allowed to return, ASAN will
	 * fairly reliably report a "stack overflow" where the ESP and
	 * EPC (instruction pointer!) registers are both set to the
	 * same value, which is non-sensical.  See some discussion in
	 * https://github.com/zephyrproject-rtos/zephyr/pull/52769
	 *
	 * But suspending the main thread instead of aborting is cheap
	 * and easy.
	 */
	k_thread_suspend(k_current_get());
#endif
	return 0;
}

#if defined(QEMU_BOOT_TESTS)
/* cleanly exit qemu so CI can continue and check test results */
static inline void qemu_xtensa_exit(int status)
{
	register int syscall_id __asm__ ("a2") = 1;      /* SYS_exit is 1 */
	register int exit_status __asm__ ("a3") = status;

	__asm__ __volatile__ (
		"simcall\n"
		:
		: "r" (syscall_id), "r" (exit_status)
		: "memory"
	);
}
#endif

#ifdef CONFIG_REBOOT
void sys_arch_reboot(int type)
{
#if defined(QEMU_BOOT_TESTS)
	qemu_xtensa_exit(type);
#endif
	while (1) {
		k_cpu_idle();
	}
}
#endif

#if CONFIG_ZTEST
void test_main(void)
{
	sof_app_main();
#if CONFIG_SOF_BOOT_TEST
	sof_run_boot_tests();
#if defined(QEMU_BOOT_TESTS)
	qemu_xtensa_exit(0);
#endif
#endif
}
#else
int main(void)
{
	return sof_app_main();
}
#endif
