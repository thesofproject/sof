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
#include <sof/boot_test.h>
#include <sof/common.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/pm_runtime.h>
#include <sof/math/numbers.h>
#include <sof/tlv.h>
#include <sof/trace/trace.h>
#include <ipc4/error_status.h>
#include <ipc/header.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#include <ipc4/notification.h>
#include <ipc/trace.h>
#include <user/trace.h>

#include <rtos/atomic.h>
#include <rtos/kernel.h>
#include <sof/lib_manager.h>

#if CONFIG_SOF_BOOT_TEST
/* CONFIG_SOF_BOOT_TEST depends on Zephyr */
#include <zephyr/ztest.h>
#endif

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

LOG_MODULE_DECLARE(ipc, CONFIG_SOF_LOG_LEVEL);

struct ipc4_msg_data {
	struct ipc_cmd_hdr msg_in; /* local copy of current message from host header */
	struct ipc_cmd_hdr msg_out; /* local copy of current message to host header */
	atomic_t delayed_reply;
	uint32_t delayed_error;
};

static struct ipc4_msg_data msg_data;

/* fw sends a fw ipc message to send the status of the last host ipc message */
static struct ipc_msg msg_reply = {0, 0, 0, 0, false, LIST_INIT(msg_reply.list)};

static struct ipc_msg msg_notify = {0, 0, 0, 0, false, LIST_INIT(msg_notify.list)};

#if CONFIG_LIBRARY
static inline struct ipc4_message_request *ipc4_get_message_request(void)
{
	struct ipc *ipc = ipc_get();

	return (struct ipc4_message_request *)ipc->comp_data;
}

static inline void ipc4_send_reply(struct ipc4_message_reply *reply)
{
	struct ipc *ipc = ipc_get();

	/* copy the extension from the message reply */
	reply->extension.dat = msg_reply.extension;
	memcpy((char *)ipc->comp_data, reply, sizeof(*reply));
}

static inline const struct ipc4_pipeline_set_state_data *ipc4_get_pipeline_data(void)
{
	const struct ipc4_pipeline_set_state_data *ppl_data;
	struct ipc *ipc = ipc_get();

	ppl_data = (const struct ipc4_pipeline_set_state_data *)ipc->comp_data;

	return ppl_data;
}
#else
static inline struct ipc4_message_request *ipc4_get_message_request(void)
{
	/* ignoring _hdr as it does not contain valid data in IPC4/IDC case */
	return ipc_from_hdr(&msg_data.msg_in);
}

static inline void ipc4_send_reply(struct ipc4_message_reply *reply)
{
	struct ipc *ipc = ipc_get();
	char *data = ipc->comp_data;

	ipc_msg_send(&msg_reply, data, true);
}

static inline const struct ipc4_pipeline_set_state_data *ipc4_get_pipeline_data(void)
{
	const struct ipc4_pipeline_set_state_data *ppl_data;

	ppl_data = (const struct ipc4_pipeline_set_state_data *)MAILBOX_HOSTBOX_BASE;
	dcache_invalidate_region((__sparse_force void __sparse_cache *)ppl_data,
				 sizeof(*ppl_data));

	return ppl_data;
}
#endif
/*
 * Global IPC Operations.
 */
static int ipc4_new_pipeline(struct ipc4_message_request *ipc4)
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

	/* Stay on the current pipeline */
	if (current->pipeline != ((struct pipeline_data *)ctx->comp_data)->p)
		return 0;

	err = comp_params(current, &ppl_data->params->params);
	if (err < 0 || err == PPL_STATUS_PATH_STOP)
		return err;

	return pipeline_for_each_comp(current, ctx, dir);
}

static int ipc4_pipeline_params(struct pipeline *p, struct comp_dev *host)
{
	struct sof_ipc_pcm_params hw_params = {{ 0 }};
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
	int err, reset_err;

	/* sanity check comp */
	if (!pcm_dev->cd->pipeline) {
		ipc_cmd_err(&ipc_tr, "ipc: comp %d pipeline not found", pcm_dev->id);
		return -EINVAL;
	}

	/* configure pipeline audio params */
	err = ipc4_pipeline_params(pcm_dev->cd->pipeline, pcm_dev->cd);
	if (err < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: pipe %d comp %d params failed %d",
			    pcm_dev->cd->pipeline->pipeline_id,
			    pcm_dev->cd->pipeline->comp_id, err);
		goto error;
	}

	/* prepare pipeline audio params */
	err = pipeline_prepare(pcm_dev->cd->pipeline, pcm_dev->cd);
	if (err < 0) {
		ipc_cmd_err(&ipc_tr, "ipc: pipe %d comp %d prepare failed %d",
			    pcm_dev->cd->pipeline->pipeline_id,
			    pcm_dev->cd->pipeline->comp_id, err);
		goto error;
	}

	return 0;

