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

#if defined(CONFIG_PLATFORM_ESP32P4)
#include <soc/gpio_reg.h>
#include <soc/soc.h>
#endif

#if defined(CONFIG_BT)
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#endif

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

static K_THREAD_STACK_DEFINE(s_bt_stack, 3072);
static struct k_thread s_bt_thread;
static volatile bool s_thread_running = true;

#if defined(CONFIG_BT)
#define BT_UUID_SOF_AUDIO_VAL \
	BT_UUID_128_ENCODE(0x4c524700, 0x534f, 0x462d, 0x4175, 0x64696f537663)
#define BT_UUID_SOF_AUDIO_STREAM_VAL \
	BT_UUID_128_ENCODE(0x4c524701, 0x534f, 0x462d, 0x4175, 0x64696f537663)

#define BT_UUID_SOF_AUDIO        BT_UUID_DECLARE_128(BT_UUID_SOF_AUDIO_VAL)
#define BT_UUID_SOF_AUDIO_STREAM BT_UUID_DECLARE_128(BT_UUID_SOF_AUDIO_STREAM_VAL)
#endif

#if defined(CONFIG_PLATFORM_NRF54LM20) && defined(CONFIG_BT)
static struct bt_conn *s_active_conn;
static volatile bool s_audio_notif_enabled;

static void sof_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	s_audio_notif_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("SOF Audio stream notifications %s by receiver",
		s_audio_notif_enabled ? "ENABLED" : "DISABLED");
}

BT_GATT_SERVICE_DEFINE(sof_audio_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_SOF_AUDIO),
	BT_GATT_CHARACTERISTIC(BT_UUID_SOF_AUDIO_STREAM,
			       BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ,
			       NULL, NULL, NULL),
	BT_GATT_CCC(sof_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static void lm20_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_WRN("Connection failed (err 0x%02x)", err);
		return;
	}
	s_active_conn = bt_conn_ref(conn);
	s_status.state = BT_STATE_BROADCASTING;
	LOG_INF("Bluetooth receiver connected! Requesting 2M PHY & low-latency interval...");

#if defined(CONFIG_BT_USER_PHY_UPDATE)
	struct bt_conn_le_phy_param phy = {
		.options = BT_CONN_LE_PHY_OPT_NONE,
		.pref_tx_phy = BT_GAP_LE_PHY_2M,
		.pref_rx_phy = BT_GAP_LE_PHY_2M,
	};
	bt_conn_le_phy_update(conn, &phy);
#endif

	struct bt_le_conn_param conn_param = {
		.interval_min = 6,  /* 7.5 ms */
		.interval_max = 8,  /* 10.0 ms */
		.latency = 0,
		.timeout = 400,     /* 4 s */
	};
	bt_conn_le_param_update(conn, &conn_param);
}

static struct k_work_delayable s_adv_work;

static void adv_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	bt_service_start_broadcast();
}

static void lm20_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Bluetooth receiver disconnected (reason 0x%02x: %s)",
		reason, bt_hci_err_to_str(reason));
	s_audio_notif_enabled = false;
	if (s_active_conn) {
		bt_conn_unref(s_active_conn);
		s_active_conn = NULL;
	}
	s_status.state = BT_STATE_READY;
	k_work_schedule(&s_adv_work, K_MSEC(100));
}

BT_CONN_CB_DEFINE(lm20_conn_cbs) = {
	.connected = lm20_connected,
	.disconnected = lm20_disconnected,
};

static const struct bt_data lm20_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_data lm20_sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SOF_AUDIO_VAL),
};
#endif /* CONFIG_PLATFORM_NRF54LM20 && CONFIG_BT */

#if defined(CONFIG_PLATFORM_NRF54L15) && defined(CONFIG_BT)
static struct bt_conn *s_receiver_conn;
static struct bt_gatt_discover_params s_discover_params;
static struct bt_gatt_subscribe_params s_subscribe_params;
static struct bt_gatt_exchange_params s_mtu_exchange_params;
static struct bt_uuid_128 s_discover_uuid = BT_UUID_INIT_128(BT_UUID_SOF_AUDIO_STREAM_VAL);

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err,
			    struct bt_gatt_exchange_params *params)
{
	LOG_INF("[BT RX] MTU exchange %s, negotiated MTU: %u bytes",
		err == 0 ? "successful" : "failed",
		bt_gatt_get_mtu(conn));
}

