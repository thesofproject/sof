/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Pawel Dobrowolski <pawelx.dobrowolski@intel.com>
 */

#ifndef __NATIVE_SYSTEM_AGENT_H__
#define __NATIVE_SYSTEM_AGENT_H__

#include <sof/audio/module_adapter/module/module_interface.h>
#include <native_system_service.h>

struct native_system_agent {
	struct system_service system_service;
	uint32_t log_handle;
	uint32_t core_id;
	uint32_t module_id;
	uint32_t instance_id;
	uint32_t module_size;
};

/**
 * @brief Starts the native system agent.
 *
 * This function initializes and starts the native system agent with the provided parameters.
 *
 * @param[in] entry_point - The module entry point function address.
 * @param[in] module_id - The identifier for the module.
 * @param[in] instance_id - The instance identifier of the module.
 * @param[in] core_id - Core on which the module will run.
 * @param[in] log_handle - The handle for logging purposes.
 * @param[in] mod_cfg - Pointer to the module configuration data.
 * @param[out] iface - Pointer to the module interface.
 *
 * @return Returns 0 on success or an error code on failure.
 */
int native_system_agent_start(uint32_t entry_point, uint32_t module_id, uint32_t instance_id,
			      uint32_t core_id, uint32_t log_handle, void *mod_cfg,
			      const void **iface);

#endif /* __NATIVE_SYSTEM_AGENT_H__ */