error:
	reset_err = pipeline_reset(pcm_dev->cd->pipeline, pcm_dev->cd);
	if (reset_err < 0)
		ipc_cmd_err(&ipc_tr, "ipc: pipe %d comp %d reset failed %d",
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

static struct ipc_comp_dev *pipeline_get_host_dev(struct ipc_comp_dev *ppl_icd)
{
	struct ipc_comp_dev *host_dev;
	struct ipc *ipc = ipc_get();
	int host_id;

	/* If the source component's direction is not set but the sink's direction is,
	 * this block will copy the direction from the sink to the source component and
	 * mark the source's direction as set.
	 */
	if (!ppl_icd->pipeline->source_comp->direction_set &&
	    ppl_icd->pipeline->sink_comp->direction_set) {
		ppl_icd->pipeline->source_comp->direction =
			ppl_icd->pipeline->sink_comp->direction;
		ppl_icd->pipeline->source_comp->direction_set = true;
	}

	/* If the sink component's direction is not set but the source's direction is,
	 * this block will copy the direction from the source to the sink component and
	 * mark the sink's direction as set.
	 */
	if (!ppl_icd->pipeline->sink_comp->direction_set &&
	    ppl_icd->pipeline->source_comp->direction_set) {
		ppl_icd->pipeline->sink_comp->direction =
			ppl_icd->pipeline->source_comp->direction;
		ppl_icd->pipeline->sink_comp->direction_set = true;
	}

	if (ppl_icd->pipeline->source_comp->direction == SOF_IPC_STREAM_PLAYBACK)
		host_id = ppl_icd->pipeline->source_comp->ipc_config.id;
	else
		host_id = ppl_icd->pipeline->sink_comp->ipc_config.id;

	host_dev = ipc_get_comp_by_id(ipc, host_id);
	if (!host_dev)
		ipc_cmd_err(&ipc_tr, "comp host with ID %d not found", host_id);

	return host_dev;
}

/* Ipc4 pipeline message <------> ipc3 pipeline message
 * RUNNING     <-------> TRIGGER START
 * INIT + PAUSED  <-------> PIPELINE COMPLETE
 * INIT + RESET <-------> PIPELINE COMPLETE
 * PAUSED      <-------> TRIGGER_PAUSE
 * RESET       <-------> TRIGGER_STOP + RESET
 * EOS(end of stream) <-------> NOT SUPPORTED YET
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

int ipc4_pipeline_prepare(struct ipc_comp_dev *ppl_icd, uint32_t cmd)
{
	struct ipc_comp_dev *host = NULL;
	struct ipc *ipc = ipc_get();
	int status;
	int ret = 0;

	status = ppl_icd->pipeline->status;
	tr_dbg(&ipc_tr, "pipeline %d: initial state: %d, cmd: %d", ppl_icd->id,
	       status, cmd);

	switch (cmd) {
	case SOF_IPC4_PIPELINE_STATE_RUNNING:
		/* init params when pipeline is complete or reset */
		switch (status) {
		case COMP_STATE_ACTIVE:
		case COMP_STATE_PAUSED:
			/* No action needed */
			break;
		case COMP_STATE_READY:
			host = pipeline_get_host_dev(ppl_icd);
			if (!host)
				return IPC4_INVALID_RESOURCE_ID;

			tr_dbg(&ipc_tr, "pipeline %d: set params", ppl_icd->id);
			ret = ipc4_pcm_params(host);
			if (ret < 0)
				return IPC4_INVALID_REQUEST;
			break;
		default:
			ipc_cmd_err(&ipc_tr,
				    "pipeline %d: Invalid state for RUNNING: %d",
				    ppl_icd->id, status);
			return IPC4_INVALID_REQUEST;
		}
		break;
	case SOF_IPC4_PIPELINE_STATE_RESET:
		switch (status) {
		case COMP_STATE_INIT:
			tr_dbg(&ipc_tr, "pipeline %d: reset from init", ppl_icd->id);
			ret = ipc4_pipeline_complete(ipc, ppl_icd->id, cmd);
			if (ret < 0)
				ret = IPC4_INVALID_REQUEST;

			break;
		case COMP_STATE_READY:
		case COMP_STATE_ACTIVE:
		case COMP_STATE_PAUSED:
			/* No action needed */
			break;
		default:
			ipc_cmd_err(&ipc_tr,
				    "pipeline %d: Invalid state for RESET: %d",
				    ppl_icd->id, status);
			return IPC4_INVALID_REQUEST;
		}

		break;
	case SOF_IPC4_PIPELINE_STATE_PAUSED:
		switch (status) {
		case COMP_STATE_INIT:
			tr_dbg(&ipc_tr, "pipeline %d: pause from init", ppl_icd->id);
			ret = ipc4_pipeline_complete(ipc, ppl_icd->id, cmd);
			if (ret < 0)
				ret = IPC4_INVALID_REQUEST;

			break;
		default:
			/* No action needed */
			break;
		}

		break;
	/* special case- TODO */
	case SOF_IPC4_PIPELINE_STATE_EOS:
		if (status != COMP_STATE_ACTIVE)
			return IPC4_INVALID_REQUEST;
		COMPILER_FALLTHROUGH;
	case SOF_IPC4_PIPELINE_STATE_SAVED:
	case SOF_IPC4_PIPELINE_STATE_ERROR_STOP:
	default:
		ipc_cmd_err(&ipc_tr, "pipeline %d: unsupported trigger cmd: %d",
			    ppl_icd->id, cmd);
		return IPC4_INVALID_REQUEST;
	}

	return ret;
}

int ipc4_pipeline_trigger(struct ipc_comp_dev *ppl_icd, uint32_t cmd, bool *delayed)
{
	struct ipc_comp_dev *host;
	int status;
	int ret;

	status = ppl_icd->pipeline->status;
	tr_dbg(&ipc_tr, "pipeline %d: initial state: %d, cmd: %d", ppl_icd->id,
	       status, cmd);

	if (status == COMP_STATE_INIT)
		return 0;

	host = pipeline_get_host_dev(ppl_icd);
	if (!host)
		return IPC4_INVALID_RESOURCE_ID;

	switch (cmd) {
	case SOF_IPC4_PIPELINE_STATE_RUNNING:
		/* init params when pipeline is complete or reset */
		switch (status) {
		case COMP_STATE_ACTIVE:
			/* nothing to do if the pipeline is already running */
			return 0;
		case COMP_STATE_READY:
		case COMP_STATE_PREPARE:
			cmd = COMP_TRIGGER_PRE_START;
			break;
		case COMP_STATE_PAUSED:
			cmd = COMP_TRIGGER_PRE_RELEASE;
			break;
		default:
			ipc_cmd_err(&ipc_tr,
				    "pipeline %d: Invalid state for RUNNING: %d",
				    ppl_icd->id, status);
			return IPC4_INVALID_REQUEST;
		}
		break;
	case SOF_IPC4_PIPELINE_STATE_RESET:
		switch (status) {
		case COMP_STATE_ACTIVE:
		case COMP_STATE_PAUSED:
			cmd = COMP_TRIGGER_STOP;
			break;
		default:
			return 0;
		}
		break;
	case SOF_IPC4_PIPELINE_STATE_PAUSED:
		switch (status) {
		case COMP_STATE_INIT:
		case COMP_STATE_READY:
		case COMP_STATE_PAUSED:
			return 0;
		default:
			cmd = COMP_TRIGGER_PAUSE;
			break;
		}

		break;
	default:
		ipc_cmd_err(&ipc_tr, "pipeline %d: unsupported trigger cmd: %d",
			    ppl_icd->id, cmd);
		return IPC4_INVALID_REQUEST;
	}

	/* trigger the component */
	ret = pipeline_trigger(host->cd->pipeline, host->cd, cmd);
	if (ret < 0) {
		ipc_cmd_err(&ipc_tr, "pipeline %d: trigger cmd %d failed with: %d",
			    ppl_icd->id, cmd, ret);
		ret = IPC4_PIPELINE_STATE_NOT_SET;
	} else if (ret == PPL_STATUS_SCHEDULED) {
		tr_dbg(&ipc_tr, "pipeline %d: trigger cmd %d is delayed",
		       ppl_icd->id, cmd);
		*delayed = true;
		ret = 0;
	} else if (cmd == COMP_TRIGGER_STOP) {
		/*
		 * reset the pipeline components if STOP trigger is executed in
		 * the same thread.
		 * Otherwise, the pipeline will be reset after the STOP trigger
		 * has finished executing in the pipeline task.
		 */
		ret = pipeline_reset(host->cd->pipeline, host->cd);
		if (ret < 0)
			ret = IPC4_INVALID_REQUEST;
	}

	return ret;
}

