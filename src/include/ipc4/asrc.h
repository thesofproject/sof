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
 */

/**
 * \file include/ipc4/asrc.h
 * \brief IPC4 asrc definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_ASRC_H__
#define __SOF_IPC4_ASRC_H__

#include <stdint.h>
#include <ipc4/base-config.h>

/*
 * ASRC_CONFIG Asynchronous Sample Rate Converter module configuration.
 *
 * Module requirements:
 * - sample size (bit_depth): 32 bit
 * - number of channels: 1, 2, 3, 4, 5, 6, 7, 8
 *
 * Following sample conversion ratios are supported (input_frequency/output_frequency):
 * 48k/8k, 48k/16k, 48k/24k, 48k/32k, 48k/44.1k, 48k/48k, 44.1k/44.1k, 44.1k/48k,
 * 32k/32k, 32k/48k, 24k/8k, 24k/16k, 24k/24k, 24k/48k, 22.05k/48k, 18.9k/48k,
 * 16k/8k, 16k/16k, 16k/24k, 16k/48k, 8k/8k, 8k/24k, 8k/48k
 *
 * ASRC requires special IBS/OBS handling:
 *
 * In LL mode:
 *
 * 1. In Playback path (PUSH mode):
 * - IBS calculated based on input frequency and sample group size
 * - OBS calculated based on output frequency and sample group size, extended by X sample groups
 *
 * In Capture path (PULL mode):
 * - IBS calculated based on input frequency and sample group size, extended by X sample groups
 * - OBS calculated based on output frequency and sample group size
 * X=3 is maximum number of additional samples that can be produced by ASRC in one cycle
 * (related to drift and frequencies not divisible by 1000)
 *
 * For LL mode, jitter buffers in ASRC feature mask should be enabled (default enabled)
 *
 * Examples:
 *
 * ASRC in PULL mode, conversion 48k -> 44.1k, 32bit, 2ch
 * - IBS = round_up(freq_in)/1000 *
 *		channels_num *
 *		sample_size_in_bytes + (3 * sample_group_size) =
 *		= 48 * 2 * 4 + 3 * 2 * 4 = 408
 * - OBS = round_up(freq_out)/1000 *
 *		channels_num *
 *		sample_size_in_bytes = 45 * 2 * 4 = 360
 *
 * In DP mode:
 *
 * In Playback path (PUSH mode):
 * - IBS calculated based on input frequency and DP frame size
 * - OBS = round_up((IBS/sample_group_size) *
 *		(freq_out/freq_in)) *
 *		sample_group_size + (X * sample_group_size)
 *
 * In Capture path (PULL mode):
 * - OBS calculated based on output frequency and DP frame size
 * - IBS = round_up((OBS/sample_group_size) *
 *		(freq_in/freq_out)) *
 *		sample_group_size + (X * sample_group_size)
 * X=1 is a maximum drift that can be measured for one cycle
 *
 * For DP mode, jitter buffers in ASRC feature mask should be disabled (default enabled)
 *
 * ASRC in PUSH mode, conversion 22.05k -> 48k, 32bit, 2ch, DP frame size: 5ms
 * - IBS = round_up(freq_in)/1000 *
 *		channels_num *
 *		sample_size_in_bytes *
 *		dp_frame_size = 23 * 2 * 4 * 5 = 920
 * - OBS = round_up((IBS/sample_group_size) *
 *		(freq_out/freq_in)) *
 *		sample_group_size + 1 * sample_group_size =
 *		round_up( 115 * 48000 / 22050 ) * 8 + 1 * 8 = 251 * 8 + 8 = 2016
 */

/* This enum defines short 16bit parameters common for all modules.
 * Value of module specific parameters have to be less than 0x3000.
 */
enum ipc4_asrc_module_params {
	IPC4_MOD_ASRC_SAMPLE_DRIFT = 0x3001,
	IPC4_MOD_ASRC_SAMPLE_DRIFT_TOTAL = 0x3002
};

enum ipc4_asrc_features {
	/* Playback and capture path for Asrc has different requirmetns for ibs and obs
	 * asrc flag defined to differentiate between these two paths.
	 *
	 * Use for playback/uplink
	 */
	IPC4_MOD_ASRC_PUSH_MODE = 0,
	/* Use for capture/downlink
	 */
	IPC4_MOD_ASRC_PULL_MODE = 1,
	/* Jitter buffer in ASRC implementation is optional and can be disabled
	 * when using DP mode. For keeping backwards compatibility, by default
	 * Jitter buffer is enabled
	 */
	IPC4_MOD_ASRC_DISABLE_JITTER_BUFFER = 4
};

struct ipc4_asrc_module_cfg {
	struct ipc4_base_module_cfg base;
	/* ASRC output sampling frequency. */
	enum ipc4_sampling_frequency out_freq;

	/* Mask of allowed ASRC features:
	 * - BITS 0-1 (ASRC mode):
	 *	a. 0 1 - Playback mode / PUSH mode
	 *	b. 1 0 - Capture mode / PULL mode
	 *
	 * - BIT 4 (Disable Jitter Buffer):
	 *  a. 0 - Jitter buffer enabled
	 *  b. 1 - Jitter buffer disabled
	 */
	uint32_t asrc_mode;
} __packed __aligned(4);

#endif
