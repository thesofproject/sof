/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef MODULE_LLEXT_H
#define MODULE_LLEXT_H

#define SOF_LLEXT_MODULE_MANIFEST(manifest_name, entry, affinity, mod_uuid, instances, ...) \
{ \
	.module = { \
		.name = manifest_name, \
		.uuid = mod_uuid, \
		.entry_point = (uint32_t)(entry), \
		.instance_max_count = instances, \
		.type = { \
			.load_type = SOF_MAN_MOD_TYPE_LLEXT, \
			.domain_ll = 1, \
			.user_mode = COND_CODE_0(NUM_VA_ARGS_LESS_1(_, ##__VA_ARGS__), (0), \
						 (GET_ARG_N(1, __VA_ARGS__))), \
		}, \
		.affinity_mask = (affinity), \
	} \
}

#define SOF_LLEXT_AUX_MANIFEST(manifest_name, entry, mod_uuid) \
{ \
	.module = { \
		.name = manifest_name, \
		.uuid = mod_uuid, \
		.entry_point = (uint32_t)(entry), \
		.type = { \
			.load_type = SOF_MAN_MOD_TYPE_LLEXT_AUX, \
		}, \
	} \
}

#define SOF_LLEXT_BUILDINFO \
static const struct sof_module_api_build_info buildinfo __section(".mod_buildinfo") __used = { \
	.format = SOF_MODULE_API_BUILD_INFO_FORMAT, \
	.api_version_number.full = SOF_MODULE_API_CURRENT_VERSION, \
}

#endif