static uint8_t sof_audio_notify_cb(struct bt_conn *conn,
				   struct bt_gatt_subscribe_params *params,
				   const void *data, uint16_t length)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(params);

	if (!data || length == 0) {
		return BT_GATT_ITER_STOP;
	}

	bt_audio_feed_playback_data(data, length);
	s_status.rx_packets++;
	s_status.rx_bytes += length;

	if (s_status.rx_packets == 1 || s_status.rx_packets % 500 == 0) {
		LOG_INF("[BT RX] Received packet #%u (%u bytes, total %u KB)",
			s_status.rx_packets, length, (uint32_t)(s_status.rx_bytes / 1024));
	}

	return BT_GATT_ITER_CONTINUE;
}

static uint8_t gatt_discover_cb(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				struct bt_gatt_discover_params *params)
{
	if (!attr) {
		LOG_DBG("GATT discovery completed");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	LOG_INF("[BT RX] Found characteristic attribute handle: %u", attr->handle);

	if (!bt_uuid_cmp(params->uuid, BT_UUID_SOF_AUDIO_STREAM)) {
		memcpy(&s_discover_uuid, BT_UUID_GATT_CCC, sizeof(s_discover_uuid));
		params->uuid = &s_discover_uuid.uuid;
		params->start_handle = attr->handle + 1;
		params->type = BT_GATT_DISCOVER_DESCRIPTOR;
		s_subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		int err = bt_gatt_discover(conn, params);
		if (err) {
			LOG_ERR("GATT CCC discovery failed: %d", err);
		}
	} else {
		s_subscribe_params.notify = sof_audio_notify_cb;
		s_subscribe_params.value = BT_GATT_CCC_NOTIFY;
		s_subscribe_params.ccc_handle = attr->handle;

		int err = bt_gatt_subscribe(conn, &s_subscribe_params);
		if (err && err != -EALREADY) {
			LOG_ERR("Failed to subscribe to audio notifications: %d", err);
		} else {
			s_status.state = BT_STATE_RECEIVING;
			LOG_INF("[BT RX] Subscribed to SOF Audio Stream notifications! Audio reception active.");
		}
		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_STOP;
}

static void l15_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_WRN("Connection to SOF Transmitter failed (err 0x%02x)", err);
		s_receiver_conn = NULL;
		bt_service_start_scan();
		return;
	}

	s_receiver_conn = bt_conn_ref(conn);
	LOG_INF("[BT RX] Connected to SOF Audio Transmitter! Discovering audio service...");

	s_mtu_exchange_params.func = mtu_exchange_cb;
	int err_mtu = bt_gatt_exchange_mtu(conn, &s_mtu_exchange_params);
	if (err_mtu) {
		LOG_WRN("[BT RX] Failed to initiate MTU exchange: %d", err_mtu);
	}

	memcpy(&s_discover_uuid, BT_UUID_SOF_AUDIO_STREAM, sizeof(s_discover_uuid));
	s_discover_params.uuid = &s_discover_uuid.uuid;
	s_discover_params.func = gatt_discover_cb;
	s_discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	s_discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	s_discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

	err = bt_gatt_discover(conn, &s_discover_params);
	if (err) {
		LOG_ERR("GATT discover failed: %d", err);
	}
}

static struct k_work_delayable s_scan_work;

static void scan_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	bt_service_start_scan();
}

static void l15_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("[BT RX] Disconnected from transmitter (reason 0x%02x: %s)",
		reason, bt_hci_err_to_str(reason));
	if (s_receiver_conn) {
		bt_conn_unref(s_receiver_conn);
		s_receiver_conn = NULL;
	}
	s_status.state = BT_STATE_SCANNING;
	k_work_schedule(&s_scan_work, K_MSEC(100));
}

