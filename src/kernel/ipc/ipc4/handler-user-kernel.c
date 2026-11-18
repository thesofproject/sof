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

#include "../audio/copier/ipcgtw_copier.h"

#include "handler-route.h"

/*
 * Kernel part of the IPC4 global and module message handling.
 *
 * handler-kernel.c receives the host IPC in the kernel IPC thread and passes
 * the global messages to ipc4_user_process_glb_message() and the module
 * messages to ipc4_user_process_module_message() here. These routers, also
 * running in the kernel IPC thread, are the boundary between kernel and user
 * space for IPC4:
 * - some messages are handled here, in kernel mode, in all builds: CHAIN_DMA,
 *   the IPC gateway, GDB, and LARGE_CONFIG get and set of the base firmware
 *   (module_id 0), which reads the IMR manifest;
 * - the messages that create, configure, connect and delete pipelines and
 *   modules are passed to the ipc4_route_*() functions. With the LL scheduler
 *   in user space (CONFIG_SOF_USERSPACE_LL) these are in handler-ll-user.c and
 *   forward the message to the user-space IPC thread, which runs the handlers
 *   in src/ipc/ipc4/handler-user.c. Otherwise they are in handler-ll-kernel.c
 *   and run those handlers directly in the kernel IPC thread.
 * The "user" in the name of this file and of the routers refers to the
 * messages, which are handled by the user-space part where possible.
 */

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

/* Userspace message context, copied in/out by kernel IPC thread. */
/* fw sends a fw ipc message to send the status of the last host ipc message */
static struct ipc_msg *msg_reply;

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
			sof_heap_free(sof_sys_user_heap_get(), cdma_comp);
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

	err = copier_ipcgtw_process((const struct ipc4_ipcgtw_cmd *)ipc4, ipc_ks(ipc)->comp_data,
				    &reply_size);
	/* reply size is returned in header extension dword */
	msg_reply->extension = reply_size;

	if (reply_size > 0) {
		msg_reply->tx_data = ipc_ks(ipc)->comp_data;
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
		ret = ipc4_route_create_pipeline(ipc4);
		break;
	case SOF_IPC4_GLB_DELETE_PIPELINE:
		ret = ipc4_route_delete_pipeline(ipc4);
		break;
	case SOF_IPC4_GLB_SET_PIPELINE_STATE:
		ret = ipc4_route_set_pipeline_state(ipc4);
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

__cold int ipc4_init_module_instance(struct ipc4_message_request *ipc4)
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


__cold int ipc4_get_large_config_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_large_config_reply reply;
	const struct ipc4_module_large_config *config =
		(const struct ipc4_module_large_config *)ipc4;
	char *data = ipc_ks(ipc_get())->comp_data;
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


__cold int ipc4_set_large_config_module_instance(struct ipc4_message_request *ipc4)
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
		const char *data = (const char *)ipc_ks(ipc)->comp_data + sizeof(*config);
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


__cold int ipc4_user_process_module_message(struct ipc4_message_request *ipc4,
					    struct ipc_msg *reply)
{
	uint32_t type;
	int ret;

	assert_can_be_cold();

	type = ipc4->primary.r.type;
	msg_reply = reply;

	switch (type) {
	case SOF_IPC4_MOD_INIT_INSTANCE:
		ret = ipc4_route_init_module(ipc4);
		break;
	case SOF_IPC4_MOD_CONFIG_GET:
		ret = ipc4_route_module_config(ipc4, false, msg_reply);
		break;
	case SOF_IPC4_MOD_CONFIG_SET:
		ret = ipc4_route_module_config(ipc4, true, msg_reply);
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_GET:
		ret = ipc4_route_large_config_get(ipc4, msg_reply);
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_SET:
		ret = ipc4_route_large_config_set(ipc4);
		break;
	case SOF_IPC4_MOD_BIND:
		ret = ipc4_route_bind(ipc4);
		break;
	case SOF_IPC4_MOD_UNBIND:
		ret = ipc4_route_unbind(ipc4);
		break;
	case SOF_IPC4_MOD_DELETE_INSTANCE:
		ret = ipc4_route_delete_module(ipc4);
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

