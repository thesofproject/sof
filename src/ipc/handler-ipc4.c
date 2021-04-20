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
#include <sof/drivers/ipc.h>
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

struct ipc_data {
	struct ipc_msg msg;
	uint32_t msg_dat[2];
};

/*
 * Global IPC Operations.
 */
static int ipc4_create_pipeline(uint32_t *msg)
{
	tr_err(&ipc_tr, "ipc4 create pipeline %x:", *msg);

	return 0;
}

static int ipc4_delete_pipeline(uint32_t *msg)
{
	tr_err(&ipc_tr, "ipc4 delete pipeline %x:", *msg);

	return 0;
}

static int ipc4_set_pipeline_state(uint32_t *msg)
{
	tr_err(&ipc_tr, "ipc4 set pipeline state %x:", *msg);

	return 0;
}

static int ipc4_process_glb_message(uint32_t *msg)
{
	union ipc4_message_header ipc4;
	uint32_t type;
	int ret = 0;

	ipc4.dat = msg[0];
	type = ipc4.r.type;

	switch (type) {
	case SOF_IPC4_GLB_BOOT_CONFIG:
	case SOF_IPC4_GLB_ROM_CONTROL:
	case SOF_IPC4_GLB_IPCGATEWAY_CMD:
	case SOF_IPC4_GLB_START_RTOS_EDF_TASK:
	case SOF_IPC4_GLB_STOP_RTOS_EDF_TASK:
	case SOF_IPC4_GLB_PERF_MEASUREMENTS_CMD:
	case SOF_IPC4_GLB_CHAIN_DMA:
	case SOF_IPC4_GLB_LOAD_MULTIPLE_MODULES:
	case SOF_IPC4_GLB_UNLOAD_MULTIPLE_MODULES:
		tr_err(&ipc_tr, "not implemented ipc message type %d", type);
		break;

	/* pipeline settings */
	case SOF_IPC4_GLB_CREATE_PIPELINE:
		ret = ipc4_create_pipeline(msg);
		break;
	case SOF_IPC4_GLB_DELETE_PIPELINE:
		ret = ipc4_delete_pipeline(msg);
		break;
	case SOF_IPC4_GLB_SET_PIPELINE_STATE:
		ret = ipc4_set_pipeline_state(msg);
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

static int ipc4_init_module(uint32_t *msg)
{
	return 0;
}

static int ipc4_bind_module(uint32_t *msg)
{
	return 0;
}

static int ipc4_unbind_module(uint32_t *msg)
{
	return 0;
}

static int ipc4_set_large_config_module(uint32_t *msg)
{
	return 0;
}

static int ipc4_process_module_message(uint32_t *msg)
{
	union ipc4_message_header ipc4;
	uint32_t type;
	int ret = 0;

	ipc4.dat = msg[0];
	type = ipc4.r.type;

	switch (type) {
	case SOF_IPC4_MOD_INIT_INSTANCE:
		ret = ipc4_init_module(msg);
		break;
	case SOF_IPC4_MOD_CONFIG_GET:
		break;
	case SOF_IPC4_MOD_CONFIG_SET:
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_GET:
		break;
	case SOF_IPC4_MOD_LARGE_CONFIG_SET:
		ret = ipc4_set_large_config_module(msg);
		break;
	case SOF_IPC4_MOD_BIND:
		ret = ipc4_bind_module(msg);
		break;
	case SOF_IPC4_MOD_UNBIND:
		ret = ipc4_unbind_module(msg);
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

/*
 * Most ABI 4 messages use compact format - keep logic simpler
 * and handle all in IPC command.
 */
uint32_t *ipc_compact_read_msg(void)
{
	struct ipc *ipc = ipc_get();
	struct ipc_data *dat;

	dat = ipc_get_drvdata(ipc);
	if (!dat) {
		dat = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(*dat));
		if (!dat) {
			tr_err(&ipc_tr, "Unable to allocate IPC private data");
			return NULL;
		}
		ipc_set_drvdata(ipc, dat);
	}

	dat->msg_dat[0] = ipc_read(IPC_DIPCTDR);
	dat->msg_dat[1] = ipc_read(IPC_DIPCTDD);

	return dat->msg_dat;
}

void ipc_cmd(uint32_t *msg)
{
	union ipc4_message_header ipc4;
	enum ipc4_message_target target;
	int err = -EINVAL;

	if (!msg)
		return;

	ipc4.dat = msg[0];
	target = ipc4.r.msg_tgt;

	switch (target) {
	case SOF_IPC4_MESSAGE_TARGET_FW_GEN_MSG:
		err = ipc4_process_glb_message(msg);
		break;
	case SOF_IPC4_MESSAGE_TARGET_MODULE_MSG:
		err = ipc4_process_module_message(msg);
		break;
	default:
		/* should not reach here as we only have 2 message tyes */
		tr_err(&ipc_tr, "ipc4: invalid target %d", target);
		err = -EINVAL;
	}

	/* FW sends a ipc message to host if request bit is set*/
	if (ipc4.r.rsp == SOF_IPC4_MESSAGE_DIR_MSG_REQUEST) {
		struct ipc *ipc = ipc_get();
		struct ipc_data *dat;

		ipc4.dat = 0;
		ipc4.r.rsp = SOF_IPC4_MESSAGE_DIR_MSG_REPLY;

		dat = ipc_get_drvdata(ipc);
		dat->msg.header = ipc4.dat;
		dat->msg.header |= (err & SOF_IPC4_REPLY_STATUS_MASK);
		dat->msg.data = 0;
		dat->msg.tx_size = 0;

		list_init(&dat->msg.list);
		ipc_msg_send(&dat->msg, NULL, true);
	}
}