static void ipc_compound_pre_start(int msg_id)
{
	/* ipc thread will wait for all scheduled tasks to be complete
	 * Use a reference count to check status of these tasks.
	 */
	atomic_add(&msg_data.delayed_reply, 1);
}

static void ipc_compound_post_start(uint32_t msg_id, int ret, bool delayed)
{
	if (ret) {
		ipc_cmd_err(&ipc_tr, "failed to process msg %d status %d", msg_id, ret);
		atomic_set(&msg_data.delayed_reply, 0);
		return;
	}

	/* decrease counter if it is not scheduled by another thread */
	if (!delayed)
		atomic_sub(&msg_data.delayed_reply, 1);
}

static void ipc_compound_msg_done(uint32_t msg_id, int error)
{
	if (!atomic_read(&msg_data.delayed_reply)) {
		ipc_cmd_err(&ipc_tr, "unexpected delayed reply");
		return;
	}

	atomic_sub(&msg_data.delayed_reply, 1);

	/* error reported in delayed pipeline task */
	if (error < 0) {
		if (msg_id == SOF_IPC4_GLB_SET_PIPELINE_STATE)
			msg_data.delayed_error = IPC4_PIPELINE_STATE_NOT_SET;
	}
}

#if CONFIG_LIBRARY
/* There is no parallel execution in testbench for scheduler and pipelines, so the result would
 * be always IPC4_FAILURE. Therefore the compound messages handling is simplified. The pipeline
 * triggers will require an explicit scheduler call to get the components to desired state.
 */
static int ipc_wait_for_compound_msg(void)
{
	atomic_set(&msg_data.delayed_reply, 0);
	return IPC4_SUCCESS;
}
#else
static int ipc_wait_for_compound_msg(void)
{
	int try_count = 30;

	while (atomic_read(&msg_data.delayed_reply)) {
		k_sleep(Z_TIMEOUT_US(250));

		if (!try_count--) {
			atomic_set(&msg_data.delayed_reply, 0);
			ipc_cmd_err(&ipc_tr, "ipc4: failed to wait schedule thread");
			return IPC4_FAILURE;
		}
	}

	return IPC4_SUCCESS;
}
#endif

const struct ipc4_pipeline_set_state_data *ipc4_get_pipeline_data_wrapper(void)
{
	const struct ipc4_pipeline_set_state_data *ppl_data;

	ppl_data = ipc4_get_pipeline_data();

	return ppl_data;
}

static int ipc4_set_pipeline_state(struct ipc4_message_request *ipc4)
{
	const struct ipc4_pipeline_set_state_data *ppl_data;
	struct ipc4_pipeline_set_state state;
	struct ipc_comp_dev *ppl_icd;
	struct ipc *ipc = ipc_get();
	uint32_t cmd, ppl_count;
	uint32_t id = 0;
	const uint32_t *ppl_id;
	bool use_idc = false;
	uint32_t idx;
	int ret = 0;
	int i;

	state.primary.dat = ipc4->primary.dat;
	state.extension.dat = ipc4->extension.dat;
	cmd = state.primary.r.ppl_state;
	ppl_data = ipc4_get_pipeline_data();

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
		ppl_icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
						 ppl_id[i], IPC_COMP_IGNORE_REMOTE);
		if (!ppl_icd) {
			tr_err(&ipc_tr, "ipc: comp %d not found", ppl_id[i]);
			return IPC4_INVALID_RESOURCE_ID;
		}

		if (i) {
			if (ppl_icd->core != idx)
				use_idc = true;
		} else {
			idx = ppl_icd->core;
		}
	}

	/* Run the prepare phase on the pipelines */
	for (i = 0; i < ppl_count; i++) {
		ppl_icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
						 ppl_id[i], IPC_COMP_IGNORE_REMOTE);
		if (!ppl_icd) {
			ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", ppl_id[i]);
			return IPC4_INVALID_RESOURCE_ID;
		}

		/* Pass IPC to target core
		 * or use idc if more than one core used
		 */
		if (!cpu_is_me(ppl_icd->core)) {
			if (use_idc) {
				struct idc_msg msg = { IDC_MSG_PPL_STATE,
					IDC_MSG_PPL_STATE_EXT(ppl_id[i],
							      IDC_PPL_STATE_PHASE_PREPARE),
					ppl_icd->core,
					sizeof(cmd), &cmd, };

				ret = idc_send_msg(&msg, IDC_BLOCKING);
			} else {
				return ipc4_process_on_core(ppl_icd->core, false);
			}
		} else {
			ret = ipc4_pipeline_prepare(ppl_icd, cmd);
		}

		if (ret != 0)
			return ret;
	}

	/* Run the trigger phase on the pipelines */
	for (i = 0; i < ppl_count; i++) {
		bool delayed = false;

		ppl_icd = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE,
						 ppl_id[i], IPC_COMP_IGNORE_REMOTE);
		if (!ppl_icd) {
			ipc_cmd_err(&ipc_tr, "ipc: comp %d not found", ppl_id[i]);
			return IPC4_INVALID_RESOURCE_ID;
		}

		/* Pass IPC to target core
		 * or use idc if more than one core used
		 */
		if (!cpu_is_me(ppl_icd->core)) {
			if (use_idc) {
				struct idc_msg msg = { IDC_MSG_PPL_STATE,
					IDC_MSG_PPL_STATE_EXT(ppl_id[i],
							      IDC_PPL_STATE_PHASE_TRIGGER),
					ppl_icd->core,
					sizeof(cmd), &cmd, };

				ret = idc_send_msg(&msg, IDC_BLOCKING);
			} else {
				return ipc4_process_on_core(ppl_icd->core, false);
			}
		} else {
			ipc_compound_pre_start(state.primary.r.type);
			ret = ipc4_pipeline_trigger(ppl_icd, cmd, &delayed);
			ipc_compound_post_start(state.primary.r.type, ret, delayed);
			if (delayed) {
				/* To maintain pipeline order for triggers, we must
				 * do a blocking wait until trigger is processed.
				 * This will add a max delay of 'ppl_count' LL ticks
				 * to process the full trigger list.
				 */
				if (ipc_wait_for_compound_msg() != 0) {
					ipc_cmd_err(&ipc_tr, "ipc4: fail with delayed trigger");
					return IPC4_FAILURE;
				}
			}
		}

		if (ret != 0)
			return ret;
	}

	return ret;
}

