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
#include <sof/lib/pm_runtime.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <ipc4/error_status.h>
#include <ipc/header.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#include <ipc4/notification.h>
#include <ipc/trace.h>
#include <user/trace.h>

#include <rtos/kernel.h>
#include <sof/trace/dma-trace.h>
#include <sof/lib_manager.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

struct ipc4_msg_data {
	struct ipc_cmd_hdr msg_in; /* local copy of current message from host header */
	struct ipc_cmd_hdr msg_out; /* local copy of current message to host header */
	int delayed_reply;
	uint32_t delayed_error;
};

struct ipc4_msg_data msg_data;

/* fw sends a fw ipc message to send the status of the last host ipc message */
struct ipc_msg msg_reply;

#ifdef CONFIG_LOG_BACKEND_ADSP_MTRACE
static struct ipc_msg msg_notify;
#endif

/*
 * Global IPC Operations.
 */
static int ipc4_create_pipeline(struct ipc4_message_request *ipc4)
{
	struct ipc *ipc = ipc_get();

	return ipc_pipeline_new(ipc, (ipc_pipe_new *)ipc4);
}

static int ipc4_delete_pipeline(struct ipc4_message_request *ipc4)
{
	struct ipc4_pipeline_delete *pipe;
	struct ipc *ipc = ipc_get();

	pipe = (struct ipc4_pipeline_delete *)ipc4;
	tr_dbg(&ipc_tr, "ipc4 delete pipeline %x:", (uint32_t)pipe->primary.r.instance_id);

	return ipc_pipeline_free(ipc, pipe->primary.r.instance_id);
}

static int ipc4_comp_params(struct comp_dev *current,
			    struct comp_buffer *calling_buf,
			    struct pipeline_walk_context *ctx, int dir)
{
	struct pipeline_data *ppl_data = ctx->comp_data;
	int err;

	/* don't do any params if current is running */
	if (current->state == COMP_STATE_ACTIVE)
		return 0;

	if (current->pipeline != ((struct pipeline_data *)ctx->comp_data)->p)
		return 0;

	err = comp_params(current, &ppl_data->params->params);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	return pipeline_for_each_comp(current, ctx, dir);
}

static int ipc4_pipeline_params(struct pipeline *p, struct comp_dev *host,
				struct sof_ipc_pcm_params *params)
{
	struct sof_ipc_pcm_params hw_params;
	struct pipeline_data data = {
		.start = host,
		.params = &hw_params,
		.p = p,
	};

	struct pipeline_walk_context param_ctx = {
		.comp_func = ipc4_comp_params,
		.comp_data = &data,
		.skip_incomplete = true,
	};

	return param_ctx.comp_func(host, NULL, &param_ctx, host->direction);
}

static int ipc4_pcm_params(struct ipc_comp_dev *pcm_dev)
{
	struct sof_ipc_pcm_params params;
	int err, reset_err;

	memset(&params, 0, sizeof(params));

	/* sanity check comp */
	if (!pcm_dev->cd->pipeline) {
		tr_err(&ipc_tr, "ipc: comp %d pipeline not found", pcm_dev->id);
		return -EINVAL;
	}

	/* configure pipeline audio params */
	err = ipc4_pipeline_params(pcm_dev->cd->pipeline, pcm_dev->cd, &params);
	if (err < 0) {
		tr_err(&ipc_tr, "ipc: pipe %d comp %d params failed %d",
		       pcm_dev->cd->pipeline->pipeline_id,
		       pcm_dev->cd->pipeline->comp_id, err);
		goto error;
	}

	/* prepare pipeline audio params */
	err = pipeline_prepare(pcm_dev->cd->pipeline, pcm_dev->cd);
	if (err < 0) {
		tr_err(&ipc_tr, "ipc: pipe %d comp %d prepare failed %d",
		       pcm_dev->cd->pipeline->pipeline_id,
		       pcm_dev->cd->pipeline->comp_id, err);
		goto error;
	}

	return 0;

error:
	reset_err = pipeline_reset(pcm_dev->cd->pipeline, pcm_dev->cd);
	if (reset_err < 0)
		tr_err(&ipc_tr, "ipc: pipe %d comp %d reset failed %d",
		       pcm_dev->cd->pipeline->pipeline_id,
		       pcm_dev->cd->pipeline->comp_id, reset_err);

	return err;
}

static bool is_any_ppl_active(void)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;

	list_for_item(clist, &ipc_get()->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_PIPELINE)
			continue;

		if (icd->pipeline->status == COMP_STATE_ACTIVE)
			return true;
	}

	return false;
}

