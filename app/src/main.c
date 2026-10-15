/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <sof/boot_test.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_PLATFORM_ESP32P4) || defined(CONFIG_PLATFORM_ESP32C6) || defined(CONFIG_PLATFORM_ESP32S3)
#include <rtos/sof.h>
#include <sof/init.h>
#include <sof/audio/pipeline/sof_static_pipeline.h>
#endif
#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
#include <zephyr/usb/usbd.h>
#include <zephyr/device.h>
#include <sample_usbd.h>
#if defined(CONFIG_USBD_AUDIO2_CLASS)
#include <zephyr/usb/class/usbd_uac2.h>
#endif
#if defined(CONFIG_COMP_BT_AUDIO)
#include <sof/audio/bt_service.h>
#endif
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

#if defined(CONFIG_PLATFORM_ESP32S3)
#include <soc/rtc_cntl_reg.h>
#include <esp_system.h>

#define DOUBLE_TAP_MAGIC 0x424F4F54 /* 'BOOT' */

static struct k_timer s_double_tap_timer;

static void double_tap_timer_expiry(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	REG_WRITE(RTC_CNTL_STORE0_REG, 0);
	LOG_DBG("Double-tap window closed");
}

static void check_double_tap_bootloader(void)
{
	uint32_t val = REG_READ(RTC_CNTL_STORE0_REG);
	if (val == DOUBLE_TAP_MAGIC) {
		REG_WRITE(RTC_CNTL_STORE0_REG, 0);
		printk("\n\n*** DOUBLE-TAP RESET DETECTED: Entering ROM Download Mode ***\n\n");
		k_msleep(50);
		REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
		esp_restart();
	}

	/* First reset: arm double-tap detection window for 1500 ms */
	REG_WRITE(RTC_CNTL_STORE0_REG, DOUBLE_TAP_MAGIC);
	k_timer_init(&s_double_tap_timer, double_tap_timer_expiry, NULL);
	k_timer_start(&s_double_tap_timer, K_MSEC(1500), K_NO_WAIT);
}
#endif

static int sof_app_main(void)
{
	int ret;

#if defined(CONFIG_PLATFORM_ESP32S3)
	check_double_tap_bootloader();
#endif

	LOG_INF("SOF on %s", CONFIG_BOARD);

	/* sof_main is actually SOF initialization */
	ret = sof_main(0, NULL);
	if (ret) {
		LOG_ERR("SOF initialization failed");
	}

	LOG_INF("SOF initialized");

#if defined(CONFIG_PLATFORM_ESP32P4) || defined(CONFIG_PLATFORM_ESP32C6) || defined(CONFIG_PLATFORM_ESP32S3)
	/* Initialize static audio pipelines */
	sof_static_pipelines_init(sof_get());

#if defined(CONFIG_USBD_AUDIO2_CLASS)
	/* Register UAC2 class callbacks before initializing USB stack */
	const struct device *uac2_dev = DEVICE_DT_GET_ONE(zephyr_uac2);
	if (device_is_ready(uac2_dev)) {
		usbd_uac2_set_ops(uac2_dev, sof_get_uac2_ops(), NULL);
	} else {
		LOG_ERR("UAC2 device not ready");
	}
#endif

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	/* Initialize USB device stack */
	struct usbd_context *sample_usbd = sample_usbd_init_device(NULL);
	if (sample_usbd) {
		usbd_enable(sample_usbd);
		LOG_INF("USB device and SOF pipelines started");
	} else {
		LOG_ERR("Failed to initialize USB device context");
	}
#endif

#if defined(CONFIG_COMP_BT_AUDIO)
	/* Initialize Bluetooth Audio Service and power on ESP32-C6 coprocessor */
	bt_service_init();
#endif
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
