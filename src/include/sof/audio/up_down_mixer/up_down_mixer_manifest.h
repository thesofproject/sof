/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */

#ifndef __UP_DOWN_MIXER_MANIFEST_H__
#define __UP_DOWN_MIXER_MANIFEST_H__

#include <rimage/sof/user/manifest.h>
#include <sof/audio/module_adapter/library/module_api_ver.h>

#define ADSP_BUILD_INFO_FORMAT 0

struct module_interface up_down_mixer_interface;

void *loadable_udm_entry_point(void *mod_cfg, void *parent_ppl, void **mod_ptr) {
	return &up_down_mixer_interface;
}

struct sof_module_api_build_info udm_build_info __attribute__((section(".buildinfo"))) = {
	ADSP_BUILD_INFO_FORMAT,
	{
		((0x3FF & SOF_MODULE_API_MAJOR_VERSION)  << 20) |
		((0x3FF & SOF_MODULE_API_MIDDLE_VERSION) << 10) |
		((0x3FF & SOF_MODULE_API_MINOR_VERSION)  << 0)
	}
};

extern struct module_interface up_down_mixer_interface;

__attribute__((section(".module")))
const struct sof_man_module_manifest udm_manifest = {
	.module = {
		.name = "UPDWMIX",
		.uuid = {0x0C, 0x06, 0xF8, 0x42, 0x2F, 0x83, 0xBF, 0x4D,
			 0xB2, 0x47, 0x51, 0xE9, 0x61, 0x99, 0x7B, 0x34},
		.entry_point = (uint32_t)loadable_udm_entry_point,
		.type = { .load_type = SOF_MAN_MOD_TYPE_MODULE,
		.domain_ll = 1 },
		.affinity_mask = 1,
}
};

#endif /* __UP_DOWN_MIXER_MANIFEST_H__ */
