// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Piotr Makaruk <piotr.makaruk@intel.com>
 *	   Adrian Warecki <adrian.warecki@intel.com>
 */

#include <sof/common.h>
#include <stdbool.h>
#include <ipc4/notification.h>

#include <rtos/symbol.h>

static enum sof_ipc4_resource_event_type dir_to_xrun_event(enum sof_ipc_stream_direction dir)
{
	return (dir == SOF_IPC_STREAM_PLAYBACK) ? SOF_IPC4_GATEWAY_UNDERRUN_DETECTED :
						  SOF_IPC4_GATEWAY_OVERRUN_DETECTED;
}

bool send_copier_gateway_xrun_notif_msg(uint32_t pipeline_id, enum sof_ipc_stream_direction dir)
{
	return send_resource_notif(pipeline_id, dir_to_xrun_event(dir), SOF_IPC4_PIPELINE, NULL,
				   0);
}

bool send_gateway_xrun_notif_msg(uint32_t resource_id, enum sof_ipc_stream_direction dir)
{
	return send_resource_notif(resource_id, dir_to_xrun_event(dir), SOF_IPC4_GATEWAY, NULL, 0);
}

void send_mixer_underrun_notif_msg(uint32_t resource_id, uint32_t eos_flag, uint32_t data_mixed,
				   uint32_t expected_data_mixed)
{
	struct ipc4_mixer_underrun_event_data mixer_underrun_data;

	mixer_underrun_data.eos_flag = eos_flag;
	mixer_underrun_data.data_mixed = data_mixed;
	mixer_underrun_data.expected_data_mixed = expected_data_mixed;

	send_resource_notif(resource_id, SOF_IPC4_MIXER_UNDERRUN_DETECTED, SOF_IPC4_PIPELINE,
			    &mixer_underrun_data, sizeof(mixer_underrun_data));
}
EXPORT_SYMBOL(send_mixer_underrun_notif_msg);

void send_process_data_error_notif_msg(uint32_t resource_id, uint32_t error_code)
{
	struct ipc4_process_data_error_event_data error_data;

	error_data.error_code = error_code;

	send_resource_notif(resource_id, SOF_IPC4_PROCESS_DATA_ERROR, SOF_IPC4_MODULE_INSTANCE,
			    &error_data, sizeof(error_data));
}
