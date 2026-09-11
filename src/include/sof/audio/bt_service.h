/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#ifndef __SOF_AUDIO_BT_SERVICE_H__
#define __SOF_AUDIO_BT_SERVICE_H__

#include <sof/audio/pipeline/sof_static_pipeline.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum bt_service_state {
	BT_STATE_DISABLED = 0,
	BT_STATE_READY = 1,
	BT_STATE_BROADCASTING = 2,
	BT_STATE_RECEIVING = 3,
	BT_STATE_SCANNING = 4,
};

enum bt_audio_format {
	BT_AUDIO_FMT_48K_STD = 0,  /* Standard BAP (48_2_1): 48 kHz / 16b @ 160 kbps, 10ms, 100 octets/ch */
	BT_AUDIO_FMT_48K_HQ = 1,   /* High-Quality BAP (48_4_1): 48 kHz / 16b @ 192 kbps, 10ms, 120 octets/ch */
	BT_AUDIO_FMT_48K_MAX = 2,  /* High-Bitrate Studio (48_MAX): 48 kHz / 24b @ 384 kbps, 10ms, 240 octets/ch */
	BT_AUDIO_FMT_44K = 3,      /* CD Audio BAP (441_2_1): 44.1 kHz / 16b @ 208 kbps, 10ms, 130 octets/ch */
	BT_AUDIO_FMT_48K_LL = 4,   /* Low-Latency BAP (48_1_1): 48 kHz / 16b @ 160 kbps, 7.5ms, 75 octets/ch */
	BT_AUDIO_FMT_96K_HR = 5,   /* LC3plus High-Resolution: 96 kHz / 24b @ 512 kbps, 5.0ms, 160 octets/ch */
	BT_AUDIO_FMT_LPCM = 6,     /* Direct Linear PCM: 32 kHz / 16b @ 1024 kbps, 10ms, 640 octets/ch */
	BT_AUDIO_FMT_COUNT
};

struct bt_audio_format_desc {
	enum bt_audio_format format;
	const char *name;
	const char *codec_name;
	uint32_t sample_rate;
	uint8_t bit_depth;
	uint32_t frame_duration_us;
	uint32_t frame_duration_ms;
	uint16_t octets_per_codec_frame;
	uint16_t bitrate_kbps;
	uint16_t frame_samples;
	uint16_t frame_bytes;
};

struct bt_service_status {
	bool c6_powered;
	enum bt_service_state state;
	enum sof_audio_route route;
	enum bt_audio_format format;
	uint32_t sample_rate;
	uint16_t bitrate_kbps;
	uint16_t frame_bytes;
	const char *codec_name;
	uint32_t tx_packets;
	uint32_t rx_packets;
	uint32_t tx_bytes;
	uint32_t rx_bytes;
	int8_t rssi;
};

int bt_service_init(void);
void bt_service_power_c6(bool enable);
int bt_service_start_broadcast(void);
int bt_service_stop_broadcast(void);
int bt_service_start_scan(void);
int bt_service_set_route(enum sof_audio_route route);
enum sof_audio_route bt_service_get_route(void);
int bt_service_set_format(enum bt_audio_format fmt);
enum bt_audio_format bt_service_get_format(void);
const struct bt_audio_format_desc *bt_service_get_format_desc(enum bt_audio_format fmt);
const struct bt_audio_format_desc *bt_service_get_current_format_desc(void);
int bt_service_format_from_name(const char *name, enum bt_audio_format *fmt);
void bt_service_get_status(struct bt_service_status *status);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_AUDIO_BT_SERVICE_H__ */
