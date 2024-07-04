/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef MODULE_LLEXT_H
#define MODULE_LLEXT_H

#define SOF_LLEXT_MODULE_MANIFEST(manifest_name, entry, affinity, mod_uuid, instances) \
{ \
	.module = { \
		.name = manifest_name, \
		.uuid = {mod_uuid}, \
		.entry_point = (uint32_t)(entry), \
		.instance_max_count = instances, \
		.type = { \
			.load_type = SOF_MAN_MOD_TYPE_LLEXT, \
			.domain_ll = 1, \
		}, \
		.affinity_mask = (affinity), \
	} \
}

#define SOF_LLEXT_AUX_MANIFEST(manifest_name, entry, mod_uuid) \
{ \
	.module = { \
		.name = manifest_name, \
		.uuid = {mod_uuid}, \
		.entry_point = (uint32_t)(entry), \
		.type = { \
			.load_type = SOF_MAN_MOD_TYPE_LLEXT_AUX, \
		}, \
	} \
}

#define SOF_LLEXT_MOD_ENTRY(name, interface) \
static const struct module_interface *name##_llext_entry(void *mod_cfg, \
					void *parent_ppl, void **mod_ptr) \
{ \
	return interface; \
}

#define SOF_LLEXT_BUILDINFO \
static const struct sof_module_api_build_info buildinfo __section(".mod_buildinfo") __used = { \
	.format = SOF_MODULE_API_BUILD_INFO_FORMAT, \
	.api_version_number.full = SOF_MODULE_API_CURRENT_VERSION, \
}

#endif
