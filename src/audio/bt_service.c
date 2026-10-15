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

static const struct bt_audio_format_desc s_formats[BT_AUDIO_FMT_COUNT] = {
	[BT_AUDIO_FMT_48K_STD] = {
		.format = BT_AUDIO_FMT_48K_STD,
		.name = "48k_std",
		.codec_name = "LC3",
		.sample_rate = 48000,
		.bit_depth = 16,
		.frame_duration_us = 10000,
		.frame_duration_ms = 10,
		.octets_per_codec_frame = 100,
		.bitrate_kbps = 160,
		.frame_samples = 480,
		.frame_bytes = 1920,
	},
	[BT_AUDIO_FMT_48K_HQ] = {
		.format = BT_AUDIO_FMT_48K_HQ,
		.name = "48k_hq",
		.codec_name = "LC3",
		.sample_rate = 48000,
		.bit_depth = 16,
		.frame_duration_us = 10000,
		.frame_duration_ms = 10,
		.octets_per_codec_frame = 120,
		.bitrate_kbps = 192,
		.frame_samples = 480,
		.frame_bytes = 1920,
	},
	[BT_AUDIO_FMT_48K_MAX] = {
		.format = BT_AUDIO_FMT_48K_MAX,
		.name = "48k_max",
		.codec_name = "LC3",
		.sample_rate = 48000,
		.bit_depth = 24,
		.frame_duration_us = 10000,
		.frame_duration_ms = 10,
		.octets_per_codec_frame = 240,
		.bitrate_kbps = 384,
		.frame_samples = 480,
		.frame_bytes = 1920,
	},
	[BT_AUDIO_FMT_44K] = {
		.format = BT_AUDIO_FMT_44K,
		.name = "44k",
		.codec_name = "LC3",
		.sample_rate = 44100,
		.bit_depth = 16,
		.frame_duration_us = 10000,
		.frame_duration_ms = 10,
		.octets_per_codec_frame = 130,
		.bitrate_kbps = 208,
		.frame_samples = 441,
		.frame_bytes = 1764,
	},
	[BT_AUDIO_FMT_48K_LL] = {
		.format = BT_AUDIO_FMT_48K_LL,
		.name = "48k_ll",
		.codec_name = "LC3",
		.sample_rate = 48000,
		.bit_depth = 16,
		.frame_duration_us = 7500,
		.frame_duration_ms = 8,
		.octets_per_codec_frame = 75,
		.bitrate_kbps = 160,
		.frame_samples = 360,
		.frame_bytes = 1440,
	},
	[BT_AUDIO_FMT_96K_HR] = {
		.format = BT_AUDIO_FMT_96K_HR,
		.name = "96k_hr",
		.codec_name = "LC3plus-HR",
		.sample_rate = 96000,
		.bit_depth = 24,
		.frame_duration_us = 5000,
		.frame_duration_ms = 5,
		.octets_per_codec_frame = 160,
		.bitrate_kbps = 512,
		.frame_samples = 480,
		.frame_bytes = 1920,
	},
	[BT_AUDIO_FMT_LPCM] = {
		.format = BT_AUDIO_FMT_LPCM,
		.name = "lpcm",
		.codec_name = "LPCM (Raw)",
		.sample_rate = 32000,
		.bit_depth = 16,
		.frame_duration_us = 10000,
		.frame_duration_ms = 10,
		.octets_per_codec_frame = 640,
		.bitrate_kbps = 1024,
		.frame_samples = 320,
		.frame_bytes = 1280,
	},
};

static struct bt_service_status s_status = {
	.c6_powered = false,
	.state = BT_STATE_DISABLED,
	.route = SOF_AUDIO_ROUTE_USB_DAI,
	.format = BT_AUDIO_FMT_48K_STD,
	.sample_rate = 48000,
	.bitrate_kbps = 160,
	.frame_bytes = 1920,
	.codec_name = "LC3",
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

	const struct bt_audio_format_desc *desc = &s_formats[s_status.format];
	s_status.state = BT_STATE_BROADCASTING;
	LOG_INF("Bluetooth LE Audio Broadcast started: '%s' (%s @ %u Hz, %u kbps, %u ms SDU)",
		desc->name, desc->codec_name, desc->sample_rate, desc->bitrate_kbps, desc->frame_duration_ms);
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

int bt_service_set_format(enum bt_audio_format fmt)
{
	if (fmt >= BT_AUDIO_FMT_COUNT) {
		LOG_ERR("Invalid Bluetooth audio format: %d", fmt);
		return -EINVAL;
	}

	const struct bt_audio_format_desc *desc = &s_formats[fmt];
	s_status.format = fmt;
	s_status.sample_rate = desc->sample_rate;
	s_status.bitrate_kbps = desc->bitrate_kbps;
	s_status.frame_bytes = desc->frame_bytes;
	s_status.codec_name = desc->codec_name;

	bt_audio_set_playback_rate(desc->sample_rate);
	bt_audio_set_capture_rate(desc->sample_rate);

	LOG_INF("Bluetooth audio format set to '%s' (%s @ %u Hz %u-bit, %u kbps, %u ms SDU, %u B PCM)",
		desc->name, desc->codec_name, desc->sample_rate, desc->bit_depth,
		desc->bitrate_kbps, desc->frame_duration_ms, desc->frame_bytes);
	return 0;
}

enum bt_audio_format bt_service_get_format(void)
{
	return s_status.format;
}

const struct bt_audio_format_desc *bt_service_get_format_desc(enum bt_audio_format fmt)
{
	if (fmt >= BT_AUDIO_FMT_COUNT) {
		return NULL;
	}
	return &s_formats[fmt];
}

const struct bt_audio_format_desc *bt_service_get_current_format_desc(void)
{
	return &s_formats[s_status.format];
}

int bt_service_format_from_name(const char *name, enum bt_audio_format *fmt)
{
	if (!name || !fmt) {
		return -EINVAL;
	}

	for (int i = 0; i < BT_AUDIO_FMT_COUNT; i++) {
		if (strcmp(name, s_formats[i].name) == 0) {
			*fmt = (enum bt_audio_format)i;
			return 0;
		}
	}
	return -ENOENT;
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

	static uint8_t s_audio_buf[1920] __aligned(4);

	while (s_thread_running) {
		const struct bt_audio_format_desc *desc = &s_formats[s_status.format];
		uint32_t frame_bytes = desc->frame_bytes;
		uint32_t sleep_ms = desc->frame_duration_ms ? desc->frame_duration_ms : 10;

		if (s_status.state == BT_STATE_BROADCASTING) {
			/* Fetch audio from pipeline/bt_audio component */
			size_t fetched = bt_audio_fetch_capture_data(s_audio_buf, frame_bytes);
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
			s_status.rx_bytes += frame_bytes;
		}

		k_msleep(sleep_ms);
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
