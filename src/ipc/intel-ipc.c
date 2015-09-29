/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * Intel IPC.
 */

#include <reef/debug.h>
#include <reef/timer.h>
#include <reef/interrupt.h>
#include <reef/ipc.h>
#include <reef/mailbox.h>
#include <reef/reef.h>
#include <platform/interrupt.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <uapi/intel-ipc.h>

static int ipc = 1000;

#if 0
static int hsw_process_reply(struct sst_hsw *hsw, u32 header)
{
	struct ipc_message *msg;
	u32 reply = msg_get_global_reply(header);

	trace_ipc_reply("processing -->", header);

	msg = sst_ipc_reply_find_msg(&hsw->ipc, header);
	if (msg == NULL) {
		trace_ipc_error("error: can't find message header", header);
		return -EIO;
	}

	/* first process the header */
	switch (reply) {
	case IPC_INTEL_GLB_REPLY_PENDING:
		trace_ipc_pending_reply("received", header);
		msg->pending = true;
		hsw->ipc.pending = true;
		return 1;
	case IPC_INTEL_GLB_REPLY_SUCCESS:
		if (msg->pending) {
			trace_ipc_pending_reply("completed", header);
			sst_dsp_inbox_read(hsw->dsp, msg->rx_data,
				msg->rx_size);
			hsw->ipc.pending = false;
		} else {
			/* copy data from the DSP */
			sst_dsp_outbox_read(hsw->dsp, msg->rx_data,
				msg->rx_size);
		}
		break;
	/* these will be rare - but useful for debug */
	case IPC_INTEL_GLB_REPLY_UNKNOWN_MESSAGE_TYPE:
		trace_ipc_error("error: unknown message type", header);
		msg->errno = -EBADMSG;
		break;
	case IPC_INTEL_GLB_REPLY_OUT_OF_RESOURCES:
		trace_ipc_error("error: out of resources", header);
		msg->errno = -ENOMEM;
		break;
	case IPC_INTEL_GLB_REPLY_BUSY:
		trace_ipc_error("error: reply busy", header);
		msg->errno = -EBUSY;
		break;
	case IPC_INTEL_GLB_REPLY_FAILURE:
		trace_ipc_error("error: reply failure", header);
		msg->errno = -EINVAL;
		break;
	case IPC_INTEL_GLB_REPLY_STAGE_UNINITIALIZED:
		trace_ipc_error("error: stage uninitialized", header);
		msg->errno = -EINVAL;
		break;
	case IPC_INTEL_GLB_REPLY_NOT_FOUND:
		trace_ipc_error("error: reply not found", header);
		msg->errno = -EINVAL;
		break;
	case IPC_INTEL_GLB_REPLY_SOURCE_NOT_STARTED:
		trace_ipc_error("error: source not started", header);
		msg->errno = -EINVAL;
		break;
	case IPC_INTEL_GLB_REPLY_INVALID_REQUEST:
		trace_ipc_error("error: invalid request", header);
		msg->errno = -EINVAL;
		break;
	case IPC_INTEL_GLB_REPLY_ERROR_INVALID_PARAM:
		trace_ipc_error("error: invalid parameter", header);
		msg->errno = -EINVAL;
		break;
	default:
		trace_ipc_error("error: unknown reply", header);
		msg->errno = -EINVAL;
		break;
	}

	/* update any stream states */
	if (msg_get_global_type(header) == IPC_INTEL_GLB_STREAM_MESSAGE)
		hsw_stream_update(hsw, msg);

	/* wake up and return the error if we have waiters on this message ? */
	list_del(&msg->list);
	sst_ipc_tx_msg_reply_complete(&hsw->ipc, msg);

	return 1;
}

#endif

static void ipc_get_fw_version(void)
{
	struct sst_hsw_ipc_fw_ready ready;
	
	ready.inbox_offset = MAILBOX_INBOX_OFFSET;
	ready.outbox_offset = MAILBOX_OUTBOX_OFFSET;
	ready.inbox_size = MAILBOX_INBOX_SIZE;
	ready.outbox_size = MAILBOX_OUTBOX_SIZE;
	ready.fw_info_size = 0;
	//ready.fw_info[IPC_MAX_MAILBOX_BYTES - 5 * sizeof(uint32_t)];

	mailbox_outbox_write(0, &ready, sizeof(ready));	
}

static inline uint32_t msg_get_global_type(uint32_t msg)
{
	return (msg & IPC_INTEL_GLB_TYPE_MASK) >> IPC_INTEL_GLB_TYPE_SHIFT;
}

static void ipc_cmd(void)
{
	uint32_t type, header;

	header = shim_read(SHIM_IPCD);
	type = msg_get_global_type(header);

	switch (type) {
	case IPC_INTEL_GLB_GET_FW_VERSION:
		
	case IPC_INTEL_GLB_ALLOCATE_STREAM:
	case IPC_INTEL_GLB_FREE_STREAM:
	case IPC_INTEL_GLB_GET_FW_CAPABILITIES:
	case IPC_INTEL_GLB_REQUEST_DUMP:
	case IPC_INTEL_GLB_GET_DEVICE_FORMATS:
	case IPC_INTEL_GLB_SET_DEVICE_FORMATS:
	case IPC_INTEL_GLB_ENTER_DX_STATE:
	case IPC_INTEL_GLB_GET_MIXER_STREAM_INFO:
	case IPC_INTEL_GLB_MAX_IPC_MESSAGE_TYPE:
	case IPC_INTEL_GLB_RESTORE_CONTEXT:
	case IPC_INTEL_GLB_SHORT_REPLY:
		dbg_val(type);
		break;
	case IPC_INTEL_GLB_STREAM_MESSAGE:
//		handled = hsw_stream_message(hsw, header);
		break;
	case IPC_INTEL_GLB_DEBUG_LOG_MESSAGE:
//		handled = hsw_log_message(hsw, header);
		break;
	case IPC_INTEL_GLB_MODULE_OPERATION:
//		handled = hsw_module_message(hsw, header);
		break;
	default:
		
		break;
	}
}



static void do_cmd(void)
{
	uint32_t val;
	
	ipc_cmd();

	/* clear BUSY bit and set DONE bit - accept new messages */
	val = shim_read(SHIM_IPCX);
	val &= ~SHIM_IPCX_BUSY;
	val |= SHIM_IPCX_DONE;
	shim_write(SHIM_IPCX, val);

	/* unmask busy interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_BUSY);
}

static void do_notify(void)
{
	dbg();

	/* clear DONE bit - tell Host we have completed */
	shim_write(SHIM_IPCD, shim_read(SHIM_IPCD) & ~SHIM_IPCD_DONE);

	/* unmask Done interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DONE);
}

/* test code to check working IRQ */
static void irq_handler(void *arg)
{
	uint32_t isr;

	ipc++;
	dbg_val(ipc);

	/* Interrupt arrived, check src */
	isr = shim_read(SHIM_ISRD);

	if (isr & SHIM_ISRD_DONE) {

		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_DONE);
		do_notify();
	}

	if (isr & SHIM_ISRD_BUSY) {
		
		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_BUSY);
		do_cmd();
	}

	interrupt_clear(IRQ_NUM_EXT_IA);
}

int platform_ipc_init(struct ipc *context)
{
	interrupt_register(IRQ_NUM_EXT_IA, irq_handler, context);
	interrupt_enable(IRQ_NUM_EXT_IA);

	return 0;
}
