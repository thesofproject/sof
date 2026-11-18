// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
// Author: Rander Wang <rander.wang@linux.intel.com>
/*
 * IPC (InterProcessor Communication) provides a method of two way
 * communication between the host processor and the DSP. The IPC used here
 * utilises a shared mailbox and door bell between the host and DSP.
 *
 */

#include <sof/audio/buffer.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/kernel/ipc/driver.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/llext_manager.h>
#include <sof/math/numbers.h>
#include <sof/tlv.h>
#include <sof/trace/trace.h>
#include <ipc4/error_status.h>
#include <ipc/header.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#include <ipc4/notification.h>
#include <ipc4/handler.h>
#include <ipc/trace.h>
#include <user/trace.h>

#include <rtos/atomic.h>
#include <rtos/kernel.h>
#include <sof/lib_manager.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#include "handler-route.h"

/*
 * IPC4 message routing with the LL scheduler in kernel space (without
 * CONFIG_SOF_USERSPACE_LL). The routers in handler-user-kernel.c run in the
 * kernel IPC thread and call these functions, which run the handlers in
 * src/ipc/ipc4/handler-user.c and src/ipc/ipc4/helper.c directly in that
 * thread. With the LL scheduler in user space, handler-ll-user.c forwards
 * the messages to the user-space IPC thread instead.
 */

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

__cold int ipc4_route_create_pipeline(struct ipc4_message_request *ipc4)
{
	struct ipc *ipc = ipc_get();

	assert_can_be_cold();

	return ipc_pipeline_new(ipc, (ipc_pipe_new *)ipc4);
}


__cold int ipc4_route_delete_pipeline(struct ipc4_message_request *ipc4)
{
	struct ipc4_pipeline_delete *pipe;
	struct ipc *ipc = ipc_get();

	assert_can_be_cold();

	pipe = (struct ipc4_pipeline_delete *)ipc4;
	tr_dbg(&ipc_tr, "ipc4 delete pipeline %x:", (uint32_t)pipe->primary.r.instance_id);

	return ipc_pipeline_free(ipc, pipe->primary.r.instance_id);
}


int ipc4_route_set_pipeline_state(struct ipc4_message_request *ipc4)
{
	return ipc4_set_pipeline_state(ipc4);
}

__cold int ipc4_route_init_module(struct ipc4_message_request *ipc4)
{
	return ipc4_init_module_instance(ipc4);
}

int ipc4_route_module_config(struct ipc4_message_request *ipc4, bool set,
			     struct ipc_msg *reply)
{
	uint32_t reply_ext;
	int ret;

	ret = ipc4_process_module_config(ipc4, set, &reply_ext);

	if (!set)
		reply->extension = reply_ext;

	return ret;
}


__cold int ipc4_route_large_config_get(struct ipc4_message_request *ipc4, struct ipc_msg *reply)
{
	return ipc4_get_large_config_module_instance(ipc4);
}

__cold int ipc4_route_large_config_set(struct ipc4_message_request *ipc4)
{
	return ipc4_set_large_config_module_instance(ipc4);
}

__cold int ipc4_route_bind(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_bind_unbind *bu = (struct ipc4_module_bind_unbind *)ipc4;
	struct ipc *ipc = ipc_get();

	assert_can_be_cold();

	tr_dbg(&ipc_tr, "%x : %x with %x : %x",
	       (uint32_t)bu->primary.r.module_id, (uint32_t)bu->primary.r.instance_id,
	       (uint32_t)bu->extension.r.dst_module_id, (uint32_t)bu->extension.r.dst_instance_id);

	return ipc4_comp_connect(ipc, bu);
}


__cold int ipc4_route_unbind(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_bind_unbind *bu = (struct ipc4_module_bind_unbind *)ipc4;
	struct ipc *ipc = ipc_get();

	assert_can_be_cold();

	tr_dbg(&ipc_tr, "%x : %x with %x : %x",
	       (uint32_t)bu->primary.r.module_id, (uint32_t)bu->primary.r.instance_id,
	       (uint32_t)bu->extension.r.dst_module_id, (uint32_t)bu->extension.r.dst_instance_id);

	return ipc4_comp_disconnect(ipc, bu);
}


__cold int ipc4_route_delete_module(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_delete_instance module;
	struct ipc *ipc = ipc_get();
	uint32_t comp_id;

	assert_can_be_cold();

	int ret = memcpy_s(&module, sizeof(module), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	tr_dbg(&ipc_tr, "%x : %x", (uint32_t)module.primary.r.module_id,
	       (uint32_t)module.primary.r.instance_id);

	comp_id = IPC4_COMP_ID(module.primary.r.module_id, module.primary.r.instance_id);
	ret = ipc_comp_free(ipc, comp_id);
	if (ret < 0) {
		ipc_cmd_err(&ipc_tr, "failed to delete module instance %x : %x",
			    (uint32_t)module.primary.r.module_id,
			    (uint32_t)module.primary.r.instance_id);
		ret = IPC4_INVALID_RESOURCE_ID;
	}

	return ret;
}

