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

#define SOF_MODULE_API_MAJOR_VERSION 5
#define SOF_MODULE_API_MIDDLE_VERSION 0
#define SOF_MODULE_API_MINOR_VERSION 0

union sof_module_api_version {
	uint32_t full;
	struct {
		uint32_t minor    : 10;
		uint32_t middle   : 10;
		uint32_t major    : 10;
		uint32_t reserved : 2;
	} fields;
};

struct sof_module_api_build_info{
	uint32_t format;
	union sof_module_api_version api_version_number;
};

#endif /* __MODULE_API_VER_H__ */