/* Ipc4 pipeline message <------> ipc3 pipeline message
 * RUNNING     <-------> TRIGGER START
 * INIT + PAUSED  <-------> PIPELINE COMPLETE
 * INIT + RESET <-------> PIPELINE COMPLETE
 * PAUSED      <-------> TRIGER_PAUSE
 * RESET       <-------> TRIGER_STOP + RESET
 * EOS(end of stream) <-------> NOT SUPPORT NOW
 *
 *   IPC4 pipeline state machine
 *
 *                      INIT
 *                       |    \
 *                       |   __\|
 *                       |
 *                       |     RESET
 *                       |     _   _
 *                       |     /| |\
 *                       |    /    /\
 *                      \|/ |/_   /  \
 *        RUNNING <--> PAUSE _   /    \
 *            /  \      /|\ |\  /      \
 *           /    \      |    \/        \
 *          /      \     |    /\         \
 *         /        \    |   /  \         \
 *       |/_        _\|  |  /    \        _\|
 *     ERROR Stop       EOS       |______\ SAVE
 *                                      /
 */
static int set_pipeline_state(uint32_t id, uint32_t cmd, bool *delayed, uint32_t *ppl_status)
{
	struct ipc_comp_dev *pcm_dev;
	struct ipc_comp_dev *host = NULL;
	struct ipc *ipc = ipc_get();
	int status;
	int ret;

	tr_dbg(&ipc_tr, "ipc4 set pipeline %d state %x:", id, cmd);

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, id);
	if (!pcm_dev) {
		tr_err(&ipc_tr, "ipc: comp %d not found", id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	status = pcm_dev->pipeline->status;
	*ppl_status = status;
	/* source & sink components are set when pipeline is set to COMP_STATE_INIT */
	if (status != COMP_STATE_INIT) {
		int host_id;

		if (pcm_dev->pipeline->source_comp->direction == SOF_IPC_STREAM_PLAYBACK)
			host_id = pcm_dev->pipeline->source_comp->ipc_config.id;
		else
			host_id = pcm_dev->pipeline->sink_comp->ipc_config.id;

		host = ipc_get_comp_by_id(ipc, host_id);
		if (!host) {
			tr_err(&ipc_tr, "ipc: comp host with ID %d not found", host_id);
			return IPC4_INVALID_RESOURCE_ID;
		}

		/* check core */
		if (!cpu_is_me(host->core))
			return ipc_process_on_core(host->core, false);
	}

	switch (cmd) {
	case SOF_IPC4_PIPELINE_STATE_RUNNING:
		if (status != COMP_STATE_PAUSED && status != COMP_STATE_READY) {
			tr_err(&ipc_tr, "ipc: current status %d", status);
			return IPC4_INVALID_REQUEST;
		}

		/* init params when pipeline is complete or reset */
		if (status == COMP_STATE_READY) {
			cmd = COMP_TRIGGER_PRE_START;

			ret = ipc4_pcm_params(host);
			if (ret < 0)
				return IPC4_INVALID_REQUEST;
		} else {
			cmd = COMP_TRIGGER_PRE_RELEASE;
		}

		break;
	case SOF_IPC4_PIPELINE_STATE_RESET:
		if (status == COMP_STATE_INIT) {
			ret = ipc_pipeline_complete(ipc, id);
			if (ret < 0)
				ret = IPC4_INVALID_REQUEST;

			*ppl_status = COMP_STATE_READY;
			return ret;
		}

		/* initialized -> pause -> reset */
		if (status == COMP_STATE_READY)
			return 0;

		if (status == COMP_STATE_ACTIVE || status == COMP_STATE_PAUSED) {
			ret = pipeline_trigger(host->cd->pipeline, host->cd, COMP_TRIGGER_STOP);
			if (ret < 0) {
				tr_err(&ipc_tr, "ipc: comp %d trigger 0x%x failed %d",
				       id, cmd, ret);
				return IPC4_PIPELINE_STATE_NOT_SET;
			} else if (ret == PPL_STATUS_SCHEDULED) {
				*delayed = true;
			}
		}

		/* resource is not released by triggering reset which is used by current FW */
		ret = pipeline_reset(host->cd->pipeline, host->cd);
		if (ret < 0)
			ret = IPC4_INVALID_REQUEST;

		return ret;
	case SOF_IPC4_PIPELINE_STATE_PAUSED:
		if (status == COMP_STATE_INIT) {
			ret = ipc_pipeline_complete(ipc, id);
			if (ret < 0)
				ret = IPC4_INVALID_REQUEST;

			*ppl_status = COMP_STATE_READY;
			return ret;
		}

		if (status == COMP_STATE_READY)
			return 0;

		cmd = COMP_TRIGGER_PAUSE;
		break;
	/* special case- TODO */
	case SOF_IPC4_PIPELINE_STATE_EOS:
		if (status != COMP_STATE_ACTIVE)
			return IPC4_INVALID_REQUEST;
	case SOF_IPC4_PIPELINE_STATE_SAVED:
	case SOF_IPC4_PIPELINE_STATE_ERROR_STOP:
	default:
		tr_err(&ipc_tr, "ipc: unsupported trigger cmd 0x%x", cmd);
		return IPC4_INVALID_REQUEST;
	}

	/* trigger the component */
	ret = pipeline_trigger(host->cd->pipeline, host->cd, cmd);
	if (ret < 0) {
		tr_err(&ipc_tr, "ipc: comp %d trigger 0x%x failed %d", id, cmd, ret);
		ret = IPC4_PIPELINE_STATE_NOT_SET;
	} else if (ret == PPL_STATUS_SCHEDULED) {
		*delayed = true;
		ret = 0;
	}

	*ppl_status = host->cd->pipeline->status;
	return ret;
}

static void ipc_compound_pre_start(int msg_id)
{
	/* ipc thread will wait for all scheduled ipc messages to be complete
	 * Use a reference count to check status of these ipc messages.
	 */
	msg_data.delayed_reply++;
}

static void ipc_compound_post_start(uint32_t msg_id, int ret, bool delayed)
{
	if (ret) {
		tr_err(&ipc_tr, "failed to process msg %d status %d", msg_id, ret);
		msg_data.delayed_reply--;
		return;
	}

	/* decrease counter if it is not scheduled by another thread */
	if (!delayed)
		msg_data.delayed_reply--;
}

static void ipc_compound_msg_done(uint32_t msg_id, int error)
{
	if (!msg_data.delayed_reply) {
		tr_err(&ipc_tr, "unexpected delayed reply");
		return;
	}

	msg_data.delayed_reply--;

	/* error reported in delayed pipeline task */
	if (error < 0) {
		if (msg_id == SOF_IPC4_GLB_SET_PIPELINE_STATE)
			msg_data.delayed_error = IPC4_PIPELINE_STATE_NOT_SET;
	}
}

static int ipc_wait_for_compound_msg(void)
{
	int try_count = 30;
	int ret = 0;

	while (msg_data.delayed_reply) {
		k_sleep(Z_TIMEOUT_US(250));

		if (!try_count--) {
			ret = IPC4_FAILURE;
			tr_err(&ipc_tr, "failed to wait schedule thread");
			break;
		}
	}

	return ret;
}

/* In ipc3 path, host driver sends pcm_hw_param message to fw and
 * direction is included. The direction is set to each component
 * after pipeline is complete. In ipc4 path, direction is figured out and
 * set it to each component after connected pipeline are complete.
 */
static int update_dir_to_pipeline_component(uint32_t *ppl_id, uint32_t count)
{
	struct ipc_comp_dev *icd;
	struct ipc_comp_dev *pipe;
	struct comp_dev *dir_src = NULL;
	struct list_item *clist;
	struct ipc *ipc;
	uint32_t i;

	ipc = ipc_get();

	for (i = 0; i < count; i++) {
		pipe = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, ppl_id[i]);
		if (!pipe) {
			tr_info(&ipc_tr, "ppl_id %u: no pipeline is found", ppl_id[i]);
			continue;
		}

		if (pipe->pipeline->source_comp->direction_set) {
			dir_src = pipe->pipeline->source_comp;
			break;
		} else if (pipe->pipeline->sink_comp->direction_set) {
			dir_src = pipe->pipeline->sink_comp;
			break;
		}
	}
	if (!dir_src) {
		tr_err(&ipc_tr, "no direction source in pipeline");
		return IPC4_INVALID_RESOURCE_STATE;
	}

	/* set direction to the component in the pipeline array */
	list_for_item(clist, &ipc->comp_list) {
		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		for (i = 0; i < count; i++) {
			if (ipc_comp_pipe_id(icd) == ppl_id[i]) {
				struct comp_dev *dev = icd->cd;

				/* don't update direction for host & dai since they
				 * have direction. Especially in dai copier to dai copier
				 * case the direction can't be modified to single value
				 * since one of them is for playback and the other one
				 * is for capture
				 */
				if (dev->direction_set)
					break;

				icd->cd->direction = dir_src->direction;
				break;
			}
		}
	}

	return 0;
}

