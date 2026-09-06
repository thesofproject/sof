/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Espressif Systems / SOF Project
 */

#ifndef __SOF_AUDIO_USB_AUDIO_H__
#define __SOF_AUDIO_USB_AUDIO_H__

#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/lib/uuid.h>
#include <zephyr/kernel.h>

/* UUID for USB Audio Component */
/* b95039f6-3d23-41a4-9276-2fbb27c62d01 */
#define SOF_USB_AUDIO_UUID \
	DECLARE_SOF_RT_UUID("usb_audio", 0xb95039f6, 0x3d23, 0x41a4, \
			    0x92, 0x76, 0x2f, 0xbb, 0x27, 0xc6, 0x2d, 0x01)

#define USB_AUDIO_RING_BUFFER_SIZE 16384

struct usb_audio_ring_buffer {
	uint8_t buf[USB_AUDIO_RING_BUFFER_SIZE];
	volatile uint32_t head;
	volatile uint32_t tail;
	volatile uint32_t count;
	struct k_spinlock lock;
};

struct usb_audio_data {
	struct usb_audio_ring_buffer ring;
	uint32_t sample_rate;
	uint32_t channels;
	uint32_t frame_bytes;
	uint32_t period_bytes;
	enum sof_ipc_stream_direction direction;
	bool active;
};

void usb_audio_feed_playback_data(const void *src, size_t bytes);
size_t usb_audio_fetch_capture_data(void *dst, size_t bytes);
void usb_audio_set_playback_rate(uint32_t rate);
void usb_audio_set_capture_rate(uint32_t rate);

#endif /* __SOF_AUDIO_USB_AUDIO_H__ */