BT_CONN_CB_DEFINE(l15_conn_cbs) = {
	.connected = l15_connected,
	.disconnected = l15_disconnected,
};

static bool parse_adv_name(struct bt_data *data, void *user_data)
{
	char *name = user_data;
	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE: {
		uint8_t len = MIN(data->data_len, 31);
		memcpy(name, data->data, len);
		name[len] = '\0';
		return false;
	}
	default:
		return true;
	}
}

static void scan_device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			      struct net_buf_simple *ad)
{
	ARG_UNUSED(type);
	if (s_receiver_conn) {
		return;
	}

	char dev_name[32] = {0};
	bt_data_parse(ad, parse_adv_name, dev_name);

	if (strcmp(dev_name, "SOF-Audio-LM20") == 0) {
		LOG_INF("[BT RX] Discovered '%s' (RSSI %d dBm), initiating connection...",
			dev_name, rssi);
		s_status.rssi = rssi;

		int err = bt_le_scan_stop();
		if (err) {
			LOG_WRN("Failed to stop scan: %d", err);
		}

		struct bt_conn *conn = NULL;
		err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
					BT_LE_CONN_PARAM_DEFAULT, &conn);
		if (err) {
			LOG_ERR("Failed to create connection: %d", err);
			k_work_schedule(&s_scan_work, K_MSEC(100));
		} else {
			bt_conn_unref(conn);
		}
	}
}
#endif /* CONFIG_PLATFORM_NRF54L15 && CONFIG_BT */

/* Hardware control for ESP32-C6 co-processor via GPIO 54 */
void bt_service_power_c6(bool enable)
{
#if defined(CONFIG_PLATFORM_ESP32P4)
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
#else
	s_status.c6_powered = enable;
	s_status.state = enable ? BT_STATE_READY : BT_STATE_DISABLED;
	LOG_INF("Nordic 2.4GHz on-chip Bluetooth radio %s", enable ? "READY" : "DISABLED");
#endif
}

int bt_service_start_broadcast(void)
{
#if defined(CONFIG_PLATFORM_NRF54LM20) && defined(CONFIG_BT)
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, lm20_ad, ARRAY_SIZE(lm20_ad),
				  lm20_sd, ARRAY_SIZE(lm20_sd));
	if (err && err != -EALREADY) {
		LOG_ERR("Failed to start advertising: %d", err);
		return err;
	}
	s_status.state = BT_STATE_BROADCASTING;
	LOG_INF("Bluetooth LE Audio advertising started as '%s'", CONFIG_BT_DEVICE_NAME);
	return 0;
#else
	if (!s_status.c6_powered) {
		bt_service_power_c6(true);
		k_msleep(150);
	}

	const struct bt_audio_format_desc *desc = &s_formats[s_status.format];
	s_status.state = BT_STATE_BROADCASTING;
	LOG_INF("Bluetooth LE Audio Broadcast started: '%s' (%s @ %u Hz, %u kbps, %u ms SDU)",
		desc->name, desc->codec_name, desc->sample_rate, desc->bitrate_kbps, desc->frame_duration_ms);
	return 0;
#endif
}

int bt_service_stop_broadcast(void)
{
#if defined(CONFIG_BT_BROADCASTER)
	bt_le_adv_stop();
#endif
	if (s_status.state == BT_STATE_BROADCASTING) {
		s_status.state = BT_STATE_READY;
		LOG_INF("Bluetooth LE Audio Broadcast stopped");
	}
	return 0;
}

int bt_service_start_scan(void)
{
#if defined(CONFIG_PLATFORM_NRF54L15) && defined(CONFIG_BT)
	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};

	int err = bt_le_scan_start(&scan_param, scan_device_found);
	if (err && err != -EALREADY) {
		LOG_ERR("Failed to start BLE scan: %d", err);
		return err;
	}
	s_status.state = BT_STATE_SCANNING;
	LOG_INF("[BT RX] Scanning for SOF Audio Transmitter ('SOF-Audio-LM20')...");
	return 0;
