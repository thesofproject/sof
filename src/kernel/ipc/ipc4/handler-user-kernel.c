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
#include <sof/ipc/driver.h>
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

#include "../audio/copier/ipcgtw_copier.h"

/* Command format errors during fuzzing are reported for virtually all
 * commands, and the resulting flood of logging becomes a severe
 * performance penalty (i.e. we get a lot less fuzzing done per CPU
 * cycle).
 */
#ifdef CONFIG_ARCH_POSIX_LIBFUZZER
#define ipc_cmd_err(...)
#else
#define ipc_cmd_err(...) tr_err(__VA_ARGS__)
#endif

/* Kernel part of the IPC4 module and global message handling: the routers,
 * which run in the kernel IPC thread, and the handlers that only they call.
 * The part that also runs in user mode is in src/ipc/ipc4/handler-user.c
 */

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

/* Userspace message context, copied in/out by kernel IPC thread. */
/* fw sends a fw ipc message to send the status of the last host ipc message */
static struct ipc_msg *msg_reply;

#ifdef CONFIG_SOF_USERSPACE_LL
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
#else /* CONFIG_SOF_USERSPACE_LL */
__cold static int ipc4_new_pipeline(struct ipc4_message_request *ipc4)
{
	struct ipc *ipc = ipc_get();

	assert_can_be_cold();

	return ipc_pipeline_new(ipc, (ipc_pipe_new *)ipc4);
}

__cold static int ipc4_delete_pipeline(struct ipc4_message_request *ipc4)
{
	struct ipc4_pipeline_delete *pipe;
	struct ipc *ipc = ipc_get();

	assert_can_be_cold();

	pipe = (struct ipc4_pipeline_delete *)ipc4;
	tr_dbg(&ipc_tr, "ipc4 delete pipeline %x:", (uint32_t)pipe->primary.r.instance_id);

	return ipc_pipeline_free(ipc, pipe->primary.r.instance_id);
}
#endif /* CONFIG_SOF_USERSPACE_LL */

__cold static int ipc4_process_chain_dma(struct ipc4_message_request *ipc4)
{
	assert_can_be_cold();

#if CONFIG_COMP_CHAIN_DMA
	struct ipc_comp_dev *cdma_comp;
	struct ipc *ipc = ipc_get();
	const struct ipc4_chain_dma *cdma = (const struct ipc4_chain_dma *)ipc4;
	int comp_id;
	int ret;

	comp_id = IPC4_COMP_ID(cdma->primary.r.host_dma_id + IPC4_MAX_MODULE_COUNT, 0);
	cdma_comp = ipc_get_comp_by_id(ipc, comp_id);

	if (!cdma_comp) {
		/*
		 * Nothing to do when the chainDMA is not allocated and asked to
		 * be freed
		 */
		if (!cdma->primary.r.allocate && !cdma->primary.r.enable)
			return IPC4_SUCCESS;

		ret = ipc4_chain_manager_create(cdma);
		if (ret < 0)
			return IPC4_FAILURE;

		cdma_comp = ipc_get_comp_by_id(ipc, comp_id);
		if (!cdma_comp) {
			return IPC4_FAILURE;
		}

		ret = ipc4_chain_dma_state(cdma_comp->cd, cdma);
		if (ret < 0) {
			comp_free(cdma_comp->cd);
			list_item_del(&cdma_comp->list);
			rfree(cdma_comp);
			return IPC4_FAILURE;
		}

		return IPC4_SUCCESS;
	}

	ret = ipc4_chain_dma_state(cdma_comp->cd, cdma);
	if (ret < 0)
		return IPC4_INVALID_CHAIN_STATE_TRANSITION;

	return IPC4_SUCCESS;
#else /* CONFIG_COMP_CHAIN_DMA */
	return IPC4_UNAVAILABLE;
#endif /* CONFIG_COMP_CHAIN_DMA */
}

