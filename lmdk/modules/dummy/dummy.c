// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright 2023 Intel Corporation. All rights reserved.
 */

#include <rimage/sof/user/manifest.h>
#include <module/api_ver.h>

DECLARE_LOADABLE_MODULE_API_VERSION(dummy);

void *dummyPackageEntryPoint(void *mod_cfg, void *parent_ppl, void **mod_ptr)
{
	/* supposed to return here a pointer to module interface implementation */
	return (void *)0x12345678;
}

__attribute__((section(".module")))
const struct sof_man_module_manifest dummy_module_manifest = {
	.module = {
		.name = "DUMMY",
		.uuid = { 0x01234567, 0x89AB, 0xCDEF, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			  0x08 }},
		.entry_point = (uint32_t)dummyPackageEntryPoint,
		.type = {
			.load_type = SOF_MAN_MOD_TYPE_MODULE,
			.domain_ll = 1
		},
		.affinity_mask = 3,
	}
};
