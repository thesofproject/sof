/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Sound Open Firmware (SOF) Project. All rights reserved.
 */

#ifndef __SOF_AUDIO_USB_AUDIO_H__
#define __SOF_AUDIO_USB_AUDIO_H__

#include <sof/audio/component.h>
#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 16 KB (85 ms at 48 kHz stereo S16_LE) lock-protected ring buffer */
#define USB_AUDIO_RING_BUFFER_SIZE 16384
/* Prebuffer 10 ms (1920 bytes) at stream start to absorb USB/PLL jitter */
#define USB_AUDIO_PREBUFFER_BYTES  1920

struct usb_audio_ring_buffer {
	uint8_t buf[USB_AUDIO_RING_BUFFER_SIZE];
	uint32_t head;
	uint32_t tail;
	uint32_t count;
	struct k_spinlock lock;
};

struct usb_audio_data {
	struct usb_audio_ring_buffer ring;
	bool active;
	bool started;
	uint32_t sample_rate;
	uint32_t channels;
	uint32_t frame_bytes;
	uint32_t period_bytes;
};

void usb_audio_feed_playback_data(const void *src, size_t bytes);
size_t usb_audio_fetch_capture_data(void *dst, size_t bytes);
bool usb_audio_has_capture_data(size_t min_bytes);
bool usb_audio_peek_capture_data(void *dst, size_t bytes);
void usb_audio_consume_capture_data(size_t bytes);
void usb_audio_set_playback_rate(uint32_t rate);
void usb_audio_set_capture_rate(uint32_t rate);

#endif /* __SOF_AUDIO_USB_AUDIO_H__ */
