// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */

#include <stdbool.h>
#include <stdint.h>
#include <utilities/array.h>
#include <native_system_agent.h>

/* The create_instance_f is a function call type known in module. The module entry_point
 * points to this type of function which starts module creation.
 */

typedef void* (*native_create_instance_f)(void *mod_cfg, void *parent_ppl,
					  void **mod_ptr);

struct native_system_agent native_sys_agent;

int native_system_agent_start(uintptr_t entry_point, uint32_t module_id, uint32_t instance_id,
			      uint32_t core_id, uint32_t log_handle, void *mod_cfg,
			      const void **iface)
{
	native_sys_agent.module_id = module_id;
	native_sys_agent.instance_id = instance_id;
	native_sys_agent.core_id = core_id;
	native_sys_agent.log_handle = log_handle;
	const void *ret;

	void *system_agent_p = &native_sys_agent;

	native_create_instance_f ci = (native_create_instance_f)entry_point;

	ret = ci(mod_cfg, NULL, &system_agent_p);
	if (!ret)
		return -EINVAL;

	*iface = ret;
	return 0;
}
