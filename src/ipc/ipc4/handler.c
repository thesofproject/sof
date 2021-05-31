// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
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
#include <sof/debug/gdb/gdb.h>
#include <sof/debug/panic.h>
#include <sof/drivers/idc.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/common.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/schedule.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/clk.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/dai.h>
#include <sof/lib/dma.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/mm_heap.h>
#include <sof/lib/pm_runtime.h>
#include <sof/list.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <sof/string.h>
#include <sof/trace/dma-trace.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/dai.h>
#include <ipc/debug.h>
#include <ipc4/error_status.h>
#include <ipc4/header.h>
#include <ipc4/module.h>
#include <ipc4/pipeline.h>
#include <ipc4/notification.h>
#include <ipc/pm.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <user/trace.h>
#include <ipc/probe.h>
#include <sof/probe/probe.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IPC4_COMP_ID(x, y) ((x) << 16 | (y))
#define IPC4_INST_ID(x) ((x) & 0xffff)


/*
 * Global IPC Operations.
 */
static int ipc4_create_pipeline(union ipc4_message_header *ipc4)
{
	struct ipc *ipc = ipc_get();

	tr_err(&ipc_tr, "ipc4 create pipeline %x:", (uint32_t)ipc4->r.type);

	return ipc_pipeline_new(ipc, (ipc_pipe_new *)ipc4);
}