static int ipc4_set_pipeline_state(struct ipc4_message_request *ipc4)
{
	struct ipc4_pipeline_set_state_data *ppl_data;
	struct ipc4_pipeline_set_state state;
	uint32_t status = COMP_STATE_INIT;
	uint32_t cmd, ppl_count;
	uint32_t *ppl_id, id;
	int ret = 0;
	int i;

	state.primary.dat = ipc4->primary.dat;
	state.extension.dat = ipc4->extension.dat;
	cmd = state.primary.r.ppl_state;

	ppl_data = (struct ipc4_pipeline_set_state_data *)MAILBOX_HOSTBOX_BASE;
	dcache_invalidate_region((__sparse_force void __sparse_cache *)ppl_data, sizeof(*ppl_data));
	if (state.extension.r.multi_ppl) {
		ppl_count = ppl_data->pipelines_count;
		ppl_id = ppl_data->ppl_id;
		dcache_invalidate_region((__sparse_force void __sparse_cache *)ppl_id,
					 sizeof(int) * ppl_count);
	} else {
		ppl_count = 1;
		id = state.primary.r.ppl_id;
		ppl_id = &id;
	}

	for (i = 0; i < ppl_count; i++) {
		bool delayed = false;

		ipc_compound_pre_start(state.primary.r.type);
		ret = set_pipeline_state(ppl_id[i], cmd, &delayed, &status);
		ipc_compound_post_start(state.primary.r.type, ret, delayed);

		if (ret != 0)
			return ret;
	}

	/* update direction after all connected pipelines are complete */
	if (status == COMP_STATE_READY)
		ret = update_dir_to_pipeline_component(ppl_id, ppl_count);

	return ret;
}

