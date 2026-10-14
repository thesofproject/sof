/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

/**
 * \file include/ipc4/kpb.h
 * \brief Key Phrase Buffer (KPB) audio pre-roll module configuration and parameters.
 * \ingroup ipc4_telemetry_probe_api
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_KPB_H__
#define __SOF_IPC4_KPB_H__

#include "base-config.h"

/**
 * \brief KPB module instance base configuration structure.
 * \ingroup ipc4_telemetry_probe_api
 */
struct ipc4_kpb_module_cfg {
	struct ipc4_base_module_cfg base_cfg; /**< Standard base module configuration. */
} __packed __aligned(4);

/**
 * \brief Parameter IDs for KPB module LargeConfigSet/Get.
 * \ingroup ipc4_telemetry_probe_api
 */
enum ipc4_kpb_module_config_params {
	/** Configure module IDs that execute within Fast Mode (FM) draining tasks. */
	KP_BUF_CFG_FM_MODULE = 1,
	/** Set history ring-buffer duration in milliseconds; 0 = use CONFIG_KPB_MAX_BUFF_TIME. */
	KP_BUF_CFG_BUFF_TIME_MS = 2,
	/** Real-time sink microphone channel selector ID. */
	KP_BUF_CLIENT_MIC_SELECT = 11,
};

#endif