#if CONFIG_LIBRARY_MANAGER
static int ipc4_load_library(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_load_library library;
	int ret;

	library.header.dat = ipc4->primary.dat;

	ret = lib_manager_load_library(library.header.r.dma_id, library.header.r.lib_id,
				       ipc4->primary.r.type);
	if (ret != 0)
		return (ret == -EINVAL) ? IPC4_ERROR_INVALID_PARAM : IPC4_FAILURE;

	return IPC4_SUCCESS;
}
#endif

static int ipc4_process_chain_dma(struct ipc4_message_request *ipc4)
{
#if CONFIG_COMP_CHAIN_DMA
	struct ipc_comp_dev *cdma_comp;
	struct ipc *ipc = ipc_get();
	struct ipc4_chain_dma cdma;
	int comp_id;
	int ret;

	ret = memcpy_s(&cdma, sizeof(cdma), ipc4, sizeof(*ipc4));
	if (ret < 0)
		return IPC4_FAILURE;

	comp_id = IPC4_COMP_ID(cdma.primary.r.host_dma_id + IPC4_MAX_MODULE_COUNT, 0);
	cdma_comp = ipc_get_comp_by_id(ipc, comp_id);

	if (!cdma_comp) {
		/*
		 * Nothing to do when the chainDMA is not allocated and asked to
		 * be freed
		 */
		if (!cdma.primary.r.allocate && !cdma.primary.r.enable)
			return IPC4_SUCCESS;

		ret = ipc4_chain_manager_create(&cdma);
		if (ret < 0)
			return IPC4_FAILURE;

		cdma_comp = ipc_get_comp_by_id(ipc, comp_id);
		if (!cdma_comp) {
			return IPC4_FAILURE;
		}

		ret = ipc4_chain_dma_state(cdma_comp->cd, &cdma);
		if (ret < 0) {
			comp_free(cdma_comp->cd);
			return IPC4_FAILURE;
		}

		return IPC4_SUCCESS;
	}

	ret = ipc4_chain_dma_state(cdma_comp->cd, &cdma);
	if (ret < 0)
		return IPC4_INVALID_CHAIN_STATE_TRANSITION;

	if (!cdma.primary.r.allocate && !cdma.primary.r.enable)
		list_item_del(&cdma_comp->list);

	return IPC4_SUCCESS;
#else
	return IPC4_UNAVAILABLE;
#endif
}

static int ipc4_process_ipcgtw_cmd(struct ipc4_message_request *ipc4)
{
#if CONFIG_IPC4_GATEWAY
	struct ipc *ipc = ipc_get();
	uint32_t reply_size = 0;
	int err;

	/* NOTE: reply implementation is messy! First, reply payload is copied
	 * to ipc->comp_data buffer. Then, new buffer is allocated and assigned
	 * to msg_reply.tx_data. ipc_msg_send() copies payload from ipc->comp_data
	 * to msg_reply.tx_data. Then, ipc_prepare_to_send() copies payload from
	 * msg_reply.tx_data to memory window and frees msg_reply.tx_data. That is
	 * quite weird: seems one extra copying can be eliminated.
	 */

	err = copier_ipcgtw_process((const struct ipc4_ipcgtw_cmd *)ipc4, ipc->comp_data,
				    &reply_size);
	/* reply size is returned in header extension dword */
	msg_reply.extension = reply_size;

	if (reply_size > 0) {
		msg_reply.tx_data = rballoc(0, SOF_MEM_CAPS_RAM, reply_size);
		if (msg_reply.tx_data) {
			msg_reply.tx_size = reply_size;
		} else {
			ipc_cmd_err(&ipc_tr, "failed to allocate %u bytes for msg_reply.tx_data",
				    reply_size);
			msg_reply.extension = 0;
			return IPC4_OUT_OF_MEMORY;
		}
	}

	return err < 0 ? IPC4_FAILURE : IPC4_SUCCESS;
#else
	ipc_cmd_err(&ipc_tr, "CONFIG_IPC4_GATEWAY is disabled");
	return IPC4_UNAVAILABLE;
#endif
}

