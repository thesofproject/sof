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

struct system_agent_params {
	uintptr_t entry_point;	/* The module entry point function address. */
	uint32_t module_id;	/* The identifier for the module. */
	uint32_t instance_id;	/* The instance identifier of the module. */
	uint32_t core_id;	/* Core on which the module will run. */
	uint32_t log_handle;	/* The handle for logging purposes. */
	void *mod_cfg;		/* Pointer to the module configuration data. */
};

typedef int (*system_agent_start_fn)(const struct system_agent_params *params,
				     const void **adapter);

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
 * @param[in] params - Pointer to the system agent parameter structure
 * @param[out] iface - Pointer to the module interface.
 *
 * @return Returns 0 on success or an error code on failure.
 */
int native_system_agent_start(const struct system_agent_params *params,
			      const void **iface);

#endif /* __NATIVE_SYSTEM_AGENT_H__ */