__cold static int ipc4_process_ipcgtw_cmd(struct ipc4_message_request *ipc4)
{
	assert_can_be_cold();

#if CONFIG_IPC4_GATEWAY
	struct ipc *ipc = ipc_get();
	uint32_t reply_size = 0;
	int err;

	err = copier_ipcgtw_process((const struct ipc4_ipcgtw_cmd *)ipc4, ipc->comp_data,
				    &reply_size);
	/* reply size is returned in header extension dword */
	msg_reply->extension = reply_size;

	if (reply_size > 0) {
		msg_reply->tx_data = ipc->comp_data;
		msg_reply->tx_size = reply_size;
	}

	return err < 0 ? IPC4_FAILURE : IPC4_SUCCESS;
#else /* CONFIG_IPC4_GATEWAY */
	ipc_cmd_err(&ipc_tr, "CONFIG_IPC4_GATEWAY is disabled");
	return IPC4_UNAVAILABLE;
#endif /* CONFIG_IPC4_GATEWAY */
}

static int ipc_glb_gdb_debug(struct ipc4_message_request *ipc4)
{
#if CONFIG_GDBSTUB
	ipc_enter_gdb = true;
	return IPC4_SUCCESS;
#else
	return IPC4_UNAVAILABLE;
#endif
}

int ipc4_user_process_glb_message(struct ipc4_message_request *ipc4,
				  struct ipc_msg *reply)
{
#ifdef CONFIG_SOF_USERSPACE_LL
	struct ipc *ipc = ipc_get();
#endif
	uint32_t type;
	int ret;

	type = ipc4->primary.r.type;
	msg_reply = reply;

	switch (type) {
	case SOF_IPC4_GLB_BOOT_CONFIG:
	case SOF_IPC4_GLB_ROM_CONTROL:
	case SOF_IPC4_GLB_PERF_MEASUREMENTS_CMD:
	case SOF_IPC4_GLB_LOAD_MULTIPLE_MODULES:
	case SOF_IPC4_GLB_UNLOAD_MULTIPLE_MODULES:
		ipc_cmd_err(&ipc_tr, "not implemented ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;

	case SOF_IPC4_GLB_CHAIN_DMA:
		ret = ipc4_process_chain_dma(ipc4);
		break;

	/* pipeline settings */
	case SOF_IPC4_GLB_CREATE_PIPELINE:
#ifdef CONFIG_SOF_USERSPACE_LL
	{
		const struct ipc4_pipeline_create *create =
			(const struct ipc4_pipeline_create *)ipc4;

		ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
					   create->extension.r.core_id);
	}
#else
		ret = ipc4_new_pipeline(ipc4);
#endif
		break;
	case SOF_IPC4_GLB_DELETE_PIPELINE:
#ifdef CONFIG_SOF_USERSPACE_LL
	{
		const struct ipc4_pipeline_delete *del = (const struct ipc4_pipeline_delete *)ipc4;
		struct ipc_comp_dev *ppl = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
								  del->primary.r.instance_id,
								  IPC_COMP_ALL);

		if (!ppl) {
			ret = IPC4_INVALID_RESOURCE_ID;
			break;
		}
		ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat, ppl->core);
	}
#else
		ret = ipc4_delete_pipeline(ipc4);
#endif
		break;
	case SOF_IPC4_GLB_SET_PIPELINE_STATE:
#ifdef CONFIG_SOF_USERSPACE_LL
	{
		struct ipc4_pipeline_set_state state = {
			.primary.dat = ipc4->primary.dat,
			.extension.dat = ipc4->extension.dat,
		};
		const uint32_t *ppl_id;
		unsigned int ppl_count;
		int id = ipc4_pipeline_id_get(ipc4, &state, &ppl_id, &ppl_count);
		if (id < 0) {
			ret = IPC4_INVALID_RESOURCE_ID;
			break;
		}

		struct ipc_comp_dev *ppl = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, id,
								  IPC_COMP_ALL);

		if (!ppl) {
			ret = IPC4_INVALID_RESOURCE_ID;
			break;
		}

		/*
		 * Validate all pipelines before any state change and check,
		 * if they span multiple cores, needing per-core processing
		 */
		bool multicore = false;

		ret = IPC4_SUCCESS;
		for (unsigned int i = 1; i < ppl_count; i++) {
			struct ipc_comp_dev *icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
									  ppl_id[i], IPC_COMP_ALL);

			if (!icd) {
				ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", ppl_id[i]);
				ret = IPC4_INVALID_RESOURCE_ID;
				break;
			}

			if (icd->core != ppl->core)
				multicore = true;
		}

		if (ret != IPC4_SUCCESS)
			break;

		if (multicore)
			ret = ipc4_user_set_pipeline_state_multicore(ipc4, ppl_id, ppl_count);
		else
			ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
						   ppl->core);
	}
