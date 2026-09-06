/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <sof/boot_test.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_PLATFORM_ESP32P4)
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_uac2.h>
#include <zephyr/device.h>
#include <sample_usbd.h>
#include <rtos/sof.h>
#include <sof/init.h>
#include <sof/audio/pipeline/sof_static_pipeline.h>
#endif

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

#if defined(CONFIG_PLATFORM_ESP32P4)
	/* Initialize static audio pipelines (EQ+DRC Playback & TDFB+EQ Capture) */
	sof_static_pipelines_init(sof_get());

	/* Register UAC2 class callbacks before initializing USB stack */
	const struct device *uac2_dev = DEVICE_DT_GET_ONE(zephyr_uac2);
	if (device_is_ready(uac2_dev)) {
		usbd_uac2_set_ops(uac2_dev, sof_get_uac2_ops(), NULL);
	} else {
		LOG_ERR("UAC2 device not ready");
	}

	/* Initialize USB device stack and UAC2 class */
	struct usbd_context *sample_usbd = sample_usbd_init_device(NULL);
	if (sample_usbd) {
		usbd_enable(sample_usbd);
		LOG_INF("ESP32-P4 USB UAC2 device and SOF pipelines started");
	} else {
		LOG_ERR("Failed to initialize USB device context");
	}
#endif

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
#if CONFIG_SOF_BOOT_TEST && (defined(QEMU_BOOT_TESTS) || CONFIG_SOF_BOOT_TEST_STANDALONE)
	sof_run_boot_tests();
#if defined(QEMU_BOOT_TESTS)
	/* qemu_xtensa_exit() only exists for QEMU targets; a standalone
	 * boot test (e.g. native_sim) just returns from test_main()
	 */
	qemu_xtensa_exit(0);
#endif
#endif
}
#else
int main(int argc, char *argv[])
{
	return sof_app_main();
}
#endif
