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
#include <reef/stream.h>
#include <reef/dai.h>
#include <reef/dma.h>
#include <reef/alloc.h>
#include <reef/wait.h>
#include <reef/trace.h>
#include <reef/ssp.h>
#include <platform/interrupt.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <platform/dma.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>
#include <uapi/intel-ipc.h>
#include <reef/intel-ipc.h>

extern struct ipc *_ipc;

static void do_cmd(void)
{
#if 0
	struct intel_ipc_data *iipc = ipc_get_drvdata(_ipc);
	uint32_t ipcxh, status;

	trace_ipc("Cmd");
	//trace_value(_ipc->host_msg);

	status = ipc_cmd();
	_ipc->host_pending = 0;

	/* clear BUSY bit and set DONE bit - accept new messages */
	ipcxh = shim_read(SHIM_IPCXH);
	ipcxh &= ~SHIM_IPCXH_BUSY;
	ipcxh |= SHIM_IPCXH_DONE | status;
	shim_write(SHIM_IPCXH, ipcxh);

	// TODO: signal audio work to enter D3 in normal context
	/* are we about to enter D3 ? */
	if (iipc->pm_prepare_D3) {
		while (1)
			wait_for_interrupt(0);
	}

	/* unmask busy interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_BUSY);
#endif
}

void do_notify(void)
{
	tracev_ipc("Not");
#if 0
	/* clear DONE bit - tell Host we have completed */
	shim_write(SHIM_IPCDH, shim_read(SHIM_IPCDH) & ~SHIM_IPCDH_DONE);

	/* unmask Done interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DONE);
#endif
}

/* test code to check working IRQ */
static void irq_handler(void *arg)
{
#if 0
	uint32_t isr;

	tracev_ipc("IRQ");

	/* Interrupt arrived, check src */
	isr = shim_read(SHIM_ISRD);

	if (isr & SHIM_ISRD_DONE) {

		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_DONE);
		interrupt_clear(IPC_INTERUPT);
		do_notify();
	}

	if (isr & SHIM_ISRD_BUSY) {

		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_BUSY);
		interrupt_clear(IPC_INTERUPT);

		/* place message in Q and process later */
		_ipc->host_msg = shim_read(SHIM_IPCXL);
		_ipc->host_pending = 1;
	}
#endif
}

/* process current message */
int ipc_process_msg_queue(void)
{
	if (_ipc->host_pending)
		do_cmd();
	return 0;
}

/* Send stream command */
// TODO Queue notifications and send seqentially
int ipc_stream_send_notification(int stream_id)
{
#if 0
	uint32_t header;
	struct ipc_intel_ipc_stream_get_position msg;

	/* cant send nofication when one is in progress */
	if (shim_read(SHIM_IPCDH) & (SHIM_IPCDH_BUSY | SHIM_IPCDH_DONE))
		return 0;

	msg.position = 100;/* this position looks not used in driver, it only care the pos registers */
	msg.fw_cycle_count = 0;
	mailbox_outbox_write(0, &msg, sizeof(msg));

	header = IPC_INTEL_GLB_TYPE(IPC_INTEL_GLB_STREAM_MESSAGE) |
		IPC_INTEL_STR_TYPE(IPC_INTEL_STR_NOTIFICATION) |
		IPC_INTEL_STG_TYPE(IPC_POSITION_CHANGED) |
		IPC_INTEL_STR_ID(stream_id);

	/* now interrupt host to tell it we have message sent */
	shim_write(SHIM_IPCDL, header);
	shim_write(SHIM_IPCDH, SHIM_IPCDH_BUSY);
#endif
	return 0;
}

int ipc_send_msg(struct ipc_msg *msg)
{

	return 0;
}

int platform_ipc_init(struct ipc *ipc)
{
	struct intel_ipc_data *iipc;
//	uint32_t imrd;

	_ipc = ipc;

	/* init ipc data */
	iipc = rzalloc(RZONE_DEV, RMOD_SYS, sizeof(struct intel_ipc_data));
	ipc_set_drvdata(_ipc, iipc);

	/* allocate page table buffer */
	iipc->page_table = rballoc(RZONE_DEV, RMOD_SYS,
		IPC_INTEL_PAGE_TABLE_SIZE);
	if (iipc->page_table)
		bzero(iipc->page_table, IPC_INTEL_PAGE_TABLE_SIZE);

	/* dma */
	iipc->dmac0 = dma_get(DMA_ID_DMAC0);

	/* PM */
	iipc->pm_prepare_D3 = 0;

	/* configure interrupt */
	interrupt_register(IPC_INTERUPT, irq_handler, NULL);
	interrupt_enable(IPC_INTERUPT);

#if 0
	/* Unmask Busy and Done interrupts */
	imrd = shim_read(SHIM_IMRD);
	imrd &= ~(SHIM_IMRD_BUSY | SHIM_IMRD_DONE);
	shim_write(SHIM_IMRD, imrd);
#endif
	return 0;
}


