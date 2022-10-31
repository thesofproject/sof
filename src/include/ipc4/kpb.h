/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

/**
 * \file include/ipc4/kpb.h
 * \brief KPB module definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_KPB_H__
#define __SOF_IPC4_KPB_H__

#include "base-config.h"

struct ipc4_kpb_module_cfg {
	struct ipc4_base_module_cfg base_cfg;
} __packed __aligned(4);
#endif
