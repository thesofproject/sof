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

static inline uint32_t msg_get_global_type(uint32_t msg)
{
	return (msg & IPC_INTEL_GLB_TYPE_MASK) >> IPC_INTEL_GLB_TYPE_SHIFT;
}

static uint32_t ipc_fw_version(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_fw_caps(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_alloc(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_free(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_stream_info(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_dump(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_device_get_formats(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_device_set_formats(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_save(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_context_restore(uint32_t header)
{
	dbg();

	return IPC_INTEL_GLB_REPLY_SUCCESS;
}

static uint32_t ipc_cmd(void)
{
	uint32_t type, header;

	header = shim_read(SHIM_IPCD);
	type = msg_get_global_type(header);

	switch (type) {
	case IPC_INTEL_GLB_GET_FW_VERSION:
		return ipc_fw_version(header);
	case IPC_INTEL_GLB_ALLOCATE_STREAM:
		return ipc_stream_alloc(header);
	case IPC_INTEL_GLB_FREE_STREAM:
		return ipc_stream_free(header);
	case IPC_INTEL_GLB_GET_FW_CAPABILITIES:
		return ipc_fw_caps(header);
	case IPC_INTEL_GLB_REQUEST_DUMP:
		return ipc_dump(header);
	case IPC_INTEL_GLB_GET_DEVICE_FORMATS:
		return ipc_device_get_formats(header);
	case IPC_INTEL_GLB_SET_DEVICE_FORMATS:
		return ipc_device_set_formats(header);
	case IPC_INTEL_GLB_ENTER_DX_STATE:
		return ipc_context_save(header);
	case IPC_INTEL_GLB_GET_MIXER_STREAM_INFO:
		return ipc_stream_info(header);
	case IPC_INTEL_GLB_RESTORE_CONTEXT:
		return ipc_context_restore(header);
	default:
		return IPC_INTEL_GLB_REPLY_UNKNOWN_MESSAGE_TYPE;
	}
}

static void do_cmd(void)
{
	uint32_t status, ipcx;
	
	status = ipc_cmd();

	/* clear BUSY bit and set DONE bit - accept new messages */
	ipcx = shim_read(SHIM_IPCX);
	ipcx &= ~SHIM_IPCX_BUSY;
	ipcx |= SHIM_IPCX_DONE | status;
	shim_write(SHIM_IPCX, ipcx);

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