static int ipc4_process_glb_message(struct ipc4_message_request *ipc4)
{
	uint32_t type;
	int ret;

	type = ipc4->primary.r.type;

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
		ret = ipc4_new_pipeline(ipc4);
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
		ipc_cmd_err(&ipc_tr, "not implemented ipc message type %d", type);
		ret = IPC4_UNAVAILABLE;
		break;

	/* Loads library (using Code Load or HD/A Host Output DMA) */
#ifdef CONFIG_LIBRARY_MANAGER
	case SOF_IPC4_GLB_LOAD_LIBRARY:
		ret = ipc4_load_library(ipc4);
		break;
	case SOF_IPC4_GLB_LOAD_LIBRARY_PREPARE:
		ret = ipc4_load_library(ipc4);
		break;
#endif
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

static int ipc4_init_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_init_instance module_init;
	struct comp_dev *dev;
	/* we only need the common header here, all we have from the IPC */
	int ret = memcpy_s(&module_init, sizeof(module_init), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	tr_dbg(&ipc_tr,
		"ipc4_init_module_instance %x : %x",
		(uint32_t)module_init.primary.r.module_id,
		(uint32_t)module_init.primary.r.instance_id);

	/* Pass IPC to target core */
	if (!cpu_is_me(module_init.extension.r.core_id))
		return ipc4_process_on_core(module_init.extension.r.core_id, false);

	dev = comp_new_ipc4(&module_init);
	if (!dev) {
		ipc_cmd_err(&ipc_tr, "error: failed to init module %x : %x",
			    (uint32_t)module_init.primary.r.module_id,
			    (uint32_t)module_init.primary.r.instance_id);
		return IPC4_MOD_NOT_INITIALIZED;
	}

	return 0;
}

static int ipc4_bind_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_bind_unbind bu;
	struct ipc *ipc = ipc_get();
	int ret = memcpy_s(&bu, sizeof(bu), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	tr_dbg(&ipc_tr, "ipc4_bind_module_instance %x : %x with %x : %x",
	       (uint32_t)bu.primary.r.module_id, (uint32_t)bu.primary.r.instance_id,
	       (uint32_t)bu.extension.r.dst_module_id, (uint32_t)bu.extension.r.dst_instance_id);

	return ipc_comp_connect(ipc, (ipc_pipe_comp_connect *)&bu);
}

static int ipc4_unbind_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_bind_unbind bu;
	struct ipc *ipc = ipc_get();
	int ret = memcpy_s(&bu, sizeof(bu), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	tr_dbg(&ipc_tr, "ipc4_unbind_module_instance %x : %x with %x : %x",
	       (uint32_t)bu.primary.r.module_id, (uint32_t)bu.primary.r.instance_id,
	       (uint32_t)bu.extension.r.dst_module_id, (uint32_t)bu.extension.r.dst_instance_id);

	return ipc_comp_disconnect(ipc, (ipc_pipe_comp_connect *)&bu);
}

static int  ipc4_get_vendor_config_module_instance(struct comp_dev *dev,
						   const struct comp_driver *drv,
						   bool init_block,
						   bool final_block,
						   uint32_t *data_off_size,
						   char *data_out,
						   const char *data_in)
{
	const struct sof_tl * const input_tl = (struct sof_tl *)data_in;
	int ret;
	struct ipc4_vendor_error *error;

	if (init_block && final_block) {
		/* we use data_off_size as in/out,
		 *  save value to new variable so it can be used as out size
		 */
		const int tl_count = *data_off_size / sizeof(struct sof_tl);
		size_t produced_data = 0;

		for (int i = 0; i < tl_count; i++) {
			/* we go to next output tlv with each iteration */
			uint32_t data_off_size_local;
			struct sof_tlv *output_tlv = (struct sof_tlv *)(data_out + produced_data);

			if (produced_data + input_tl[i].max_length > MAILBOX_DSPBOX_SIZE) {
				tr_err(&ipc_tr, "error: response payload bigger than DSPBOX size");
				return IPC4_FAILURE;
			}

			/* local size is in/out: max msg len goes in, msg len goes out */
			data_off_size_local = input_tl[i].max_length;
			ret = drv->ops.get_large_config(dev, input_tl[i].type,
							true,
							true,
							&data_off_size_local, output_tlv->value);
			if (ret) {
				/* This is how the reference firmware handled error here. Currently
				 * no memory is allocated for output in case of error,
				 * so this may be obsolete.
				 */
				error = (struct ipc4_vendor_error *)data_out;
				error->param_idx = input_tl[i].type;
				error->err_code = IPC4_FAILURE;
				*data_off_size = sizeof(*error);
				ipc_cmd_err(&ipc_tr, "error: get_large_config returned %d", ret);
				return IPC4_FAILURE;
			}

			/* update header */
			output_tlv->type = input_tl[i].type;
			output_tlv->length = data_off_size_local;
			produced_data += data_off_size_local + sizeof(*output_tlv);
		}
		*data_off_size = produced_data;
	} else {
		char *output_buffer;
		struct sof_tlv *tl_header;

		if (init_block) {
			*data_off_size = input_tl->max_length;
			output_buffer = data_out + sizeof(*tl_header);
		} else {
			output_buffer = data_out;
		}

		ret = drv->ops.get_large_config(dev, input_tl->type,
						init_block,
						final_block,
						data_off_size, output_buffer);

		/* on error report which param failed */
		if (ret) {
			error = (struct ipc4_vendor_error *)data_out;
			error->param_idx = input_tl->type;
			error->err_code = IPC4_FAILURE;
			*data_off_size = sizeof(*error);
			ipc_cmd_err(&ipc_tr, "error: get_large_config returned %d", ret);
			return IPC4_FAILURE;
		}

		/* for initial block update TL header */
		if (init_block) {
			/* we use tlv struct here for clarity, we have length not max_length */
			tl_header = (struct sof_tlv *)data_out;
			tl_header->type = input_tl->type;
			tl_header->length = *data_off_size;
			/* for initial block data_off_size includes also size of TL */
			*data_off_size += sizeof(*tl_header);
		}
	}
	return IPC4_SUCCESS;
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
	int ret = memcpy_s(&config, sizeof(config), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	tr_dbg(&ipc_tr, "ipc4_get_large_config_module_instance %x : %x",
	       (uint32_t)config.primary.r.module_id, (uint32_t)config.primary.r.instance_id);

	/* get component dev for non-basefw since there is no
	 * component dev for basefw
	 */
	if (config.primary.r.module_id) {
		uint32_t comp_id;

		comp_id = IPC4_COMP_ID(config.primary.r.module_id,
				       config.primary.r.instance_id);
		dev = ipc4_get_comp_dev(comp_id);
		if (!dev)
			return IPC4_MOD_INVALID_ID;

		drv = dev->drv;

		/* Pass IPC to target core */
		if (!cpu_is_me(dev->ipc_config.core))
			return ipc4_process_on_core(dev->ipc_config.core, false);
	} else {
		drv = ipc4_get_comp_drv(config.primary.r.module_id);
	}

	if (!drv)
		return IPC4_MOD_INVALID_ID;

	if (!drv->ops.get_large_config)
		return IPC4_INVALID_REQUEST;

	data_offset =  config.extension.r.data_off_size;

	/* check for vendor param first */
	if (config.extension.r.large_param_id == VENDOR_CONFIG_PARAM) {
		/* For now only vendor_config case uses payload from hostbox */
		dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
					 config.extension.r.data_off_size);
		ret = ipc4_get_vendor_config_module_instance(dev, drv,
							     config.extension.r.init_block,
							     config.extension.r.final_block,
							     &data_offset,
							     data,
							     (const char *)MAILBOX_HOSTBOX_BASE);
	} else {
#if CONFIG_LIBRARY
		data += sizeof(reply);
#endif
		ret = drv->ops.get_large_config(dev, config.extension.r.large_param_id,
						config.extension.r.init_block,
						config.extension.r.final_block,
						&data_offset, data);
	}

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
		ipc_cmd_err(&ipc_tr, "error: failed to allocate tx_data");
		ret = IPC4_OUT_OF_MEMORY;
	}

	return ret;
}