#if CONFIG_LIBRARY_MANAGER
static int ipc4_load_library(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_load_library library;
	int ret = IPC4_UNAVAILABLE;

	library.header.dat = ipc4->primary.dat;
	library.data.dat = ipc4->extension.dat;

	ret = lib_manager_load_library(library.header.r.dma_id, library.header.r.lib_id);
	tr_info(&ipc_tr, "ipc: ipc4_load_library %d", ret);

	return ret;
}
#endif

static int ipc4_process_chain_dma(struct ipc4_message_request *ipc4)
{
	struct ipc4_chain_dma cdma;
	struct ipc *ipc = ipc_get();
	int ret;

	memcpy_s(&cdma, sizeof(cdma), ipc4, sizeof(cdma));

	if (cdma.primary.r.allocate && cdma.extension.r.fifo_size) {
		ret = ipc4_create_chain_dma(ipc, &cdma);
		if (ret)
			tr_err(&ipc_tr, "failed to create chain dma %d", ret);

		return ret;
	}

	msg_data.delayed_reply = 1;
	ret = ipc4_trigger_chain_dma(ipc, &cdma);
	/* it is not scheduled in another thread */
	if (ret != PPL_STATUS_SCHEDULED) {
		msg_data.delayed_reply = 0;
		msg_data.delayed_error = 0;
	} else {
		ret = 0;
	}

	return ret;
}

