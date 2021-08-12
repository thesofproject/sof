/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 */

/*
 * This file contains structures that are exact copies of an existing ABI used
 * by IOT middleware. They are Intel specific and will be used by one middleware.
 *
 * Some of the structures may contain programming implementations that makes them
 * unsuitable for generic use and general usage.
 *
 * This code is mostly copied "as-is" from existing C++ interface files hence the use of
 * different style in places. The intention is to keep the interface as close as possible to
 * original so it's easier to track changes with IPC host code.
 */

/*
 * \file include/ipc4/peak_volume.h
 * \brief peak volume definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_PEAK_VOL_H__
#define __SOF_IPC4_PEAK_VOL_H__

#include "base-config.h"

enum ipc4_vol_mode {
	IPC4_MODE_PEAKVOL		= 1,
	IPC4_MODE_GAIN			= 2,
};

enum ipc4_peak_volume_param {
	/*
	 * Use LARGE_CONFIG_SET to change volume / apply curve.
	 * Ipc mailbox must contain properly built PeakVolumeConfig.
	 */
	IPC4_VOLUME = 0,
	IPC4_SET_ATTENUATION = 1,
	IPC4_VOLUME_TRANSITION_DELAY = 2
};

enum ipc4_curve_type {
	IPC4_AUDIO_CURVE_TYPE_NONE = 0,
	IPC4_AUDIO_CURVE_TYPE_WINDOWS_FADE = 1
};

static const uint32_t IPC4_ALL_CHANNELS_MASK = 0xffffffff;

struct ipc4_peak_volume_config {
	/*
	 * ID of channel. If set to ALL_CHANNELS_MASK then
	 * configuration is identical and will be set for all all channels
	 */
	uint32_t channel_id;
	/*
	 * Target channel volume. Takes values from 0 to 0x7fffffff.
	 */
	uint32_t target_volume;
	/*
	 * Fade curve type
	 */
	uint32_t curve_type;
	uint32_t reserved;
	/**
	 * Curve duration in number of hundreds nanoseconds for format specified during
	 * initialization.
	 */
	uint64_t curve_duration;
} __attribute__((packed, aligned(8)));

struct ipc4_peak_volume_module_cfg {
	struct ipc4_base_module_cfg base_cfg;
	struct ipc4_peak_volume_config config[0];
} __attribute__((packed, aligned(8)));
#endif
