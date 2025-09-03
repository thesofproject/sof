/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 *
 * Author: Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __SOF_AUDIO_USERSPACE_PROXY_H__
#define __SOF_AUDIO_USERSPACE_PROXY_H__

#if CONFIG_USERSPACE
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/app_memory/app_memdomain.h>
#include <utilities/array.h>

#define APP_USER_DATA	K_APP_DMEM(ipc_partition)
#define APP_USER_BSS	K_APP_BMEM(ipc_partition)

#include <native_system_agent.h>
#include <module/module/interface.h>

struct module_interface;
struct comp_driver;
struct sof_man_module;

/* Processing module structure fields needed for user mode */
struct userspace_context {
	struct k_mem_domain *comp_dom;			/* Module specific memory domain	*/
	uint32_t security_domain_id;			/* Determine security domain id number	*/
	void *wrk_ctx;					/* Work queue context			*/
	const struct module_interface *interface;	/* Userspace module interface		*/
};
/**
* Creates userspace module proxy
*
* @param user_ctx - pointer to pointer of userspace module context
* @param drv - pointer to component driver
* @param manifest - pointer to module manifest
* @param agent - pointer to system agent start function
* @param entry_point - loadable module entry point
* @param module_id - module id
* @param instance_id - instance id
* @param core_id - core id - unused
* @param log_handle - log handle
* @param mod_cfg - ipc4 module configuration
* @param interface - pointer to variable to store created module interface
* @return 0 for success, error otherwise.
*/
int userspace_proxy_create(struct userspace_context **user_ctx, const struct comp_driver *drv,
			   const struct sof_man_module *manifest, system_agent_start_fn start_fn,
			   uintptr_t entry_point, uint32_t module_id, uint32_t instance_id,
			   uint32_t core_id, uint32_t log_handle, byte_array_t *mod_cfg,
			   const void **adapter);

/**
* Creates userspace module proxy
* 
* @param drv - pointer to component driver
* @param user_ctx - pointer to userspace module context
*/
void userspace_proxy_destroy(const struct comp_driver *drv, struct userspace_context *user_ctx);

#else

#define APP_USER_DATA

#endif /* CONFIG_USERSPACE */
#endif /* __SOF_AUDIO_USERSPACE_PROXY_H__ */
