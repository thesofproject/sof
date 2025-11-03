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

int native_system_agent_start(const struct system_agent_params *params,
			      const void **iface)
{
	native_sys_agent.module_id = params->module_id;
	native_sys_agent.instance_id = params->instance_id;
	native_sys_agent.core_id = params->core_id;
	native_sys_agent.log_handle = params->log_handle;
	const void *ret;

	void *system_agent_p = &native_sys_agent;

	native_create_instance_f ci = (native_create_instance_f)params->entry_point;

	ret = ci(params->mod_cfg, NULL, &system_agent_p);
	if (!ret)
		return -EINVAL;

	*iface = ret;
	return 0;
}
