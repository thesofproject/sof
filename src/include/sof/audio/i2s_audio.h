/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#ifndef __SOF_AUDIO_I2S_AUDIO_H__
#define __SOF_AUDIO_I2S_AUDIO_H__

#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/lib/uuid.h>
#include <zephyr/kernel.h>

/* UUID for I2S Hardware Audio Component */
/* 3d6b7c52-19a0-4e61-9822-8a3f4710d56e */
#define SOF_I2S_AUDIO_UUID \
	DECLARE_SOF_RT_UUID("i2s_audio", 0x3d6b7c52, 0x19a0, 0x4e61, \
			    0x98, 0x22, 0x8a, 0x3f, 0x47, 0x10, 0xd5, 0x6e)

struct i2s_audio_stats {
	uint32_t tx_frames;
	uint32_t rx_frames;
	uint32_t tx_errors;
	uint32_t rx_errors;
};

void i2s_audio_get_stats(struct i2s_audio_stats *stats);

#endif /* __SOF_AUDIO_I2S_AUDIO_H__ */