#else /* CONFIG_SOF_USERSPACE_LL */
		ret = ipc4_set_pipeline_state(ipc4);
#endif /* CONFIG_SOF_USERSPACE_LL */
		break;

	case SOF_IPC4_GLB_GET_PIPELINE_STATE:
	case SOF_IPC4_GLB_GET_PIPELINE_CONTEXT_SIZE:
	case SOF_IPC4_GLB_SAVE_PIPELINE:
	case SOF_IPC4_GLB_RESTORE_PIPELINE:
		ipc_cmd_err(&ipc_tr, "not implemented ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;

	case SOF_IPC4_GLB_INTERNAL_MESSAGE:
		ipc_cmd_err(&ipc_tr, "not implemented ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;

	/* Notification (FW to SW driver) */
	case SOF_IPC4_GLB_NOTIFICATION:
		ipc_cmd_err(&ipc_tr, "not implemented ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;

	case SOF_IPC4_GLB_IPCGATEWAY_CMD:
		ret = ipc4_process_ipcgtw_cmd(ipc4);
		break;

	case SOF_IPC4_GLB_ENTER_GDB:
		ret = ipc_glb_gdb_debug(ipc4);
		break;

	default:
		ipc_cmd_err(&ipc_tr, "unsupported ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;
	}

	return ret;
}

/*
 * Ipc4 Module message <------> ipc3 module message
 * init module <-------> create component
 * bind modules <-------> connect components
 * module set_large_config <-------> component cmd
 * delete module <-------> free component
 */

__cold static int ipc4_init_module_instance(struct ipc4_message_request *ipc4)
{
	const struct ipc4_module_init_instance *module_init =
		(const struct ipc4_module_init_instance *)ipc4;
	struct comp_dev *dev;

	assert_can_be_cold();

	tr_dbg(&ipc_tr, "%x : %x",
	       (uint32_t)module_init->primary.r.module_id,
	       (uint32_t)module_init->primary.r.instance_id);

	/* Pass IPC to target core */
	if (!cpu_is_me(module_init->extension.r.core_id))
		return ipc4_process_on_core(module_init->extension.r.core_id, false);

	dev = comp_new_ipc4(module_init);
	if (!dev) {
		ipc_cmd_err(&ipc_tr, "error: failed to init module %x : %x",
			    (uint32_t)module_init->primary.r.module_id,
			    (uint32_t)module_init->primary.r.instance_id);
		return IPC4_MOD_NOT_INITIALIZED;
	}

	return 0;
}

#ifndef CONFIG_SOF_USERSPACE_LL
__cold static int ipc4_bind_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_bind_unbind *bu = (struct ipc4_module_bind_unbind *)ipc4;
	struct ipc *ipc = ipc_get();

	assert_can_be_cold();

	tr_dbg(&ipc_tr, "%x : %x with %x : %x",
	       (uint32_t)bu->primary.r.module_id, (uint32_t)bu->primary.r.instance_id,
	       (uint32_t)bu->extension.r.dst_module_id, (uint32_t)bu->extension.r.dst_instance_id);

	return ipc4_comp_connect(ipc, bu);
}

__cold static int ipc4_unbind_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_bind_unbind *bu = (struct ipc4_module_bind_unbind *)ipc4;
	struct ipc *ipc = ipc_get();

	assert_can_be_cold();

	tr_dbg(&ipc_tr, "%x : %x with %x : %x",
	       (uint32_t)bu->primary.r.module_id, (uint32_t)bu->primary.r.instance_id,
	       (uint32_t)bu->extension.r.dst_module_id, (uint32_t)bu->extension.r.dst_instance_id);

	return ipc4_comp_disconnect(ipc, bu);
}
#endif /* !CONFIG_SOF_USERSPACE_LL */

#ifndef CONFIG_SOF_USERSPACE_LL
static int ipc4_set_get_config_module_instance(struct ipc4_message_request *ipc4, bool set)
{
	uint32_t reply_ext;
	int ret;

	ret = ipc4_process_module_config(ipc4, set, &reply_ext);

	if (!set)
		msg_reply->extension = reply_ext;

	return ret;
}
#endif /* !CONFIG_SOF_USERSPACE_LL */

