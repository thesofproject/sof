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
#include <sof/kernel/llext_manager.h>
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
 * IPC4 message routing with the LL scheduler in user space
 * (CONFIG_SOF_USERSPACE_LL). The routers in handler-user-kernel.c run in the
 * kernel IPC thread and call these functions for the messages that create,
 * configure, connect and delete pipelines and modules. They do what needs
 * kernel mode (target core lookup, multicore sequencing, driver lookup and
 * module loading) and forward the message to the user-space IPC thread of the
 * target core, which runs the handlers in src/ipc/ipc4/handler-user.c. With
 * the LL scheduler in kernel space, handler-ll-kernel.c calls those handlers
 * directly instead.
 */

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

/*
 * Determine the target core for an IPC4 module message.
 * Falls back to current core when no component is bound yet.
 */
static unsigned int ipc4_user_target_core_module(struct ipc4_message_request *ipc4)
{
	/*
	 * Also works for struct ipc4_module_large_config, struct ipc4_module_bind_unbind,
	 * struct ipc4_module_delete_instance
	 */
	struct ipc4_module_config *config = (struct ipc4_module_config *)ipc4;
	uint32_t module_id = config->primary.r.module_id;

	if (module_id) {
		uint32_t instance_id = config->primary.r.instance_id;
		struct comp_dev *dev = ipc4_get_comp_dev(IPC4_COMP_ID(module_id, instance_id));

		if (dev)
			return dev->ipc_config.core;
	}

	return cpu_get_id();
}

/*
 * Handle a multi-pipeline SET_PIPELINE_STATE, where the pipelines are on
 * different cores. The user-space IPC thread can't use IDC to reach the
 * other cores, so the kernel runs the prepare phase on all pipelines and
 * then the trigger phase on all pipelines, forwarding each step to the
 * user-space IPC thread of the core, owning the pipeline.
 */
__cold static int ipc4_user_set_pipeline_state_multicore(struct ipc4_message_request *ipc4,
							 const uint32_t *ppl_id,
							 unsigned int ppl_count)
{
	static const uint32_t phases[] = {
		IDC_PPL_STATE_PHASE_PREPARE,
		IDC_PPL_STATE_PHASE_TRIGGER,
	};
	struct ipc *ipc = ipc_get();
	struct ipc_user *pdata = ipc->ipc_user_pdata;
	struct ipc4_pipeline_set_state state;
	struct ipc_comp_dev *ppl_icd;
	unsigned int i, j;
	int ret;

	assert_can_be_cold();

	for (j = 0; j < ARRAY_SIZE(phases); j++) {
		for (i = 0; i < ppl_count; i++) {
			ppl_icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, ppl_id[i],
							 IPC_COMP_ALL);
			if (!ppl_icd) {
				ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", ppl_id[i]);
				return IPC4_INVALID_RESOURCE_ID;
			}

			/* Forward as a single-pipeline request */
			state.primary.dat = ipc4->primary.dat;
			state.extension.dat = ipc4->extension.dat;
			state.primary.r.ppl_id = ppl_id[i];
			state.extension.r.multi_ppl = 0;

			pdata->ppl_phase = phases[j];
			ret = ipc_user_forward_cmd(state.primary.dat, state.extension.dat,
						   ppl_icd->core);
			pdata->ppl_phase = 0;
			if (ret != 0)
				return ret;
		}
	}

	return IPC4_SUCCESS;
}

__cold int ipc4_route_chain_dma(struct ipc4_message_request *ipc4)
{
	assert_can_be_cold();

#if CONFIG_COMP_CHAIN_DMA
	return ipc4_process_chain_dma(ipc4, ipc4_chain_dma_drv());
#else
	return IPC4_UNAVAILABLE;
#endif
}

int ipc4_route_create_pipeline(struct ipc4_message_request *ipc4)
{
	const struct ipc4_pipeline_create *create =
		(const struct ipc4_pipeline_create *)ipc4;

	return ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
				    create->extension.r.core_id);
}

int ipc4_route_delete_pipeline(struct ipc4_message_request *ipc4)
{
	struct ipc *ipc = ipc_get();
	const struct ipc4_pipeline_delete *del = (const struct ipc4_pipeline_delete *)ipc4;
	struct ipc_comp_dev *ppl = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
							  del->primary.r.instance_id,
							  IPC_COMP_ALL);

	if (!ppl)
		return IPC4_INVALID_RESOURCE_ID;

	return ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat, ppl->core);
}

int ipc4_route_set_pipeline_state(struct ipc4_message_request *ipc4)
{
	struct ipc *ipc = ipc_get();
	struct ipc4_pipeline_set_state state = {
		.primary.dat = ipc4->primary.dat,
		.extension.dat = ipc4->extension.dat,
	};
	const uint32_t *ppl_id;
	unsigned int ppl_count;
	int id = ipc4_pipeline_id_get(ipc4, &state, &ppl_id, &ppl_count);

	if (id < 0)
		return IPC4_INVALID_RESOURCE_ID;

	struct ipc_comp_dev *ppl = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, id,
							  IPC_COMP_ALL);

	if (!ppl)
		return IPC4_INVALID_RESOURCE_ID;

	/*
	 * Validate all pipelines before any state change and check,
	 * if they span multiple cores, needing per-core processing
	 */
	bool multicore = false;

	for (unsigned int i = 1; i < ppl_count; i++) {
		struct ipc_comp_dev *icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
								  ppl_id[i], IPC_COMP_ALL);

		if (!icd) {
			ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", ppl_id[i]);
			return IPC4_INVALID_RESOURCE_ID;
		}

		if (icd->core != ppl->core)
			multicore = true;
	}

	if (multicore)
		return ipc4_user_set_pipeline_state_multicore(ipc4, ppl_id, ppl_count);

	return ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat, ppl->core);
}