#else
	if (!s_status.c6_powered) {
		bt_service_power_c6(true);
		k_msleep(150);
	}

	s_status.state = BT_STATE_SCANNING;
	LOG_INF("Bluetooth LE Audio scan started, listening for nearby BAP broadcast sources...");
	return 0;
#endif
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
		uint32_t frame_bytes = desc->frame_bytes ? desc->frame_bytes : 192;
		uint32_t sleep_ms = desc->frame_duration_ms ? desc->frame_duration_ms : 10;

#if defined(CONFIG_PLATFORM_NRF54LM20) && defined(CONFIG_BT)
		/* Transmitter logic on nRF54LM20: fetch from pipeline and stream via GATT notify */
		if (s_status.state == BT_STATE_BROADCASTING || s_audio_notif_enabled) {
			size_t fetched = bt_audio_fetch_capture_data(s_audio_buf, frame_bytes);
			if (fetched > 0) {
				s_status.tx_packets++;
				s_status.tx_bytes += fetched;

				if (s_active_conn && s_audio_notif_enabled) {
					/* Stream over GATT notifications in chunks up to negotiated ATT MTU */
					uint16_t mtu = bt_gatt_get_mtu(s_active_conn);
					uint16_t max_chunk = (mtu > 3) ? (mtu - 3) : 20;
					uint16_t offset = 0;
					while (offset < fetched) {
						uint16_t chunk = MIN((uint16_t)(fetched - offset), max_chunk);
						int err = bt_gatt_notify(s_active_conn, &sof_audio_svc.attrs[1],
									 &s_audio_buf[offset], chunk);
						if (err) {
							break;
						}
						offset += chunk;
					}
				}

				if (s_status.tx_packets == 1 || s_status.tx_packets % 500 == 0) {
					LOG_INF("[BT TX] Streamed packet #%u (%u bytes, total %u KB, peer=%s)",
						s_status.tx_packets, (uint32_t)fetched,
						(uint32_t)(s_status.tx_bytes / 1024),
						s_audio_notif_enabled ? "STREAMING" : "IDLE");
				}
			}
		}
#elif defined(CONFIG_PLATFORM_NRF54L15) && defined(CONFIG_BT)
		/* Receiver telemetry on nRF54L15 */
		ARG_UNUSED(s_audio_buf);
		ARG_UNUSED(frame_bytes);
#else
		/* ESP32-P4 loopback logic */
		if (s_status.state == BT_STATE_BROADCASTING) {
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
#endif

		k_msleep(sleep_ms);
	}
}

int bt_service_init(void)
{
	LOG_INF("Initializing Bluetooth Audio Service...");

	/* Initialize static BT audio ring buffers */
	bt_audio_init();

#if defined(CONFIG_PLATFORM_ESP32P4)
	/* Power up onboard ESP32-C6 coprocessor */
	bt_service_power_c6(true);
	k_msleep(100);
#elif defined(CONFIG_BT)
	/* Initialize native Zephyr Bluetooth Subsystem on Nordic SoC */
	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Zephyr Bluetooth init failed: %d", err);
		return err;
	}
	s_status.c6_powered = true;
	s_status.state = BT_STATE_READY;
	LOG_INF("Zephyr native Bluetooth subsystem initialized successfully");

#if defined(CONFIG_PLATFORM_NRF54LM20)
	k_work_init_delayable(&s_adv_work, adv_work_handler);
	bt_service_start_broadcast();
#elif defined(CONFIG_PLATFORM_NRF54L15)
	k_work_init_delayable(&s_scan_work, scan_work_handler);
	bt_service_start_scan();
#endif
#endif

	/* Spawn background streaming task */
	k_thread_create(&s_bt_thread, s_bt_stack, K_THREAD_STACK_SIZEOF(s_bt_stack),
			bt_audio_stream_task, NULL, NULL, NULL,
			K_PRIO_PREEMPT(5), 0, K_NO_WAIT);
	k_thread_name_set(&s_bt_thread, "bt_audio_svc");

	LOG_INF("Bluetooth Audio Service initialized successfully");
	return 0;
}
