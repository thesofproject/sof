/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */

#ifndef __MODULE_MODULE_API_VER_H__
#define __MODULE_MODULE_API_VER_H__

#include <stdint.h>

#define MODULE_API_VERSION_ENCODE(a, b, c) \
	(((0x3FF & (a)) << 20) | ((0x3FF & (b)) << 10) | (0x3FF & (c)))

#define SOF_MODULE_API_BUILD_INFO_FORMAT	0x80000000

/*
 *  Api version 5.0.0 for sof loadable modules
 */

#define SOF_MODULE_API_MAJOR_VERSION	5
#define SOF_MODULE_API_MIDDLE_VERSION	0
#define SOF_MODULE_API_MINOR_VERSION	1

#define SOF_MODULE_API_CURRENT_VERSION	MODULE_API_VERSION_ENCODE(SOF_MODULE_API_MAJOR_VERSION,	\
	SOF_MODULE_API_MIDDLE_VERSION, SOF_MODULE_API_MINOR_VERSION)

union sof_module_api_version {
	uint32_t full;
	struct {
		uint32_t minor    : 10;
		uint32_t middle   : 10;
		uint32_t major    : 10;
		uint32_t reserved : 2;
	} fields;
};

struct sof_module_api_build_info {
	uint32_t format;
	union sof_module_api_version api_version_number;
};

#define DECLARE_LOADABLE_MODULE_API_VERSION(name)						\
struct sof_module_api_build_info name ## _build_info __attribute__((section(".buildinfo"))) = {	\
	SOF_MODULE_API_BUILD_INFO_FORMAT, SOF_MODULE_API_CURRENT_VERSION			\
}

#endif /* __MODULE_MODULE_API_VER_H__ */
