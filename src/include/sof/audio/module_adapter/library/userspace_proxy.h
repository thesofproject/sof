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

#include <native_system_agent.h>
#include <module/module/interface.h>
#include <sof/audio/module_adapter/library/userspace_proxy_user.h>

struct module_interface;
struct comp_driver;
struct sof_man_module;
struct system_agent_params;

/* Processing module structure fields needed for user mode */
struct userspace_context {
	struct k_mem_domain *comp_dom;			/* Module specific memory domain	*/
	const struct module_interface *interface;	/* Userspace module interface		*/
	struct user_work_item *work_item;		/* work item for user worker thread	*/
	struct k_event *dp_event;			/* DP thread event			*/
};
#endif /* CONFIG_USERSPACE */

#if CONFIG_SOF_USERSPACE_PROXY
/**
 * Creates userspace module proxy
 *
 * @param user_ctx - pointer to pointer of userspace module context
 * @param drv - pointer to component driver
 * @param manifest - pointer to module manifest
 * @param start_fn - pointer to system agent start function
 * @param agent_params - pointer to system_agent_params
 * @param agent_interface - pointer to variable to store module interface created by agent
 * @param ops - Pointer to a variable that will hold the address of the module interface
 *		structure. The function stores a pointer to its own userspace proxy
 *		interface structure in this variable.
 *
 * @return 0 for success, error otherwise.
 */
int userspace_proxy_create(struct userspace_context **user_ctx, const struct comp_driver *drv,
			   const struct sof_man_module *manifest, system_agent_start_fn start_fn,
			   const struct system_agent_params *agent_params,
			   const void **agent_interface, const struct module_interface **ops);

/**
 * Destroy userspace module proxy
 *
 * @param drv - pointer to component driver
 * @param user_ctx - pointer to userspace module context
 */
void userspace_proxy_destroy(const struct comp_driver *drv, struct userspace_context *user_ctx);

#if IS_ENABLED(CONFIG_SOF_USERSPACE_MOD_IPC_BY_DP_THREAD)
/**
 * Register a k_event object used to notify the DP thread about a pending userspace module IPC
 * request to process.
 *
 * @param mod    Pointer to the processing module.
 * @param event  Pointer to the event to signal incoming IPC.
 *
 * @return Pointer to a k_work_user work item for userspace modules, or NULL for non-userspace
 *	   modules.
 */
struct k_work_user *userspace_proxy_register_ipc_handler(struct processing_module *mod,
							 struct k_event *event);
#endif

#endif /* CONFIG_SOF_USERSPACE_PROXY */

/*
 * Per-core userspace IPC worker lifecycle.
 *
 * A userspace IPC worker is a user-mode work queue (and its worker thread) that
 * services IPC requests targeting userspace modules running on a given core. The
 * worker is created on, and managed from, the primary core. Configurations that do
 * not use a dedicated per-core worker resolve these calls to no-ops.
 */
#if CONFIG_SOF_USERSPACE_PROXY && !CONFIG_SOF_USERSPACE_MOD_IPC_BY_DP_THREAD
/**
 * Create the userspace IPC worker for the given core.
 *
 * Must be called while running on the primary core. The worker thread is pinned to
 * the target core. Calling it again for a core that already has a worker is a no-op.
 *
 * @param cpu - target core id the worker is created for
 * @return 0 on success, negative error code otherwise.
 */
int user_worker_create(int cpu);

/**
 * Free the userspace IPC worker for the given core.
 *
 * Must be called from the primary core while the target core is still running, as it
 * aborts the worker thread.
 *
 * @param cpu - target core id whose worker is freed
 */
void user_worker_free(int cpu);
#else
static inline int user_worker_create(int cpu) { return 0; }
static inline void user_worker_free(int cpu) { }
#endif

#endif /* __SOF_AUDIO_USERSPACE_PROXY_H__ */