__cold int ipc4_route_init_module(struct ipc4_message_request *ipc4)
{
	const struct ipc4_module_init_instance *mi =
		(const struct ipc4_module_init_instance *)ipc4;

	if (cpu_is_me(mi->extension.r.core_id) && !mi->extension.r.proc_domain) {
		BUILD_ASSERT(sizeof(struct comp_driver) + sizeof(struct tr_ctx) <=
			     sizeof(((struct ipc_user *)0)->init_drv_data),
			     "ipc_user.init_drv_data too small for driver copy");

		/* User-space init: kernel does driver lookup only (requires
		 * access to IMR manifest and driver list in kernel memory).
		 * Component creation (drv->ops.create) runs in user thread
		 * so untrusted module code does not execute in kernel context.
		 */
		uint32_t comp_id = IPC4_COMP_ID(mi->primary.r.module_id,
						mi->primary.r.instance_id);
		const struct comp_driver *drv = ipc4_get_comp_drv(IPC4_MOD_ID(comp_id));

		if (!drv || !drv->ops.create)
			return IPC4_MOD_NOT_INITIALIZED;

		struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(comp_id);

		if (ctx && drv->type == SOF_COMP_MODULE_ADAPTER) {
			int err = ipc4_user_module_load(drv, mi);

			if (err < 0)
				return IPC4_MOD_NOT_INITIALIZED;
		}

		ipc_user_get()->init_drv = drv;
		return ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
					    mi->extension.r.core_id);
	}

	/*
	 * DP module creation starts running in kernel mode and
	 * switches to userspace later via a scheduler_dp_thread_ipc()
	 * call in module_init().
	 */
	return ipc4_init_module_instance(ipc4);
}

__cold int ipc4_route_module_config(struct ipc4_message_request *ipc4, bool set,
				    struct ipc_msg *reply)
{
	/* Forward to user thread for privilege-separated execution */
	int ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
				       ipc4_user_target_core_module(ipc4));

	if (!set && !ret) {
		struct ipc_user *pdata = ipc_user_get();

		reply->extension = pdata->reply_ext;
	}

	return ret;
}

__cold int ipc4_route_large_config_get(struct ipc4_message_request *ipc4, struct ipc_msg *reply)
{
	const struct ipc4_module_large_config *config =
		(const struct ipc4_module_large_config *)ipc4;
	int ret;

	/* Base firmware (module_id==0): keep in kernel —
	 * ipc4_get_comp_drv() accesses IMR manifest which
	 * has no user-space partition.
	 */
	if (!config->primary.r.module_id)
		return ipc4_get_large_config_module_instance(ipc4);

	/* Module case: forward to user thread */
	ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
				   ipc4_user_target_core_module(ipc4));
	if (!ret) {
		struct ipc_user *pdata = ipc_user_get();
		/*
		 * pdata is writable by user threads: read the size once and
		 * always reply from the kernel copy of the IPC data buffer,
		 * which is where the user thread wrote the reply.
		 */
		uint32_t size = pdata->reply_tx_size;

		if (size > SOF_IPC_MSG_MAX_SIZE || size > MAILBOX_DSPBOX_SIZE) {
			ipc_cmd_err(&ipc_tr, "reply size %u too large", size);
			return IPC4_INVALID_CONFIG_DATA_STRUCT;
		}

		reply->extension = pdata->reply_ext;
		reply->tx_size = size;
		reply->tx_data = ipc_ks(ipc_get())->comp_data;
	}

	return ret;
}

__cold int ipc4_route_large_config_set(struct ipc4_message_request *ipc4)
{
	const struct ipc4_module_large_config *config =
		(const struct ipc4_module_large_config *)ipc4;

	/* Base firmware: keep in kernel (IMR access) */
	if (!config->primary.r.module_id)
		return ipc4_set_large_config_module_instance(ipc4);

	return ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
				    ipc4_user_target_core_module(ipc4));
}

/*
 * bind and unbind can connect LL with DP. In that case it isn't
 * immediately clear whether the handler should run in the DP
 * thread context or in the LL IPC thread context. The LL IPC
 * thread has access to DP modules, so we have to perform
 * binding in the LL IPC thread context
 */
__cold int ipc4_route_bind(struct ipc4_message_request *ipc4)
{
	return ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
				    ipc4_user_target_core_module(ipc4));
}

/* DP / LL: see ipc4_route_bind() */
__cold int ipc4_route_unbind(struct ipc4_message_request *ipc4)
{
	return ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
				    ipc4_user_target_core_module(ipc4));
}

__cold int ipc4_route_delete_module(struct ipc4_message_request *ipc4)
{
	return ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
				    ipc4_user_target_core_module(ipc4));
}
