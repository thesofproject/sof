/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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
 * \file volume/peak_volume.h
 * \brief peak volume definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_PEAK_VOL_H__
#define __SOF_IPC4_PEAK_VOL_H__

#include <ipc/topology.h>
#include <ipc4/base-config.h>

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
	IPC4_SET_ATTENUATION,
	IPC4_VOLUME_TRANSITION_DELAY
};

enum ipc4_curve_type {
	IPC4_AUDIO_CURVE_TYPE_NONE = 0,
	IPC4_AUDIO_CURVE_TYPE_WINDOWS_FADE,
	IPC4_AUDIO_CURVE_TYPE_LINEAR,
	IPC4_AUDIO_CURVE_TYPE_LOG,
	IPC4_AUDIO_CURVE_TYPE_LINEAR_ZC,
	IPC4_AUDIO_CURVE_TYPE_LOG_ZC,
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
	 * Fade curve type - uses enum ipc4_curve_type
	 */
	uint32_t curve_type;
	uint32_t reserved;
	/**
	 * Curve duration in number of hundreds nanoseconds for format specified during
	 * initialization.
	 */
	uint64_t curve_duration;
} __packed __aligned(8);

struct ipc4_peak_volume_module_cfg {
	struct ipc4_base_module_cfg base_cfg;
	struct ipc4_peak_volume_config config[];
} __packed __aligned(8);

static inline enum sof_volume_ramp ipc4_curve_type_convert(enum ipc4_curve_type ipc4_type)
{
	switch (ipc4_type) {
	case IPC4_AUDIO_CURVE_TYPE_WINDOWS_FADE:
		return SOF_VOLUME_WINDOWS_FADE;

	case IPC4_AUDIO_CURVE_TYPE_LINEAR:
		return SOF_VOLUME_LINEAR;

	case IPC4_AUDIO_CURVE_TYPE_LOG:
		return SOF_VOLUME_LOG;

	case IPC4_AUDIO_CURVE_TYPE_LINEAR_ZC:
		return SOF_VOLUME_LINEAR_ZC;

	case IPC4_AUDIO_CURVE_TYPE_LOG_ZC:
		return SOF_VOLUME_LOG_ZC;

	case IPC4_AUDIO_CURVE_TYPE_NONE:
	default:
		return SOF_VOLUME_WINDOWS_NO_FADE;
	}
}
#endif
