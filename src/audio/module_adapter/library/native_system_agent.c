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

void SystemServiceLogMessage(AdspLogPriority log_priority, uint32_t log_entry,
			     AdspLogHandle const *log_handle, uint32_t param1,
			     uint32_t param2, uint32_t param3, uint32_t param4);

AdspErrorCode SystemServiceSafeMemcpy(void *RESTRICT dst, size_t maxlen,
				      const void *RESTRICT src, size_t len);

AdspErrorCode SystemServiceSafeMemmove(void *dst, size_t maxlen,
				       const void *src, size_t len);

void *SystemServiceVecMemset(void *dst, int c, size_t len);

AdspErrorCode SystemServiceCreateNotification(NotificationParams *params,
					      uint8_t *notification_buffer,
					      uint32_t notification_buffer_size,
					      AdspNotificationHandle *handle);

AdspErrorCode SystemServiceSendNotificationMessage(NotificationTarget notification_target,
						   AdspNotificationHandle message,
						   uint32_t actual_payload_size);

AdspErrorCode SystemServiceGetInterface(AdspIfaceId id, SystemServiceIface **iface);

typedef void* (*native_create_instance_f)(void *mod_cfg, void *parent_ppl,
					  void **mod_ptr);

struct native_system_agent native_sys_agent = {
		.system_service = {
			.LogMessage  = SystemServiceLogMessage,
			.SafeMemcpy = SystemServiceSafeMemcpy,
			.SafeMemmove = SystemServiceSafeMemmove,
			.VecMemset = SystemServiceVecMemset,
			.NotificationCreate = SystemServiceCreateNotification,
			.NotificationSend = SystemServiceSendNotificationMessage,
			.GetInterface = SystemServiceGetInterface
		},
	};

void *native_system_agent_start(struct adsp_system_service **sys_service, uint32_t entry_point,
				uint32_t module_id, uint32_t instance_id,
				uint32_t core_id, uint32_t log_handle, void *mod_cfg)
{
	native_sys_agent.module_id = module_id;
	native_sys_agent.instance_id = instance_id;
	native_sys_agent.core_id = core_id;
	native_sys_agent.log_handle = log_handle;

	void *system_agent_p = &native_sys_agent;

	*sys_service = &native_sys_agent.system_service;

	native_create_instance_f ci = (native_create_instance_f)(entry_point);

	return ci(mod_cfg, NULL, &system_agent_p);
}