__cold static int ipc4_get_large_config_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_large_config_reply reply;
	const struct ipc4_module_large_config *config =
		(const struct ipc4_module_large_config *)ipc4;
	char *data = ipc_get()->comp_data;
	const struct comp_driver *drv;
	struct comp_dev *dev = NULL;
	uint32_t data_offset;
	int ret;

	assert_can_be_cold();

	tr_dbg(&ipc_tr, "%x : %x",
	       (uint32_t)config->primary.r.module_id, (uint32_t)config->primary.r.instance_id);

	/* get component dev for non-basefw since there is no
	 * component dev for basefw
	 */
	if (config->primary.r.module_id) {
		uint32_t comp_id;

		comp_id = IPC4_COMP_ID(config->primary.r.module_id,
				       config->primary.r.instance_id);
		dev = ipc4_get_comp_dev(comp_id);
		if (!dev)
			return IPC4_INVALID_RESOURCE_ID;

		drv = dev->drv;

		/* Pass IPC to target core */
		if (!cpu_is_me(dev->ipc_config.core))
			return ipc4_process_on_core(dev->ipc_config.core, false);
	} else {
		/* BaseFW module has only 0th instance */
		if (config->primary.r.instance_id)
			return IPC4_INVALID_RESOURCE_ID;

		drv = ipc4_get_comp_drv(config->primary.r.module_id);
	}

	if (!drv)
		return IPC4_INVALID_RESOURCE_ID;

	if (!drv->ops.get_large_config)
		return IPC4_INVALID_REQUEST;

	data_offset = config->extension.r.data_off_size;

	/* check for vendor param first */
	if (config->extension.r.large_param_id == VENDOR_CONFIG_PARAM) {
		/* data_off_size is a 20-bit host-controlled field, so it can
		 * claim far more than the hostbox can physically hold.
		 */
		if (data_offset > MAILBOX_HOSTBOX_SIZE) {
			ipc_cmd_err(&ipc_tr, "data_off_size %u exceeds mailbox bound",
				    data_offset);
			return IPC4_INVALID_CONFIG_DATA_STRUCT;
		}
		/* For now only vendor_config case uses payload from hostbox */
		dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
					 config->extension.r.data_off_size);
		ret = ipc4_get_vendor_config_module_instance(dev, drv,
							     config->extension.r.init_block,
							     config->extension.r.final_block,
							     &data_offset, data,
							     (const char *)MAILBOX_HOSTBOX_BASE);
	} else {
#if CONFIG_LIBRARY
		data += sizeof(reply);
#endif
		ipc4_prepare_for_kcontrol_get(dev, config->extension.r.large_param_id,
					      data, data_offset);

		ret = drv->ops.get_large_config(dev, config->extension.r.large_param_id,
						config->extension.r.init_block,
						config->extension.r.final_block,
						&data_offset, data);
	}

	/* set up ipc4 error code for reply data */
	if (ret < 0)
		ret = IPC4_INVALID_RESOURCE_ID;

	/* Copy host config and overwrite */
	reply.extension.dat = config->extension.dat;
	reply.extension.r.data_off_size = data_offset;

	/* The last block, no more data */
	if (!config->extension.r.final_block && data_offset < SOF_IPC_MSG_MAX_SIZE)
		reply.extension.r.final_block = 1;

	/* Indicate last block if error occurs */
	if (ret)
		reply.extension.r.final_block = 1;

	/* no need to allocate memory for reply msg */
	if (ret)
		return ret;

	/* A module must not report more reply data than the DSP mailbox can hold. */
	if (data_offset > MAILBOX_DSPBOX_SIZE) {
		ipc_cmd_err(&ipc_tr, "reply size %u exceeds mailbox bound", data_offset);
		return IPC4_INVALID_CONFIG_DATA_STRUCT;
	}

	msg_reply->extension = reply.extension.dat;
	msg_reply->tx_size = data_offset;
	msg_reply->tx_data = data;
	return ret;
}

