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
 * \file include/ipc4/aria.h
 * \brief aria definitions.
 * NOTE: This ABI uses bit fields and is non portable.
 */

#ifndef __SOF_IPC4_ARIA_H__
#define __SOF_IPC4_ARIA_H__

#include <sof/compiler_attributes.h>
#include "base-config.h"

struct ipc4_aria_module_cfg {
	struct ipc4_base_module_cfg base_cfg;
	uint32_t attenuation;
} __packed __aligned(8);
#endif