static int ipc4_set_vendor_config_module_instance(struct comp_dev *dev,
						  const struct comp_driver *drv,
						  uint32_t module_id,
						  uint32_t instance_id,
						  bool init_block,
						  bool final_block,
						  uint32_t data_off_size,
						  const char *data)
{
	int ret;

	/* Old FW comment: bursted configs */
	if (init_block && final_block) {
		const struct sof_tlv *tlv = (struct sof_tlv *)data;
		/* if there is no payload in this large config set
		 * (4 bytes type | 4 bytes length=0 | no value)
		 * we do not handle such case
		 */
		if (data_off_size < sizeof(struct sof_tlv))
			return IPC4_INVALID_CONFIG_DATA_STRUCT;

		/* ===Iterate over payload===
		 * Payload can have multiple sof_tlv structures inside,
		 * You can find how many by checking payload size (data_off_size)
		 * Here we just set pointer end_offset to the end of data
		 * and iterate until we reach that
		 */
		const uint8_t *end_offset = (const uint8_t *)data + data_off_size;

		while ((const uint8_t *)tlv < end_offset) {
			/* check for invalid length */
			if (!tlv->length)
				return IPC4_INVALID_CONFIG_DATA_LEN;

			ret = drv->ops.set_large_config(dev, tlv->type, init_block,
				final_block, tlv->length, tlv->value);
			if (ret < 0) {
				ipc_cmd_err(&ipc_tr, "failed to set large_config_module_instance %x : %x",
					    (uint32_t)module_id, (uint32_t)instance_id);
				return IPC4_INVALID_RESOURCE_ID;
			}
			/* Move pointer to the end of this tlv */
			tlv = (struct sof_tlv *)((const uint8_t *)tlv +
				sizeof(struct sof_tlv) + ALIGN_UP(tlv->length, 4));
		}
		return IPC4_SUCCESS;
	}
	/* else, !(init_block && final_block) */
	const struct sof_tlv *tlv = (struct sof_tlv *)data;
	uint32_t param_id = 0;

	if (init_block) {
		/* for initial block use param_id from tlv
		 * move pointer and size to end of the tlv
		 */
		param_id = tlv->type;
		data += sizeof(struct sof_tlv);
		data_off_size -= sizeof(struct sof_tlv);
	}
	return drv->ops.set_large_config(dev, param_id, init_block, final_block,
					 data_off_size, data);
}

static int ipc4_set_large_config_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_large_config config;
	struct comp_dev *dev = NULL;
	const struct comp_driver *drv;
	int ret = memcpy_s(&config, sizeof(config), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 config.extension.r.data_off_size);
	tr_dbg(&ipc_tr, "ipc4_set_large_config_module_instance %x : %x",
	       (uint32_t)config.primary.r.module_id, (uint32_t)config.primary.r.instance_id);

	if (config.primary.r.module_id) {
		uint32_t comp_id;

		comp_id = IPC4_COMP_ID(config.primary.r.module_id, config.primary.r.instance_id);
		dev = ipc4_get_comp_dev(comp_id);
		if (!dev)
			return IPC4_MOD_INVALID_ID;

		drv = dev->drv;

		/* Pass IPC to target core */
		if (!cpu_is_me(dev->ipc_config.core))
			return ipc4_process_on_core(dev->ipc_config.core, false);
	} else {
		drv = ipc4_get_comp_drv(config.primary.r.module_id);
	}

	if (!drv)
		return IPC4_MOD_INVALID_ID;

	if (!drv->ops.set_large_config)
		return IPC4_INVALID_REQUEST;

	/* check for vendor param first */
	if (config.extension.r.large_param_id == VENDOR_CONFIG_PARAM) {
		ret = ipc4_set_vendor_config_module_instance(dev, drv,
							     (uint32_t)config.primary.r.module_id,
							     (uint32_t)config.primary.r.instance_id,
							     config.extension.r.init_block,
							     config.extension.r.final_block,
							     config.extension.r.data_off_size,
							     (const char *)MAILBOX_HOSTBOX_BASE);
	} else {
#if CONFIG_LIBRARY
		struct ipc *ipc = ipc_get();
		const char *data = (const char *)ipc->comp_data + sizeof(config);
#else
		const char *data = (const char *)MAILBOX_HOSTBOX_BASE;
#endif
		ret = drv->ops.set_large_config(dev, config.extension.r.large_param_id,
			config.extension.r.init_block, config.extension.r.final_block,
			config.extension.r.data_off_size, data);
		if (ret < 0) {
			ipc_cmd_err(&ipc_tr, "failed to set large_config_module_instance %x : %x",
				    (uint32_t)config.primary.r.module_id,
				    (uint32_t)config.primary.r.instance_id);
			ret = IPC4_INVALID_RESOURCE_ID;
		}
	}

	return ret;
}

