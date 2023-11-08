/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __MODULE_ADAPTER_IADK_API_VERSION_H__
#define __MODULE_ADAPTER_IADK_API_VERSION_H__

#include <module/module/api_ver.h>

#define IADK_MODULE_API_BUILD_INFO_FORMAT	0

#define IADK_MODULE_API_MAJOR_VERSION		4
#define IADK_MODULE_API_MIDDLE_VERSION		5
#define IADK_MODULE_API_MINOR_VERSION		0

#define IADK_MODULE_API_CURRENT_VERSION	MODULE_API_VERSION_ENCODE(IADK_MODULE_API_MAJOR_VERSION, \
	IADK_MODULE_API_MIDDLE_VERSION, IADK_MODULE_API_MINOR_VERSION)

#endif /* __MODULE_ADAPTER_IADK_API_VERSION_H__ */
