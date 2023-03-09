/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */

#ifndef __MODULE_API_VER_H__
#define __MODULE_API_VER_H__

/*
 *  Api version 5.0.0 for sof loadable modules
 */

#define _MAJOR_API_MODULE_VERSION 5
#define _MIDDLE_API_MODULE_VERSION 0
#define _MINOR_API_MODULE_VERSION 0

typedef union sof_module_api_version {
	uint32_t full;
	struct {
		uint32_t minor    : 10;
		uint32_t middle   : 10;
		uint32_t major    : 10;
		uint32_t reserved : 2;
	} fields;
} sof_module_api_version;

typedef const struct {
	uint32_t format;
	sof_module_api_version api_version_number;
} sof_module_build_info;

#endif /* __MODULE_API_VER_H__ */
