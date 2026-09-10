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

struct bt_service_status {
	bool c6_powered;
	enum bt_service_state state;
	enum sof_audio_route route;
	uint32_t sample_rate;
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
void bt_service_get_status(struct bt_service_status *status);

#ifdef __cplusplus
}
#endif

#endif /* __SOF_AUDIO_BT_SERVICE_H__ */
