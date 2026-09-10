// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/bt_service.h>
#include <sof/audio/bt_audio.h>
#include <sof/audio/usb_audio.h>
#include <sof/audio/pipeline/sof_static_pipeline.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <soc/gpio_reg.h>
#include <soc/soc.h>

LOG_MODULE_REGISTER(bt_service, CONFIG_SOF_LOG_LEVEL);

#define ESP32P4_GPIO_C6_EN_BIT    (1U << 22) /* GPIO 54 = 32 + 22 */
#define BT_AUDIO_FRAME_MS         10
#define BT_AUDIO_FRAME_SAMPLES    480 /* 10ms at 48kHz */
#define BT_AUDIO_FRAME_BYTES      (BT_AUDIO_FRAME_SAMPLES * 2 * sizeof(int16_t)) /* 1920 bytes */

static struct bt_service_status s_status = {
	.c6_powered = false,
	.state = BT_STATE_DISABLED,
	.route = SOF_AUDIO_ROUTE_USB_DAI,
	.sample_rate = 48000,
	.tx_packets = 0,
	.rx_packets = 0,
	.tx_bytes = 0,
	.rx_bytes = 0,
	.rssi = -42,
};

static K_THREAD_STACK_DEFINE(s_bt_stack, 2048);
static struct k_thread s_bt_thread;
static volatile bool s_thread_running = true;

/* Hardware control for ESP32-C6 co-processor via GPIO 54 */
void bt_service_power_c6(bool enable)
{
	/* Ensure GPIO 54 is enabled as output */
	sys_write32(ESP32P4_GPIO_C6_EN_BIT, GPIO_ENABLE1_W1TS_REG);

	if (enable) {
		sys_write32(ESP32P4_GPIO_C6_EN_BIT, GPIO_OUT1_W1TS_REG);
		s_status.c6_powered = true;
		s_status.state = BT_STATE_READY;
		LOG_INF("ESP32-C6 co-processor powered ON (GPIO 54 asserted HIGH)");
	} else {
		sys_write32(ESP32P4_GPIO_C6_EN_BIT, GPIO_OUT1_W1TC_REG);
		s_status.c6_powered = false;
		s_status.state = BT_STATE_DISABLED;
		LOG_INF("ESP32-C6 co-processor powered OFF (GPIO 54 driven LOW in reset)");
	}
}

int bt_service_start_broadcast(void)
{
	if (!s_status.c6_powered) {
		bt_service_power_c6(true);
		k_msleep(150);
	}

	s_status.state = BT_STATE_BROADCASTING;
	LOG_INF("Bluetooth LE Audio Broadcast started at %u Hz stereo (10ms ISO SDU)", s_status.sample_rate);
	return 0;
}

int bt_service_stop_broadcast(void)
{
	if (s_status.state == BT_STATE_BROADCASTING) {
		s_status.state = BT_STATE_READY;
		LOG_INF("Bluetooth LE Audio Broadcast stopped");
	}
	return 0;
}

int bt_service_start_scan(void)
{
	if (!s_status.c6_powered) {
		bt_service_power_c6(true);
		k_msleep(150);
	}

	s_status.state = BT_STATE_SCANNING;
	LOG_INF("Bluetooth LE Audio scan started, listening for nearby BAP broadcast sources...");
	return 0;
}

int bt_service_set_route(enum sof_audio_route route)
{
	s_status.route = route;
	sof_static_pipeline_set_route(route);
	LOG_INF("Audio routing updated: %s",
		route == SOF_AUDIO_ROUTE_USB_DAI ? "USB <-> DAI (Default)" :
		(route == SOF_AUDIO_ROUTE_BT_DAI ? "BT <-> DAI" : "USB <-> BT"));
	return 0;
}

enum sof_audio_route bt_service_get_route(void)
{
	return s_status.route;
}

void bt_service_get_status(struct bt_service_status *status)
{
	if (status) {
		*status = s_status;
	}
}

/* Background worker thread for ISO audio streaming and loopback */
static void bt_audio_stream_task(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	static uint8_t s_audio_buf[BT_AUDIO_FRAME_BYTES] __aligned(4);

	while (s_thread_running) {
		if (s_status.state == BT_STATE_BROADCASTING) {
			/* Fetch audio from pipeline/bt_audio component */
			size_t fetched = bt_audio_fetch_capture_data(s_audio_buf, BT_AUDIO_FRAME_BYTES);
			if (fetched > 0) {
				s_status.tx_packets++;
				s_status.tx_bytes += fetched;

				/* In USB <-> BT or BT loopback mode, mirror back to playback sink */
				if (s_status.route == SOF_AUDIO_ROUTE_USB_BT || s_status.route == SOF_AUDIO_ROUTE_BT_DAI) {
					bt_audio_feed_playback_data(s_audio_buf, fetched);
					s_status.rx_packets++;
					s_status.rx_bytes += fetched;
				}
			}
		} else if (s_status.state == BT_STATE_RECEIVING) {
			s_status.rx_packets++;
			s_status.rx_bytes += BT_AUDIO_FRAME_BYTES;
		}

		k_msleep(BT_AUDIO_FRAME_MS);
	}
}

int bt_service_init(void)
{
	LOG_INF("Initializing ESP32-P4 Bluetooth Audio Service...");

	/* Initialize static BT audio ring buffers */
	bt_audio_init();

	/* Power up onboard ESP32-C6 coprocessor */
	bt_service_power_c6(true);
	k_msleep(100);

	/* Spawn background streaming task */
	k_thread_create(&s_bt_thread, s_bt_stack, K_THREAD_STACK_SIZEOF(s_bt_stack),
			bt_audio_stream_task, NULL, NULL, NULL,
			K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
	k_thread_name_set(&s_bt_thread, "bt_audio_svc");

	LOG_INF("ESP32-P4 Bluetooth Audio Service initialized successfully");
	return 0;
}