static int ipc4_process_glb_message(struct ipc4_message_request *ipc4)
{
	uint32_t type;
	int ret;

	type = ipc4->primary.r.type;

	switch (type) {
	case SOF_IPC4_GLB_BOOT_CONFIG:
	case SOF_IPC4_GLB_ROM_CONTROL:
	case SOF_IPC4_GLB_IPCGATEWAY_CMD:
	case SOF_IPC4_GLB_PERF_MEASUREMENTS_CMD:
	case SOF_IPC4_GLB_LOAD_MULTIPLE_MODULES:
	case SOF_IPC4_GLB_UNLOAD_MULTIPLE_MODULES:
		tr_err(&ipc_tr, "not implemented ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;

	case SOF_IPC4_GLB_CHAIN_DMA:
		ret = ipc4_process_chain_dma(ipc4);
		break;

	/* pipeline settings */
	case SOF_IPC4_GLB_CREATE_PIPELINE:
		ret = ipc4_create_pipeline(ipc4);
		break;
	case SOF_IPC4_GLB_DELETE_PIPELINE:
		ret = ipc4_delete_pipeline(ipc4);
		break;
	case SOF_IPC4_GLB_SET_PIPELINE_STATE:
		ret = ipc4_set_pipeline_state(ipc4);
		break;

	case SOF_IPC4_GLB_GET_PIPELINE_STATE:
	case SOF_IPC4_GLB_GET_PIPELINE_CONTEXT_SIZE:
	case SOF_IPC4_GLB_SAVE_PIPELINE:
	case SOF_IPC4_GLB_RESTORE_PIPELINE:
		tr_err(&ipc_tr, "not implemented ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;

	/* Loads library (using Code Load or HD/A Host Output DMA) */
#ifdef CONFIG_LIBRARY_MANAGER
	case SOF_IPC4_GLB_LOAD_LIBRARY:
		ret = ipc4_load_library(ipc4);
		break;
#endif
	case SOF_IPC4_GLB_INTERNAL_MESSAGE:
		tr_err(&ipc_tr, "not implemented ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;

	/* Notification (FW to SW driver) */
	case SOF_IPC4_GLB_NOTIFICATION:
		tr_err(&ipc_tr, "not implemented ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;

	default:
		tr_err(&ipc_tr, "unsupported ipc message type %d", type);
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

static int ipc4_init_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_init_instance module;
	struct sof_ipc_comp comp;
	struct comp_dev *dev;

	memcpy_s(&module, sizeof(module), ipc4, sizeof(module));
	tr_dbg(&ipc_tr, "ipc4_init_module_instance %x : %x", (uint32_t)module.primary.r.module_id,
	       (uint32_t)module.primary.r.instance_id);

	memset(&comp, 0, sizeof(comp));
	comp.id = IPC4_COMP_ID(module.primary.r.module_id, module.primary.r.instance_id);
	comp.pipeline_id = module.extension.r.ppl_instance_id;
	comp.core = module.extension.r.core_id;
	comp.ext_data_length = module.extension.r.param_block_size;
	dev = comp_new(&comp);
	if (!dev) {
		tr_err(&ipc_tr, "error: failed to init module %x : %x",
		       (uint32_t)module.primary.r.module_id,
		       (uint32_t)module.primary.r.instance_id);
		return IPC4_MOD_NOT_INITIALIZED;
	}

	return 0;
}

static int ipc4_bind_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_bind_unbind bu;
	struct ipc *ipc = ipc_get();

	memcpy_s(&bu, sizeof(bu), ipc4, sizeof(bu));
	tr_dbg(&ipc_tr, "ipc4_bind_module_instance %x : %x with %x : %x",
	       (uint32_t)bu.primary.r.module_id, (uint32_t)bu.primary.r.instance_id,
	       (uint32_t)bu.extension.r.dst_module_id, (uint32_t)bu.extension.r.dst_instance_id);

	return ipc_comp_connect(ipc, (ipc_pipe_comp_connect *)&bu);
}

static int ipc4_unbind_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_bind_unbind bu;
	struct ipc *ipc = ipc_get();

	memcpy_s(&bu, sizeof(bu), ipc4, sizeof(bu));
	tr_dbg(&ipc_tr, "ipc4_unbind_module_instance %x : %x with %x : %x",
	       (uint32_t)bu.primary.r.module_id, (uint32_t)bu.primary.r.instance_id,
	       (uint32_t)bu.extension.r.dst_module_id, (uint32_t)bu.extension.r.dst_instance_id);

	return ipc_comp_disconnect(ipc, (ipc_pipe_comp_connect *)&bu);
}

static int ipc4_get_large_config_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_large_config_reply reply;
	struct ipc4_module_large_config config;
	char *data = ipc_get()->comp_data;
	const struct comp_driver *drv;
	struct comp_dev *dev = NULL;
	uint32_t data_offset;
	void *response_buffer;
	int ret;

	memcpy_s(&config, sizeof(config), ipc4, sizeof(config));
	tr_dbg(&ipc_tr, "ipc4_get_large_config_module_instance %x : %x",
	       (uint32_t)config.primary.r.module_id, (uint32_t)config.primary.r.instance_id);

	drv = ipc4_get_comp_drv(config.primary.r.module_id);
	if (!drv)
		return IPC4_MOD_INVALID_ID;

	if (!drv->ops.get_large_config)
		return IPC4_INVALID_REQUEST;

	/* get component dev for non-basefw since there is no
	 * component dev for basefw
	 */
	if (config.primary.r.module_id) {
		uint32_t comp_id;

		comp_id = IPC4_COMP_ID(config.primary.r.module_id, config.primary.r.instance_id);
		dev = ipc4_get_comp_dev(comp_id);
		if (!dev)
			return IPC4_MOD_INVALID_ID;
	}

	data_offset =  config.extension.r.data_off_size;
	ret = drv->ops.get_large_config(dev, config.extension.r.large_param_id,
					config.extension.r.init_block,
					config.extension.r.final_block,
					&data_offset, data);

	/* set up ipc4 error code for reply data */
	if (ret < 0)
		ret = IPC4_MOD_INVALID_ID;

	/* Copy host config and overwrite */
	reply.extension.dat = config.extension.dat;
	reply.extension.r.data_off_size = data_offset;

	/* The last block, no more data */
	if (!config.extension.r.final_block && data_offset < SOF_IPC_MSG_MAX_SIZE)
		reply.extension.r.final_block = 1;

	/* Indicate last block if error occurs */
	if (ret)
		reply.extension.r.final_block = 1;

	/* no need to allocate memory for reply msg */
	if (ret)
		return ret;

	msg_reply.extension = reply.extension.dat;
	response_buffer = rballoc(0, SOF_MEM_CAPS_RAM, data_offset);
	if (response_buffer) {
		msg_reply.tx_size = data_offset;
		msg_reply.tx_data = response_buffer;
	} else {
		tr_err(&ipc_tr, "error: failed to allocate tx_data");
		ret = IPC4_OUT_OF_MEMORY;
	}

	return ret;
}

static int ipc4_set_large_config_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_large_config config;
	struct comp_dev *dev = NULL;
	const struct comp_driver *drv;
	int ret;

	memcpy_s(&config, sizeof(config), ipc4, sizeof(config));
	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE, config.extension.r.data_off_size);
	tr_dbg(&ipc_tr, "ipc4_set_large_config_module_instance %x : %x",
	       (uint32_t)config.primary.r.module_id, (uint32_t)config.primary.r.instance_id);

	drv = ipc4_get_comp_drv(config.primary.r.module_id);
	if (!drv)
		return IPC4_MOD_INVALID_ID;

	if (!drv->ops.set_large_config)
		return IPC4_INVALID_REQUEST;

	if (config.primary.r.module_id) {
		uint32_t comp_id;

		comp_id = IPC4_COMP_ID(config.primary.r.module_id, config.primary.r.instance_id);
		dev = ipc4_get_comp_dev(comp_id);
		if (!dev)
			return IPC4_MOD_INVALID_ID;
	}

	ret = drv->ops.set_large_config(dev, config.extension.r.large_param_id,
					config.extension.r.init_block,
					config.extension.r.final_block,
					config.extension.r.data_off_size,
					(char *)MAILBOX_HOSTBOX_BASE);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to set large_config_module_instance %x : %x",
		       (uint32_t)config.primary.r.module_id,
		       (uint32_t)config.primary.r.instance_id);
		ret = IPC4_INVALID_RESOURCE_ID;
	}

	return ret;
}

