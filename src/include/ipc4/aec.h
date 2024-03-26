/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
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

/**
 * \file include/ipc4/aec.h
 * \brief IPC4 global aec definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_AEC_CONFIG_H__
#define __SOF_IPC4_AEC_CONFIG_H__

#include <stdint.h>
#include <ipc4/base-config.h>

struct sof_ipc4_aec_config {
	struct ipc4_base_module_cfg base_cfg;
	struct ipc4_audio_format reference_fmt;
	struct ipc4_audio_format output_fmt;
	uint32_t cpc_low_power_mode;
};

enum sof_ipc4_aec_config_params {
	IPC4_AEC_SET_EXT_FMT,
};

#endif
