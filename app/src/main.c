/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <sof/boot_test.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_STATIC_PIPELINE)
#include <zephyr/device.h>
#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
#include <zephyr/usb/usbd.h>
#include <sample_usbd.h>
#endif
#if defined(CONFIG_USBD_AUDIO2_CLASS)
#include <zephyr/usb/class/usbd_uac2.h>
#endif
#include <sof/platform.h>
#include <sof/audio/pipeline/static_pipeline.h>
#if defined(CONFIG_COMP_BT_AUDIO)
#include <sof/audio/bt_service.h>
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led0))
#include <zephyr/drivers/gpio.h>
static const struct gpio_dt_spec s_led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#endif
#endif

#if defined(CONFIG_PLATFORM_ESP32S3)
#include <soc/rtc_cntl_reg.h>
#include <esp_system.h>
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

#if defined(CONFIG_PLATFORM_ESP32S3)
#define DOUBLE_TAP_MAGIC 0x424F4F54 /* 'BOOT' */

static struct k_timer s_double_tap_timer;

static void double_tap_timer_expiry(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	REG_WRITE(RTC_CNTL_STORE0_REG, 0);
}

static void check_double_tap_bootloader(void)
{
	uint32_t val = REG_READ(RTC_CNTL_STORE0_REG);

	if (val == DOUBLE_TAP_MAGIC) {
		REG_WRITE(RTC_CNTL_STORE0_REG, 0);
		printk("\n*** DOUBLE-TAP RESET DETECTED: Entering ROM Download Mode ***\n");
		k_msleep(50);
		REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
		esp_restart();
	}

	REG_WRITE(RTC_CNTL_STORE0_REG, DOUBLE_TAP_MAGIC);
	k_timer_init(&s_double_tap_timer, double_tap_timer_expiry, NULL);
	k_timer_start(&s_double_tap_timer, K_MSEC(1500), K_NO_WAIT);
}
#endif

#if defined(CONFIG_PLATFORM_RP2350)
extern const struct sof_static_topology g_rp2350_static_topology;
#elif defined(CONFIG_PLATFORM_NRF54L15)
extern const struct sof_static_topology g_nrf54l15_static_topology;
#elif defined(CONFIG_PLATFORM_NRF54LM20)
extern const struct sof_static_topology g_nrf54lm20_static_topology;
#elif defined(CONFIG_PLATFORM_ESP32P4)
extern const struct sof_static_topology g_esp32p4_static_topology;
#elif defined(CONFIG_PLATFORM_ESP32S3)
extern const struct sof_static_topology g_esp32s3_static_topology;
#endif

/**
 * TODO: Here comes SOF initialization
 */

static int sof_app_main(void)
{
	int ret;

#if defined(CONFIG_PLATFORM_ESP32S3)
	check_double_tap_bootloader();
#endif

#if defined(CONFIG_STATIC_PIPELINE) && DT_NODE_EXISTS(DT_ALIAS(led0))
	if (s_led0.port && device_is_ready(s_led0.port))
		gpio_pin_configure_dt(&s_led0, GPIO_OUTPUT_ACTIVE);
#endif

	LOG_INF("SOF on %s", CONFIG_BOARD);

	/* sof_main is actually SOF initialization */
	ret = sof_main(0, NULL);
	if (ret) {
		LOG_ERR("SOF initialization failed");
	}

	LOG_INF("SOF initialized");

#if defined(CONFIG_STATIC_PIPELINE)
#if defined(CONFIG_PLATFORM_TEENSY41)
	ret = sof_static_topology_init(&g_teensy41_static_topology);
#elif defined(CONFIG_PLATFORM_RP2350)
	ret = sof_static_topology_init(&g_rp2350_static_topology);
#elif defined(CONFIG_PLATFORM_NRF54L15)
	ret = sof_static_topology_init(&g_nrf54l15_static_topology);
#elif defined(CONFIG_PLATFORM_NRF54LM20)
	ret = sof_static_topology_init(&g_nrf54lm20_static_topology);
#elif defined(CONFIG_PLATFORM_ESP32P4)
	ret = sof_static_topology_init(&g_esp32p4_static_topology);
#elif defined(CONFIG_PLATFORM_ESP32S3)
	ret = sof_static_topology_init(&g_esp32s3_static_topology);
#else
	ret = 0;
#endif
	if (ret < 0) {
		LOG_ERR("Static topology initialization failed: %d", ret);
		return ret;
	}

#if defined(CONFIG_PLATFORM_NRF54L15)
	sof_static_pipeline_start(1);
#elif defined(CONFIG_PLATFORM_NRF54LM20)
	sof_static_pipeline_start(1);
	sof_static_pipeline_start(2);
#endif

#if defined(CONFIG_USBD_AUDIO2_CLASS)
	const struct device *uac2_dev = DEVICE_DT_GET_ONE(zephyr_uac2);

	if (device_is_ready(uac2_dev)) {
		usbd_uac2_set_ops(uac2_dev, sof_static_uac2_get_ops(), NULL);
	} else {
		LOG_ERR("UAC2 device not ready");
	}
#endif

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
	struct usbd_context *sample_usbd = sample_usbd_init_device(NULL);

	if (sample_usbd) {
		usbd_enable(sample_usbd);
		LOG_INF("SOF USB device and static pipelines started");
	} else {
		LOG_ERR("Failed to initialize USB device context");
	}
#endif

#if defined(CONFIG_COMP_BT_AUDIO)
	bt_service_init();
#endif
#endif /* CONFIG_STATIC_PIPELINE */

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
int main(void)
{
	return sof_app_main();
}
#endif
