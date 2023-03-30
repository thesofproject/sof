// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2023 Intel Corporation. All rights reserved.

#include <rimage/sof/user/manifest.h>

/* rimage fails if no .bss section found */
int bss_workaround;

#define ADSP_BUILD_INFO_FORMAT 0

/* these supposed to be defined in some API header file which is not yet available */
union adsp_api_version {
	uint32_t full;
	struct {
		uint32_t minor    : 10;
		uint32_t middle   : 10;
		uint32_t major    : 10;
		uint32_t reserved : 2;
	} fields;
};

struct adsp_build_info {
	uint32_t format;
	union adsp_api_version api_version_number;
};

struct adsp_build_info dummy_build_info __attribute__((section(".buildinfo"))) = {
	ADSP_BUILD_INFO_FORMAT,
	{
		((0x3FF & MAJOR_IADSP_API_VERSION)  << 20) |
		((0x3FF & MIDDLE_IADSP_API_VERSION) << 10) |
		((0x3FF & MINOR_IADSP_API_VERSION)  << 0)
	}
};

__attribute__((section(".cmi.text")))
void *dummyPackageEntryPoint(int a, int b)
{
	/* supposed to return here a pointer to module interface implementation */
	return (void *)0x12345678;
}

__attribute__((section(".module")))
const struct sof_man_module_manifest dummy_module_manifest = {
	.module = {
		.name = "DUMMY",
		.uuid = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
		.entry_point = (uint32_t)dummyPackageEntryPoint,
		.type = {
			.load_type = SOF_MAN_MOD_TYPE_MODULE,
			.domain_ll = 1
		},
		.affinity_mask = 3,
	}
};
