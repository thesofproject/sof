/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_LLEXT_MANAGER_H__
#define __SOF_LLEXT_MANAGER_H__

#include <stdbool.h>
#include <stdint.h>

#if CONFIG_LLEXT
#include <rimage/sof/user/manifest.h>

struct comp_dev;
struct comp_driver;
struct comp_ipc_config;

static inline bool module_is_llext(const struct sof_man_module *mod)
{
	return mod->type.load_type == SOF_MAN_MOD_TYPE_LLEXT;
}

uintptr_t llext_manager_allocate_module(struct processing_module *proc,
					const struct comp_ipc_config *ipc_config,
					const void *ipc_specific_config);

int llext_manager_free_module(const uint32_t component_id);

bool comp_is_llext(struct comp_dev *comp);
#else
#define module_is_llext(mod) false
#define llext_manager_allocate_module(proc, ipc_config, ipc_specific_config) 0
#define llext_manager_free_module(component_id) 0
#define llext_unload(ext) 0
#define comp_is_llext(comp) false
#endif

#endif
