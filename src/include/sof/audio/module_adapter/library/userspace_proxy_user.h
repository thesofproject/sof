/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Warecki <adrian.warecki@intel.com>
 */

#ifndef __SOF_AUDIO_USERSPACE_PROXY_USER_H__
#define __SOF_AUDIO_USERSPACE_PROXY_USER_H__

#if CONFIG_SOF_USERSPACE_PROXY
struct module_agent_params {
	system_agent_start_fn start_fn;
	struct system_agent_params params;
	byte_array_t mod_cfg;
	const void *out_interface;
};

struct module_large_cfg_set_params {
	uint32_t config_id;
	enum module_cfg_fragment_position pos;
	uint32_t data_off_size;
	const uint8_t *fragment;
	size_t fragment_size;
	uint8_t *response;
	size_t response_size;
};

struct module_large_cfg_get_params {
	uint32_t config_id;
	uint32_t *data_off_size;
	uint8_t *fragment;
	size_t fragment_size;
};

struct module_processing_mode_params {
	enum module_processing_mode mode;
};

struct module_process_params {
	struct sof_source **sources;
	int num_of_sources;
	struct sof_sink **sinks;
	int num_of_sinks;
};

enum userspace_proxy_cmd {
	USER_PROXY_MOD_CMD_AGENT_START,
	USER_PROXY_MOD_CMD_INIT,
	USER_PROXY_MOD_CMD_PREPARE,
	USER_PROXY_MOD_CMD_PROC_READY,
	USER_PROXY_MOD_CMD_SET_PROCMOD,
	USER_PROXY_MOD_CMD_GET_PROCMOD,
	USER_PROXY_MOD_CMD_SET_CONF,
	USER_PROXY_MOD_CMD_GET_CONF,
	USER_PROXY_MOD_CMD_BIND,
	USER_PROXY_MOD_CMD_UNBIND,
	USER_PROXY_MOD_CMD_RESET,
	USER_PROXY_MOD_CMD_FREE,
	USER_PROXY_MOD_CMD_TRIGGER
};

struct module_params {
	enum userspace_proxy_cmd cmd;
	int status;
	struct processing_module *mod;
	struct userspace_context *context;
	/* The field used in the union depends on the value of cmd */
	union {
		struct module_agent_params		agent;
		struct module_large_cfg_set_params	set_conf;
		struct module_large_cfg_get_params	get_conf;
		struct module_processing_mode_params	proc_mode;
		struct module_process_params		proc;
		struct bind_info			*bind_data;
		int					trigger_data;
	} ext;
};

struct user_work_item {
	struct k_work_user work_item;		/* ipc worker workitem			*/
	struct k_event *event;			/* ipc worker done event		*/
	struct module_params params;
};

void userspace_proxy_handle_request(struct processing_module *mod, struct module_params *params);

void userspace_proxy_worker_handler(struct k_work_user *work_item);

#endif /* CONFIG_SOF_USERSPACE_PROXY */

#endif /* __SOF_AUDIO_USERSPACE_PROXY_USER_H__ */