__cold static int ipc4_set_large_config_module_instance(struct ipc4_message_request *ipc4)
{
	const struct ipc4_module_large_config *config =
		(const struct ipc4_module_large_config *)ipc4;
	struct comp_dev *dev = NULL;
	const struct comp_driver *drv;
	int ret;

	assert_can_be_cold();

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 config->extension.r.data_off_size);
	tr_dbg(&ipc_tr, "%x : %x",
	       (uint32_t)config->primary.r.module_id, (uint32_t)config->primary.r.instance_id);

	if (config->primary.r.module_id) {
		uint32_t comp_id;

		comp_id = IPC4_COMP_ID(config->primary.r.module_id, config->primary.r.instance_id);
		dev = ipc4_get_comp_dev(comp_id);
		if (!dev)
			return IPC4_INVALID_RESOURCE_ID;

		drv = dev->drv;

		/* Pass IPC to target core */
		if (!cpu_is_me(dev->ipc_config.core))
			return ipc4_process_on_core(dev->ipc_config.core, false);
	} else {
		/* BaseFW module has only 0th instance */
		if (config->primary.r.instance_id)
			return IPC4_INVALID_RESOURCE_ID;

		drv = ipc4_get_comp_drv(config->primary.r.module_id);
	}

	if (!drv)
		return IPC4_INVALID_RESOURCE_ID;

	if (!drv->ops.set_large_config)
		return IPC4_INVALID_REQUEST;

	/* check for vendor param first */
	if (config->extension.r.large_param_id == VENDOR_CONFIG_PARAM) {
		ret = ipc4_set_vendor_config_module_instance(dev, drv,
							     (uint32_t)config->primary.r.module_id,
							     (uint32_t)config->primary.r.instance_id,
							     config->extension.r.init_block,
							     config->extension.r.final_block,
							     config->extension.r.data_off_size,
							     (const char *)MAILBOX_HOSTBOX_BASE);
	} else {
#if CONFIG_LIBRARY
		struct ipc *ipc = ipc_get();
		const char *data = (const char *)ipc->comp_data + sizeof(*config);
#else
		const char *data = (const char *)MAILBOX_HOSTBOX_BASE;
#endif
		ret = drv->ops.set_large_config(dev, config->extension.r.large_param_id,
			config->extension.r.init_block, config->extension.r.final_block,
			config->extension.r.data_off_size, data);
		if (ret < 0) {
			ipc_cmd_err(&ipc_tr, "failed to set large_config_module_instance %x : %x",
				    (uint32_t)config->primary.r.module_id,
				    (uint32_t)config->primary.r.instance_id);
			ret = IPC4_INVALID_RESOURCE_ID;
		}
	}

	return ret;
}

#ifndef CONFIG_SOF_USERSPACE_LL
__cold static int ipc4_delete_module_instance(struct ipc4_message_request *ipc4)
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
#endif /* !CONFIG_SOF_USERSPACE_LL */

__cold int ipc4_user_process_module_message(struct ipc4_message_request *ipc4,
					    struct ipc_msg *reply)
{
	const struct ipc4_module_init_instance *mi;
	uint32_t type;
	int ret;

	assert_can_be_cold();

	type = ipc4->primary.r.type;
	msg_reply = reply;

	switch (type) {
	case SOF_IPC4_MOD_INIT_INSTANCE:
		mi = (const struct ipc4_module_init_instance *)ipc4;

		if (cpu_is_me(mi->extension.r.core_id) && !mi->extension.r.proc_domain &&
		    IS_ENABLED(CONFIG_SOF_USERSPACE_LL)) {
#ifdef CONFIG_SOF_USERSPACE_LL
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

			if (!drv || !drv->ops.create) {
				ret = IPC4_MOD_NOT_INITIALIZED;
				break;
			}

			struct lib_manager_mod_ctx *ctx = lib_manager_get_mod_ctx(comp_id);

			if (ctx && drv->type == SOF_COMP_MODULE_ADAPTER) {
				int err = ipc4_user_module_load(drv, mi);

				if (err < 0) {
					ret = IPC4_MOD_NOT_INITIALIZED;
					break;
				}
			}

			ipc_user_get()->init_drv = drv;
			ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
						   mi->extension.r.core_id);
#endif /* CONFIG_SOF_USERSPACE_LL */
		} else {
			/*
			 * DP module creation starts running in kernel mode and
			 * switches to userspace later via a scheduler_dp_thread_ipc()
			 * call in module_init().
			 */
			ret = ipc4_init_module_instance(ipc4);
		}
		break;
	case SOF_IPC4_MOD_CONFIG_GET:
#ifdef CONFIG_SOF_USERSPACE_LL
		/* Forward to user thread for privilege-separated execution */
		ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
					   ipc4_user_target_core_module(ipc4));
		if (!ret) {
			struct ipc_user *pdata = ipc_user_get();

			msg_reply->extension = pdata->reply_ext;
		}
