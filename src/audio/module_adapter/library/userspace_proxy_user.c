// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Adrian Warecki <adrian.warecki@intel.com>

/**
 * \file audio/module_adapter/library/userspace_proxy_user.c
 * \brief Userspace proxy functions executed only in userspace context.
 * \authors Adrian Warecki
 */

/* Assume that all the code runs in user mode and unconditionally makes system calls. */
#define __ZEPHYR_USER__

#include <sof/common.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <sof/audio/component.h>
#include <sof/schedule/dp_schedule.h>
#include <sof/audio/module_adapter/module/generic.h>

#include <sof/audio/module_adapter/library/userspace_proxy.h>
#include <sof/audio/module_adapter/library/userspace_proxy_user.h>

void userspace_proxy_handle_request(struct processing_module *mod, struct module_params *params)
{
	const struct module_interface *ops = params->context->interface;

	switch (params->cmd) {
	case USER_PROXY_MOD_CMD_AGENT_START:
		/* Set pointer to user accessible mod_cfg structure. */
		params->ext.agent.params.mod_cfg = &params->ext.agent.mod_cfg;

		params->status = params->ext.agent.start_fn(&params->ext.agent.params,
							    &params->ext.agent.out_interface);
		break;

	case USER_PROXY_MOD_CMD_INIT:
		params->status = ops->init(params->mod);
		break;

	case USER_PROXY_MOD_CMD_PREPARE:
		params->status = ops->prepare(params->mod, params->ext.proc.sources,
					      params->ext.proc.num_of_sources,
					      params->ext.proc.sinks,
					      params->ext.proc.num_of_sinks);
		break;

	case USER_PROXY_MOD_CMD_PROC_READY:
		params->status = ops->is_ready_to_process(params->mod,
							  params->ext.proc.sources,
							  params->ext.proc.num_of_sources,
							  params->ext.proc.sinks,
							  params->ext.proc.num_of_sinks);
		break;

	case USER_PROXY_MOD_CMD_BIND:
		params->status = ops->bind(params->mod, params->ext.bind_data);
		break;

	case USER_PROXY_MOD_CMD_UNBIND:
		params->status = ops->unbind(params->mod, params->ext.bind_data);
		break;

	case USER_PROXY_MOD_CMD_RESET:
		params->status = ops->reset(params->mod);
		break;

	case USER_PROXY_MOD_CMD_FREE:
		params->status = ops->free(params->mod);
		break;

	case USER_PROXY_MOD_CMD_SET_CONF:
		params->status = ops->set_configuration(params->mod,
							params->ext.set_conf.config_id,
							params->ext.set_conf.pos,
							params->ext.set_conf.data_off_size,
							params->ext.set_conf.fragment,
							params->ext.set_conf.fragment_size,
							params->ext.set_conf.response,
							params->ext.set_conf.response_size);
		break;

	case USER_PROXY_MOD_CMD_GET_CONF:
		params->status = ops->get_configuration(params->mod,
							params->ext.get_conf.config_id,
							params->ext.get_conf.data_off_size,
							params->ext.get_conf.fragment,
							params->ext.get_conf.fragment_size);
		break;

	case USER_PROXY_MOD_CMD_SET_PROCMOD:
		params->status = ops->set_processing_mode(params->mod,
							  params->ext.proc_mode.mode);
		break;

	case USER_PROXY_MOD_CMD_GET_PROCMOD:
		params->ext.proc_mode.mode = ops->get_processing_mode(params->mod);
		break;

	case USER_PROXY_MOD_CMD_TRIGGER:
		params->status = ops->trigger(params->mod, params->ext.trigger_data);
		break;

	default:
		params->status = -EINVAL;
		break;
	}
}

void userspace_proxy_worker_handler(struct k_work_user *work_item)
{
	struct user_work_item *user_work_item = CONTAINER_OF(work_item, struct user_work_item,
							     work_item);
	struct module_params *params = &user_work_item->params;

	userspace_proxy_handle_request(params->mod, params);
	k_event_post(user_work_item->event, DP_TASK_EVENT_IPC_DONE);
}