static int ipc4_delete_module_instance(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_delete_instance module;
	struct ipc *ipc = ipc_get();
	uint32_t comp_id;
	int ret = memcpy_s(&module, sizeof(module), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	tr_dbg(&ipc_tr, "ipc4_delete_module_instance %x : %x", (uint32_t)module.primary.r.module_id,
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

/* disable power gating on core 0 */
static int ipc4_module_process_d0ix(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_set_d0ix d0ix;
	uint32_t module_id, instance_id;
	int ret = memcpy_s(&d0ix, sizeof(d0ix), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	module_id = d0ix.primary.r.module_id;
	instance_id = d0ix.primary.r.instance_id;

	tr_dbg(&ipc_tr, "ipc4_module_process_d0ix %x : %x", module_id, instance_id);

	/* only module 0 can be used to set d0ix state */
	if (d0ix.primary.r.module_id || d0ix.primary.r.instance_id) {
		ipc_cmd_err(&ipc_tr, "invalid resource id %x : %x", module_id, instance_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	if (d0ix.extension.r.prevent_power_gating)
		pm_runtime_disable(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);
	else
		pm_runtime_enable(PM_RUNTIME_DSP, PLATFORM_PRIMARY_CORE_ID);

	return 0;
}

/* enable/disable cores according to the state mask */
static int ipc4_module_process_dx(struct ipc4_message_request *ipc4)
{
	struct ipc4_module_set_dx dx;
	struct ipc4_dx_state_info dx_info;
	uint32_t module_id, instance_id;
	uint32_t core_id;
	int ret = memcpy_s(&dx, sizeof(dx), ipc4, sizeof(*ipc4));

	if (ret < 0)
		return IPC4_FAILURE;

	module_id = dx.primary.r.module_id;
	instance_id = dx.primary.r.instance_id;

	/* only module 0 can be used to set dx state */
	if (module_id || instance_id) {
		ipc_cmd_err(&ipc_tr, "invalid resource id %x : %x", module_id, instance_id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	dcache_invalidate_region((__sparse_force void __sparse_cache *)MAILBOX_HOSTBOX_BASE,
				 sizeof(dx_info));
	ret = memcpy_s(&dx_info, sizeof(dx_info),
		       (const void *)MAILBOX_HOSTBOX_BASE, sizeof(dx_info));
	if (ret < 0)
		return IPC4_FAILURE;

	/* check if core enable mask is valid */
	if (dx_info.core_mask > MASK(CONFIG_CORE_COUNT - 1, 0)) {
		ipc_cmd_err(&ipc_tr, "ipc4_module_process_dx: CONFIG_CORE_COUNT: %d < core enable mask: %d",
			    CONFIG_CORE_COUNT, dx_info.core_mask);
		return IPC4_ERROR_INVALID_PARAM;
	}

	/* check primary core first */
	if ((dx_info.core_mask & BIT(PLATFORM_PRIMARY_CORE_ID)) &&
	    (dx_info.dx_mask & BIT(PLATFORM_PRIMARY_CORE_ID))) {
		/* core0 can't be activated more, it's already active since we got here */
		ipc_cmd_err(&ipc_tr, "Core0 is already active");
		return IPC4_BAD_STATE;
	}

	/* Activate/deactivate requested cores */
	for (core_id = 1; core_id < CONFIG_CORE_COUNT; core_id++) {
		if ((dx_info.core_mask & BIT(core_id)) == 0)
			continue;

		if (dx_info.dx_mask & BIT(core_id)) {
			ret = cpu_enable_core(core_id);
			if (ret != 0) {
				ipc_cmd_err(&ipc_tr, "failed to enable core %d", core_id);
				return IPC4_FAILURE;
			}
		} else {
			cpu_disable_core(core_id);
			if (cpu_is_core_enabled(core_id)) {
				ipc_cmd_err(&ipc_tr, "failed to disable core %d", core_id);
				return IPC4_FAILURE;
			}
		}
	}

	/* Deactivating primary core if requested.  */
	if (dx_info.core_mask & BIT(PLATFORM_PRIMARY_CORE_ID)) {
		if (cpu_enabled_cores() & ~BIT(PLATFORM_PRIMARY_CORE_ID)) {
			ipc_cmd_err(&ipc_tr, "secondary cores 0x%x still active",
				    cpu_enabled_cores());
			return IPC4_BUSY;
		}

		if (is_any_ppl_active()) {
			ipc_cmd_err(&ipc_tr, "some pipelines are still active");
			return IPC4_BUSY;
		}

#if defined(CONFIG_PM)
		ipc_get()->task_mask |= IPC_TASK_POWERDOWN;
#endif
		/* do platform specific suspending */
		platform_context_save(sof_get());

#if !defined(CONFIG_LIBRARY) && !defined(CONFIG_ZEPHYR_NATIVE_DRIVERS)
		arch_irq_lock();
		platform_timer_stop(timer_get());
#endif
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

	if (msg->tx_size) {
		/* Invalidate cache to ensure we read the latest data from memory.
		 * The response was prepared on a secondary core but will be sent
		 * to the host from the primary core.
		 */
		if (msg->is_shared) {
			dcache_invalidate_region((__sparse_force void __sparse_cache *)msg->tx_data,
						 msg->tx_size);
		}

		mailbox_dspbox_write(0, (uint32_t *)msg->tx_data, msg->tx_size);
	}

	/* free memory for get config function */
	if (msg == &msg_reply && msg_reply.tx_size > 0) {
		rfree(msg_reply.tx_data);
		msg_reply.tx_data = NULL;
		msg_reply.tx_size = 0;
		msg_reply.is_shared = false;
	}

	return &msg_data.msg_out;
}

void ipc_boot_complete_msg(struct ipc_cmd_hdr *header, uint32_t data)
{
	header->pri = SOF_IPC4_FW_READY;
	header->ext = 0;
}

#if defined(CONFIG_PM_DEVICE) && defined(CONFIG_INTEL_ADSP_IPC)
void ipc_send_failed_power_transition_response(void)
{
	struct ipc4_message_request *request = ipc_from_hdr(&msg_data.msg_in);
	struct ipc4_message_reply response;

	response.primary.r.status = IPC4_POWER_TRANSITION_FAILED;
	response.primary.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REPLY;
	response.primary.r.msg_tgt = request->primary.r.msg_tgt;
	response.primary.r.type = request->primary.r.type;

	msg_reply.header = response.primary.dat;
	list_init(&msg_reply.list);

	ipc_msg_send_direct(&msg_reply, NULL);
}
#endif /* defined(CONFIG_PM_DEVICE) && defined(CONFIG_INTEL_ADSP_IPC) */

void ipc_send_panic_notification(void)
{
	msg_notify.header = SOF_IPC4_NOTIF_HEADER(SOF_IPC4_EXCEPTION_CAUGHT);
	msg_notify.extension = cpu_get_id();
	msg_notify.is_shared = !cpu_is_primary(cpu_get_id());
	msg_notify.tx_size = 0;
	msg_notify.tx_data = NULL;
	list_init(&msg_notify.list);

	ipc_msg_send_direct(&msg_notify, NULL);
}

#ifdef CONFIG_LOG_BACKEND_ADSP_MTRACE

static bool is_notification_queued(struct ipc_msg *msg)
{
	struct ipc *ipc = ipc_get();
	k_spinlock_key_t key;
	bool queued = false;

	key = k_spin_lock(&ipc->lock);
	if (!list_is_empty(&msg->list))
		queued = true;
	k_spin_unlock(&ipc->lock, key);

	return queued;
}

void ipc_send_buffer_status_notify(void)
{
	/* a single msg_notify object is used */
	if (is_notification_queued(&msg_notify))
		return;

	msg_notify.header = SOF_IPC4_NOTIF_HEADER(SOF_IPC4_NOTIFY_LOG_BUFFER_STATUS);
	msg_notify.extension = 0;
	msg_notify.tx_size = 0;
	msg_notify.is_shared = false;

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
	struct ipc4_message_request *in = ipc4_get_message_request();
	enum ipc4_message_target target;
	int err;

	if (cpu_is_primary(cpu_get_id()))
		tr_info(&ipc_tr, "rx\t: %#x|%#x", in->primary.dat, in->extension.dat);

	/* no process on scheduled thread */
	atomic_set(&msg_data.delayed_reply, 0);
	msg_data.delayed_error = 0;
	msg_reply.tx_size = 0;
	msg_reply.header = in->primary.dat;
	msg_reply.extension = in->extension.dat;

	target = in->primary.r.msg_tgt;

	switch (target) {
	case SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG:
		err = ipc4_process_glb_message(in);
		if (err)
			ipc_cmd_err(&ipc_tr, "ipc4: FW_GEN_MSG failed with err %d", err);
		break;
	case SOF_IPC4_MESSAGE_TARGET_MODULE_MSG:
		err = ipc4_process_module_message(in);
		if (err)
			ipc_cmd_err(&ipc_tr, "ipc4: MODULE_MSG failed with err %d", err);
		break;
	default:
		/* should not reach here as we only have 2 message types */
		ipc_cmd_err(&ipc_tr, "ipc4: invalid target %d", target);
		err = IPC4_UNKNOWN_MESSAGE_TYPE;
	}

	/* FW sends an ipc message to host if request bit is clear */
	if (in->primary.r.rsp == SOF_IPC4_MESSAGE_DIR_MSG_REQUEST) {
		struct ipc *ipc = ipc_get();
		struct ipc4_message_reply reply = {{0}};

		/* Process flow and time stamp for IPC4 msg processed on secondary core :
		 * core 0 (primary core)				core x (secondary core)
		 * # IPC msg thread		#IPC delayed worker     #core x idc thread
		 * ipc_task_ops.run()
		 * ipc_do_cmd()
		 * msg_reply.header = in->primary.dat
		 * ipc4_process_on_core(x)
		 * mask |= SECONDARY_CORE
		 * idc_send_message()
		 * Case 1:
		 * // Ipc msg processed by secondary core		idc_ipc()
		 * if ((mask & SECONDARY_CORE))				ipc_cmd()
		 *	return;						ipc_msg_send()
		 *							mask &= ~SECONDARY_CORE
		 *
		 *				ipc_platform_send_msg
		 * ----------------------------------------------------------------------------
		 * Case 2:
		 *                                                      idc_ipc()
		 *                                                      ipc_cmd()
		 *							//Prepare reply msg
		 *                                                      msg_reply.header =
		 *                                                      reply.primary.dat;
		 *                                                      ipc_msg_send()
		 *                                                      mask &= ~SECONDARY_CORE
		 *
		 * if ((mask & IPC_TASK_SECONDARY_CORE))
		 *	return;
		 * // Ipc reply msg was prepared, so return
		 * if (msg_reply.header != in->primary.dat)
		 *	return;
		 *				ipc_platform_send_msg
		 * ----------------------------------------------------------------------------
		 * Case 3:
		 *                                                      idc_ipc()
		 *                                                      ipc_cmd()
		 *							//Prepare reply msg
		 *                                                      msg_reply.header =
		 *                                                      reply.primary.dat;
		 *                                                      ipc_msg_send()
		 *                                                      mask &= ~SECONDARY_CORE
		 *
		 *                              ipc_platform_send_msg
		 *
		 * if ((mask & IPC_TASK_SECONDARY_CORE))
		 *      return;
		 * // Ipc reply msg was prepared, so return
		 * if (msg_reply.header != in->primary.dat)
		 *	return;
		 */

		/* Reply prepared by secondary core */
		if ((ipc->task_mask & IPC_TASK_SECONDARY_CORE) && cpu_is_primary(cpu_get_id()))
			return;
		/* Reply has been prepared by secondary core */
		if (msg_reply.header != in->primary.dat)
			return;

		/* Do not send reply for SET_DX if we are going to enter D3
		 * The reply is going to be sent as part of the power down
		 * sequence
		 */
		if (ipc->task_mask & IPC_TASK_POWERDOWN)
			return;

		if (ipc_wait_for_compound_msg() != 0) {
			ipc_cmd_err(&ipc_tr, "ipc4: failed to send delayed reply");
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

		tr_dbg(&ipc_tr, "tx-reply\t: %#x|%#x", msg_reply.header,
		       msg_reply.extension);

		ipc4_send_reply(&reply);
	}
}