#else
		ret = ipc4_set_get_config_module_instance(ipc4, false);
#endif
		break;
	case SOF_IPC4_MOD_CONFIG_SET:
#ifdef CONFIG_SOF_USERSPACE_LL
		/* Forward to user thread for privilege-separated execution */
		ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
					   ipc4_user_target_core_module(ipc4));
#else
		ret = ipc4_set_get_config_module_instance(ipc4, true);
#endif
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_GET:
#ifdef CONFIG_SOF_USERSPACE_LL
	{
		const struct ipc4_module_large_config *config =
			(const struct ipc4_module_large_config *)ipc4;

		if (config->primary.r.module_id) {
			/* Module case: forward to user thread */
			ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
						   ipc4_user_target_core_module(ipc4));
			if (!ret) {
				struct ipc_user *pdata = ipc_user_get();

				msg_reply->extension = pdata->reply_ext;
				msg_reply->tx_size = pdata->reply_tx_size;
				msg_reply->tx_data = pdata->reply_tx_data;
			}
		} else {
			/* Base firmware (module_id==0): keep in kernel —
			 * ipc4_get_comp_drv() accesses IMR manifest which
			 * has no user-space partition.
			 */
			ret = ipc4_get_large_config_module_instance(ipc4);
		}
	}
#else /* CONFIG_SOF_USERSPACE_LL */
		ret = ipc4_get_large_config_module_instance(ipc4);
#endif /* CONFIG_SOF_USERSPACE_LL */
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_SET:
#ifdef CONFIG_SOF_USERSPACE_LL
	{
		const struct ipc4_module_large_config *config =
			(const struct ipc4_module_large_config *)ipc4;

		if (config->primary.r.module_id) {
			ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
						   ipc4_user_target_core_module(ipc4));
		} else {
			/* Base firmware: keep in kernel (IMR access) */
			ret = ipc4_set_large_config_module_instance(ipc4);
		}
	}
#else
		ret = ipc4_set_large_config_module_instance(ipc4);
#endif
		break;
	case SOF_IPC4_MOD_BIND:
#ifdef CONFIG_SOF_USERSPACE_LL
		/*
		 * bind and unbind can connect LL with DP. In that case it isn't
		 * immediately clear whether the handler should run in the DP
		 * thread context or in the LL IPC thread context. The LL IPC
		 * thread has access to DP modules, so we have to perform
		 * binding in the LL IPC thread context
		 */
		ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
					   ipc4_user_target_core_module(ipc4));
#else
		ret = ipc4_bind_module_instance(ipc4);
#endif
		break;
	case SOF_IPC4_MOD_UNBIND:
#ifdef CONFIG_SOF_USERSPACE_LL
		/* DP / LL: see comment above */
		ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
					   ipc4_user_target_core_module(ipc4));
#else
		ret = ipc4_unbind_module_instance(ipc4);
#endif
		break;
	case SOF_IPC4_MOD_DELETE_INSTANCE:
#ifdef CONFIG_SOF_USERSPACE_LL
		ret = ipc_user_forward_cmd(ipc4->primary.dat, ipc4->extension.dat,
					   ipc4_user_target_core_module(ipc4));
#else
		ret = ipc4_delete_module_instance(ipc4);
#endif
		break;
	case SOF_IPC4_MOD_ENTER_MODULE_RESTORE:
	case SOF_IPC4_MOD_EXIT_MODULE_RESTORE:
		ret = IPC4_UNAVAILABLE;
		break;
	default:
		ret = IPC4_UNKNOWN_MESSAGE_TYPE;
		break;
	}

	return ret;
}