static int ipc4_delete_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_delete_instance module;
	struct ipc *ipc = ipc_get();
	uint32_t comp_id;
	int ret;

	memcpy_s(&module, sizeof(module), ipc4, sizeof(module));
	tr_dbg(&ipc_tr, "ipc4_delete_module_instance %x : %x", (uint32_t)module.primary.r.module_id,
	       (uint32_t)module.primary.r.instance_id);

	comp_id = IPC4_COMP_ID(module.primary.r.module_id, module.primary.r.instance_id);
	ret = ipc_comp_free(ipc, comp_id);
	if (ret < 0) {
		tr_err(&ipc_tr, "failed to delete module instance %x : %x",
		       (uint32_t)module.primary.r.module_id,
		       (uint32_t)module.primary.r.instance_id);
		ret = IPC4_INVALID_RESOURCE_ID;
	}

	return ret;
}

/* disable power gating on core 0 */
static int ipc4_module_process_d0ix(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_set_d0ix d0ix;
	uint32_t module_id, instance_id;

	memcpy_s(&d0ix, sizeof(d0ix), ipc4, sizeof(d0ix));
	module_id = d0ix.primary.r.module_id;
	instance_id = d0ix.primary.r.instance_id;

	tr_dbg(&ipc_tr, "ipc4_module_process_d0ix %x : %x", module_id, instance_id);

	if (d0ix.primary.r.module_id || d0ix.primary.r.instance_id) {
		tr_err(&ipc_tr, "invalid resource id %x : %x", module_id, instance_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	/* only module 0 can be used to set d0ix state */
	if (d0ix.extension.r.prevent_power_gating)
		pm_runtime_get(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);
	else
		pm_runtime_put(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

	return 0;
}

/* enable/disable cores according to the state mask */
static int ipc4_module_process_dx(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_set_dx dx;
	struct ipc4_dx_state_info dx_info;
	uint32_t module_id, instance_id;
	uint32_t core_id;
	int ret;

	memcpy_s(&dx, sizeof(dx), ipc4, sizeof(dx));
	module_id = dx.primary.r.module_id;
	instance_id = dx.primary.r.instance_id;

	/* only module 0 can be used to set dx state */
	if (module_id || instance_id) {
		tr_err(&ipc_tr, "invalid resource id %x : %x", module_id, instance_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE, sizeof(dx_info));
	memcpy_s(&dx_info, sizeof(dx_info), (const void *)MAILBOX_HOSTBOX_BASE, sizeof(dx_info));

	/* check if core enable mask is valid */
	if (dx_info.core_mask > MASK(CONFIG_CORE_COUNT - 1, 0)) {
		tr_err(&ipc_tr, "ipc4_module_process_dx: CONFIG_CORE_COUNT: %d < core enable mask: %d",
		       CONFIG_CORE_COUNT, dx_info.core_mask);
		return IPC4_ERROR_INVALID_PARAM;
	}

	/* check primary core first */
	if ((dx_info.core_mask & BIT(PLATFORM_PRIMARY_CORE_ID)) &&
	    (dx_info.dx_mask & BIT(PLATFORM_PRIMARY_CORE_ID))) {
		/* core0 can't be activated more, it's already active since we got here */
		tr_err(&ipc_tr, "Core0 is already active");
		return IPC4_BAD_STATE;
	}

	/* Activate/deactivate requested cores */
	for (core_id = 1; core_id < CONFIG_CORE_COUNT; core_id++) {
		if ((dx_info.core_mask & BIT(core_id)) == 0)
			continue;

		if (dx_info.dx_mask & BIT(core_id)) {
			ret = cpu_enable_core(core_id);
			if (ret != 0) {
				tr_err(&ipc_tr, "failed to enable core %d", core_id);
				return IPC4_FAILURE;
			}
		} else {
			cpu_disable_core(core_id);
			if (cpu_is_core_enabled(core_id)) {
				tr_err(&ipc_tr, "failed to disable core %d", core_id);
				return IPC4_FAILURE;
			}
		}
	}

	/* Deactivating primary core if requested.  */
	if (dx_info.core_mask & BIT(PLATFORM_PRIMARY_CORE_ID)) {
		if (cpu_enabled_cores() & ~BIT(PLATFORM_PRIMARY_CORE_ID)) {
			tr_err(&ipc_tr, "secondary cores 0x%x still active",
			       cpu_enabled_cores());
			return IPC4_BUSY;
		}

		if (is_any_ppl_active()) {
			tr_err(&ipc_tr, "some pipelines are still active");
			return IPC4_BUSY;
		}

		/* do platform specific suspending */
		platform_context_save(sof_get());

		ipc_get()->pm_prepare_D3 = 1;
	}

	return IPC4_SUCCESS;
}

static int ipc4_process_module_message(struct ipc4_message_request *ipc4)
{
	uint32_t type;
	int ret;

	type = ipc4->primary.r.type;

	switch (type) {
	case SOF_IPC4_MOD_INIT_INSTANCE:
		ret = ipc4_init_module_instance(ipc4);
		break;
	case SOF_IPC4_MOD_CONFIG_GET:
	case SOF_IPC4_MOD_CONFIG_SET:
		ret = IPC4_UNAVAILABLE;
		tr_info(&ipc_tr, "unsupported module CONFIG_GET");
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_GET:
		ret = ipc4_get_large_config_module_instance(ipc4);
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_SET:
		ret = ipc4_set_large_config_module_instance(ipc4);
		break;
	case SOF_IPC4_MOD_BIND:
		ret = ipc4_bind_module_instance(ipc4);
		break;
	case SOF_IPC4_MOD_UNBIND:
		ret = ipc4_unbind_module_instance(ipc4);
		break;
	case SOF_IPC4_MOD_DELETE_INSTANCE:
		ret = ipc4_delete_module_instance(ipc4);
		break;
	case SOF_IPC4_MOD_SET_D0IX:
		ret = ipc4_module_process_d0ix(ipc4);
		break;
	case SOF_IPC4_MOD_SET_DX:
		ret = ipc4_module_process_dx(ipc4);
		break;
	case SOF_IPC4_MOD_ENTER_MODULE_RESTORE:
	case SOF_IPC4_MOD_EXIT_MODULE_RESTORE:
		ret = IPC4_UNAVAILABLE;
		break;
	default:
		ret = IPC4_UNAVAILABLE;
		break;
	}

	return ret;
}

struct ipc_cmd_hdr *mailbox_validate(void)
{
	struct ipc_cmd_hdr *hdr = ipc_get()->comp_data;
	return hdr;
}

struct ipc_cmd_hdr *ipc_compact_read_msg(void)
{
	int words;

	words = ipc_platform_compact_read_msg(&msg_data.msg_in, 2);
	if (!words)
		return mailbox_validate();

	return &msg_data.msg_in;
}

struct ipc_cmd_hdr *ipc_prepare_to_send(const struct ipc_msg *msg)
{
	msg_data.msg_out.pri = msg->header;
	msg_data.msg_out.ext = msg->extension;

	if (msg->tx_size)
		mailbox_dspbox_write(0, (uint32_t *)msg->tx_data, msg->tx_size);

	/* free memory for get config function */
	if (msg == &msg_reply && msg_reply.tx_size > 0)
		rfree(msg_reply.tx_data);

	return &msg_data.msg_out;
}

void ipc_boot_complete_msg(struct ipc_cmd_hdr *header, uint32_t data)
{
	header->pri = SOF_IPC4_FW_READY;
	header->ext = 0;
}

#ifdef CONFIG_LOG_BACKEND_ADSP_MTRACE

static bool is_notification_queued(void)
{
	struct ipc *ipc = ipc_get();
	struct list_item *slist;
	struct ipc_msg *msg;
	k_spinlock_key_t key;
	bool queued = false;

	key = k_spin_lock(&ipc->lock);
	if (list_is_empty(&ipc->msg_list))
		goto out;

	list_for_item(slist, &ipc->msg_list) {
		msg = container_of(slist, struct ipc_msg, list);

		if (msg == &msg_notify) {
			queued = true;
			break;
		}
	}

out:
	k_spin_unlock(&ipc->lock, key);

	return queued;
}

void ipc_send_buffer_status_notify(void)
{
	/* a single msg_notify object is used */
	if (is_notification_queued())
		return;

	msg_notify.header = SOF_IPC4_NOTIF_HEADER(SOF_IPC4_NOTIFY_LOG_BUFFER_STATUS);
	msg_notify.extension = 0;
	msg_notify.tx_size = 0;
	list_init(&msg_notify.list);

	tr_dbg(&ipc_tr, "tx-notify\t: %#x|%#x", msg_notify.header, msg_notify.extension);

	ipc_msg_send(&msg_notify, NULL, true);
}
#endif

void ipc_msg_reply(struct sof_ipc_reply *reply)
{
	struct ipc4_message_request in;

	in.primary.dat = msg_data.msg_in.pri;
	ipc_compound_msg_done(in.primary.r.type, reply->error);
}

void ipc_cmd(struct ipc_cmd_hdr *_hdr)
{
	struct ipc4_message_request *in = ipc_from_hdr(_hdr);
	enum ipc4_message_target target;
	int err;

	if (!in)
		return;

	tr_dbg(&ipc_tr, "rx\t: %#x|%#x", in->primary.dat, in->extension.dat);

	/* no process on scheduled thread */
	msg_data.delayed_reply = 0;
	msg_data.delayed_error = 0;
	msg_reply.tx_size = 0;
	msg_reply.header = in->primary.dat;
	msg_reply.extension = in->extension.dat;
	list_init(&msg_reply.list);

	target = in->primary.r.msg_tgt;

	switch (target) {
	case SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG:
		err = ipc4_process_glb_message(in);
		break;
	case SOF_IPC4_MESSAGE_TARGET_MODULE_MSG:
		err = ipc4_process_module_message(in);
		break;
	default:
		/* should not reach here as we only have 2 message types */
		tr_err(&ipc_tr, "ipc4: invalid target %d", target);
		err = IPC4_UNKNOWN_MESSAGE_TYPE;
	}

	if (err)
		tr_err(&ipc_tr, "ipc4: %d failed err %d", target, err);

	/* FW sends an ipc message to host if request bit is clear */
	if (in->primary.r.rsp == SOF_IPC4_MESSAGE_DIR_MSG_REQUEST) {
		struct ipc *ipc = ipc_get();
		char *data = ipc->comp_data;
		struct ipc4_message_reply reply;

		/* Do not send reply for SET_DX if we are going to enter D3
		 * The reply is going to be sent as part of the power down
		 * sequence
		 */
		if (ipc->task_mask & IPC_TASK_POWERDOWN)
			return;

		if (ipc_wait_for_compound_msg() != 0) {
			tr_err(&ipc_tr, "ipc4: failed to send delayed reply");
			err = IPC4_FAILURE;
		}

		/* copy contents of message received */
		reply.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REPLY;
		reply.primary.r.msg_tgt = in->primary.r.msg_tgt;
		reply.primary.r.type = in->primary.r.type;
		if (msg_data.delayed_error)
			reply.primary.r.status = msg_data.delayed_error;
		else
			reply.primary.r.status = err;

		msg_reply.header = reply.primary.dat;

		tr_dbg(&ipc_tr, "tx-reply\t: %#x|%#x", reply.primary.dat,
		       reply.extension.dat);

		ipc_msg_send(&msg_reply, data, true);
	}
}
