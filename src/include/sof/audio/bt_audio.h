/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#ifndef __SOF_AUDIO_BT_AUDIO_H__
#define __SOF_AUDIO_BT_AUDIO_H__

#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/lib/uuid.h>
#include <zephyr/kernel.h>

/* UUID for Bluetooth Audio Component */
/* d274a0c8-4f81-4b72-a169-38ef61a8b940 */
#define SOF_BT_AUDIO_UUID \
	DECLARE_SOF_RT_UUID("bt_audio", 0xd274a0c8, 0x4f81, 0x4b72, \
			    0xa1, 0x69, 0x38, 0xef, 0x61, 0xa8, 0xb9, 0x40)

#define BT_AUDIO_RING_BUFFER_SIZE 1920
#define BT_AUDIO_PREBUFFER_BYTES  960 /* 5ms at 48kHz stereo 16-bit */

struct bt_audio_ring_buffer {
	uint8_t buf[BT_AUDIO_RING_BUFFER_SIZE];
	volatile uint32_t head;
	volatile uint32_t tail;
	volatile uint32_t count;
	struct k_spinlock lock;
};

struct bt_audio_data {
	struct bt_audio_ring_buffer ring;
	uint32_t sample_rate;
	uint32_t channels;
	uint32_t frame_bytes;
	uint32_t period_bytes;
	enum sof_ipc_stream_direction direction;
	bool active;
	bool started;
};

/* Bridge functions for feeding and fetching audio between BT stack and SOF pipeline */
void bt_audio_init(void);
void bt_audio_feed_playback_data(const void *src, size_t bytes);
size_t bt_audio_fetch_capture_data(void *dst, size_t bytes);
bool bt_audio_peek_capture_data(void *dst, size_t bytes);
void bt_audio_consume_capture_data(size_t bytes);
void bt_audio_set_playback_rate(uint32_t rate);
void bt_audio_set_capture_rate(uint32_t rate);
bool bt_audio_is_playback_active(void);
bool bt_audio_is_capture_active(void);

#endif /* __SOF_AUDIO_BT_AUDIO_H__ */