static int ipc4_delete_pipeline(union ipc4_message_header *ipc4)
{
	struct ipc4_pipeline_delete *pipe;
	struct ipc *ipc = ipc_get();

	tr_err(&ipc_tr, "ipc4 delete pipeline %x:", (uint32_t)ipc4->r.type);

	pipe = (struct ipc4_pipeline_delete *)ipc4;
	return ipc_pipeline_free(ipc, pipe->header.r.instance_id);
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
		tr_err(&ipc_tr, "ipc: comp %d pipeline not found",
		       pcm_dev->cd->pipeline->comp_id);
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

static int ipc4_set_pipeline_state(union ipc4_message_header *ipc4)
{
	struct ipc4_pipeline_set_state state;
	struct ipc_comp_dev *pcm_dev;
	struct ipc_comp_dev *host;
	struct ipc *ipc = ipc_get();
	uint32_t cmd, id;
	int ret;

	state.header.dat = ipc4->dat;
	id = state.header.r.ppl_id;
	cmd = state.header.r.ppl_state;

	tr_err(&ipc_tr, "ipc4 set pipeline state %x:", cmd);

	/* get the pcm_dev */
	pcm_dev = ipc_get_comp_by_ppl_id(ipc, COMP_TYPE_PIPELINE, id);
	if (!pcm_dev) {
		tr_err(&ipc_tr, "ipc: comp %d not found", id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	if (pcm_dev->pipeline->source_comp->direction == SOF_IPC_STREAM_PLAYBACK)
		host = ipc_get_comp_by_id(ipc, pcm_dev->pipeline->source_comp->ipc_config.id);
	else
		host = ipc_get_comp_by_id(ipc, pcm_dev->pipeline->sink_comp->ipc_config.id);

	if (!host) {
		tr_err(&ipc_tr, "ipc: comp host not found", 
			pcm_dev->pipeline->source_comp->ipc_config.id);
		return IPC4_INVALID_RESOURCE_ID;
	}

	/* check core */
	if (!cpu_is_me(host->core))
		return ipc_process_on_core(host->core);

	switch (cmd) {
	case SOF_IPC4_PIPELINE_STATE_RUNNING:
		cmd = COMP_TRIGGER_START;

		ret = ipc4_pcm_params(host);
		if (ret < 0)
			return IPC4_INVALID_REQUEST;
		break;
	case SOF_IPC4_PIPELINE_STATE_RESET:
		cmd = COMP_TRIGGER_STOP;
		break;
	case SOF_IPC4_PIPELINE_STATE_PAUSED:
		if (pcm_dev->pipeline->status == COMP_STATE_INIT)
			return ipc_pipeline_complete(ipc, id);
		else
			cmd = COMP_TRIGGER_PAUSE;

		break;
	case SOF_IPC4_PIPELINE_STATE_EOS:
		cmd = COMP_TRIGGER_RELEASE;
		break;
	/* special case- TODO */
	case SOF_IPC4_PIPELINE_STATE_SAVED:
	case SOF_IPC4_PIPELINE_STATE_ERROR_STOP:
		return 0;
	default:
		tr_err(&ipc_tr, "ipc: invalid trigger cmd 0x%x",cmd); 
		return IPC4_INVALID_REQUEST;
	}

	/* trigger the component */
	ret = pipeline_trigger(host->cd->pipeline, host->cd, cmd);
	if (ret < 0) {
		tr_err(&ipc_tr, "ipc: comp %d trigger 0x%x failed %d", id, cmd, ret); 
		ret = IPC4_PIPELINE_STATE_NOT_SET;
	}

	return ret;
}

static int ipc4_process_glb_message(union ipc4_message_header *ipc4)
{
	uint32_t type;
	int ret = 0;

	type = ipc4->r.type;

	switch (type) {
	case SOF_IPC4_GLB_BOOT_CONFIG:
	case SOF_IPC4_GLB_ROM_CONTROL:
	case SOF_IPC4_GLB_IPCGATEWAY_CMD:
	case SOF_IPC4_GLB_PERF_MEASUREMENTS_CMD:
	case SOF_IPC4_GLB_CHAIN_DMA:
	case SOF_IPC4_GLB_LOAD_MULTIPLE_MODULES:
	case SOF_IPC4_GLB_UNLOAD_MULTIPLE_MODULES:
		tr_err(&ipc_tr, "not implemented ipc message type %d", type);
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
		break;

	/* Loads library (using Code Load or HD/A Host Output DMA) */
	case SOF_IPC4_GLB_LOAD_LIBRARY:
	case SOF_IPC4_GLB_INTERNAL_MESSAGE:
		tr_err(&ipc_tr, "not implemented ipc message type %d", type);
		break;

	/* Notification (FW to SW driver) */
	case SOF_IPC4_GLB_NOTIFICATION:
		tr_err(&ipc_tr, "not implemented ipc message type %d", type);
		break;

	default:
		tr_err(&ipc_tr, "unsupported ipc message type %d", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * Module IPC operations.
 */

static int ipc4_init_module(union ipc4_message_header *ipc4)
{
	struct ipc4_module_init_instance module;
	struct sof_ipc_comp comp;
	struct comp_dev*dev;

	module.header.dat = *ipc_to_hdr(ipc4);
	module.data.dat = *(ipc_to_hdr(ipc4) + 1);

	tr_err(&ipc_tr, "ipc4_init_module %x : %x", (uint32_t)module.header.r.module_id,
					(uint32_t)module.header.r.instance_id);

	memset(&comp, 0 , sizeof(comp));
	comp.id = IPC4_COMP_ID(module.header.r.module_id, module.header.r.instance_id);
	comp.pipeline_id = module.data.r.ppl_instance_id;
	comp.core = module.data.r.core_id;
	dev = comp_new(&comp);
	if (!dev)
		return IPC4_MOD_NOT_INITIALIZED;

	return 0;
}

static int ipc4_bind_module(union ipc4_message_header *ipc4)
{
	struct ipc4_module_bind_unbind bu;
	struct ipc *ipc = ipc_get();
	uint32_t comp[2];

	bu.header.dat = *ipc_to_hdr(ipc4);
	bu.data.dat = *(ipc_to_hdr(ipc4) + 1);

	tr_err(&ipc_tr, "ipc4_bind_module %x : %x with %x : %x", (uint32_t)bu.header.r.module_id,
					(uint32_t)bu.header.r.instance_id, (uint32_t)bu.data.r.dst_module_id,
					(uint32_t)bu.data.r.dst_instance_id);

	comp[0] = IPC4_COMP_ID(bu.header.r.module_id, bu.header.r.instance_id);
	comp[1] = IPC4_COMP_ID(bu.data.r.dst_module_id, bu.data.r.dst_instance_id);

	return ipc_comp_connect(ipc, comp);
}

static int ipc4_unbind_module(union ipc4_message_header *ipc4)
{
	return 0;
}

static int ipc4_set_large_config_module(union ipc4_message_header *ipc4)
{
	struct ipc4_module_large_config config;
	struct comp_dev *dev;
	int comp_id;
	int ret;

	config.header.dat = *ipc_to_hdr(ipc4);
	config.data.dat = *(ipc_to_hdr(ipc4) + 1);

	comp_id = IPC4_COMP_ID(config.header.r.module_id, config.header.r.instance_id);
	dev = ipc4_get_comp_dev(comp_id);
	if (!dev)
		return IPC4_MOD_INVALID_ID;

	ret = dev->drv->ops.cmd(dev, config.data.r.large_param_id, (void *)MAILBOX_HOSTBOX_BASE,
				config.data.dat);

	return ret;
}

static int ipc4_process_module_message(union ipc4_message_header *ipc4)
{
	uint32_t type;
	int ret = 0;

	type = ipc4->r.type;

	switch (type) {
	case SOF_IPC4_MOD_INIT_INSTANCE:
		ret = ipc4_init_module(ipc4);
		break;
	case SOF_IPC4_MOD_CONFIG_GET:
		break;
	case SOF_IPC4_MOD_CONFIG_SET:
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_GET:
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_SET:
		ret = ipc4_set_large_config_module(ipc4);
		break;
	case SOF_IPC4_MOD_BIND:
		ret = ipc4_bind_module(ipc4);
		break;
	case SOF_IPC4_MOD_UNBIND:
		ret = ipc4_unbind_module(ipc4);
		break;
	case SOF_IPC4_MOD_SET_DX:
		break;
	case SOF_IPC4_MOD_SET_D0IX:
		break;
	case SOF_IPC4_MOD_ENTER_MODULE_RESTORE:
		break;
	case SOF_IPC4_MOD_EXIT_MODULE_RESTORE:
		break;
	case SOF_IPC4_MOD_DELETE_INSTANCE:
		break;
	default:
		break;
	}

	return ret;
}

ipc_cmd_hdr *mailbox_validate(void)
{
	ipc_cmd_hdr *hdr = ipc_get()->comp_data;

	/* TODO validate message */
	return hdr;
}

/*
 * Most ABI 4 messages use compact format - keep logic simpler
 * and handle all in IPC command.
 */
static uint32_t msg_in[2]; /* local copy of current message from host header */
static uint32_t msg_out[2]; /* local copy of current message to host header */
static struct ipc_msg msg_reply;
static uint32_t msg_data;

ipc_cmd_hdr *ipc_compact_read_msg(void)
{
	ipc_cmd_hdr *hdr = (ipc_cmd_hdr*)msg_in;
	int words;

	words = ipc_platform_compact_read_msg(hdr, 2);
	if (!words)
		return mailbox_validate();

	return ipc_to_hdr(msg_in);
}

ipc_cmd_hdr *ipc_process_msg(struct ipc_msg *msg)
{
	msg_out[0] = msg->header;
	msg_out[1] = *(uint32_t *)msg->tx_data;

	/* the first uint of msg data is sent by ipc data register for ipc4 */
	msg->tx_size -= sizeof(uint32_t);
	if (msg->tx_size)
		mailbox_dspbox_write(0, (uint32_t *)msg->tx_data + 1, msg->tx_size);

	return ipc_to_hdr(msg_out);
}

void ipc_boot_complete_msg(ipc_cmd_hdr *header, uint32_t *data)
{
	*header = SOF_IPC4_FW_READY;
	*data = 0;
}

void ipc_cmd(ipc_cmd_hdr *_hdr)
{
	union ipc4_message_header *in = ipc_from_hdr(_hdr);
	enum ipc4_message_target target;
	int err = -EINVAL;

	if (!in)
		return;

	target = in->r.msg_tgt;

	switch (target) {
	case SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG:
		err = ipc4_process_glb_message(in);
		break;
	case SOF_IPC4_MESSAGE_TARGET_MODULE_MSG:
		err = ipc4_process_module_message(in);
		break;
	default:
		/* should not reach here as we only have 2 message tyes */
		tr_err(&ipc_tr, "ipc4: invalid target %d", target);
		err = IPC4_UNKNOWN_MESSAGE_TYPE;
	}

	if (err) {
		tr_err(&ipc_tr, "ipc4: %d failed ....", target);
	}

	/* FW sends a ipc message to host if request bit is set*/
	if (in->r.rsp == SOF_IPC4_MESSAGE_DIR_MSG_REQUEST) {
		struct ipc4_message_reply reply;

		/* copy contents of message received */
		reply.header.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REPLY;
		reply.header.r.status = err;
		reply.header.r.msg_tgt = in->r.msg_tgt;
		reply.header.r.type = in->r.type;
		reply.data.dat = 0;

		msg_reply.header = reply.header.dat;
		msg_reply.tx_data = &msg_data;
		msg_reply.tx_size = sizeof(msg_data);
		ipc_msg_send(&msg_reply, &reply.data.dat, true);
	}
}
