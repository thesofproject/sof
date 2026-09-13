// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#ifndef __SOF_PLATFORM_TEENSY41_AUDIO_TIMER_H__
#define __SOF_PLATFORM_TEENSY41_AUDIO_TIMER_H__

#include <stdint.h>

typedef void (*teensy41_audio_timer_cb_t)(void *arg);

int teensy41_audio_timer_init(teensy41_audio_timer_cb_t cb, void *arg);
void teensy41_audio_timer_start(void);
void teensy41_audio_timer_stop(void);

#endif /* __SOF_PLATFORM_TEENSY41_AUDIO_TIMER_H__ */
