// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/debug.h>
#include <sof/timer.h>
#include <sof/interrupt.h>
#include <sof/ipc.h>
#include <sof/mailbox.h>
#include <sof/sof.h>
#include <sof/stream.h>
#include <sof/dai.h>
#include <sof/dma.h>
#include <sof/alloc.h>
#include <sof/wait.h>
#include <sof/trace.h>

#include <platform/interrupt.h>
#include <platform/mailbox.h>
#include <platform/dma.h>
#include <platform/platform.h>
#include <platform/mu.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <uapi/ipc/header.h>

extern struct ipc *_ipc;

/* No private data for IPC */

static void do_notify(void)
{
	uint32_t flags;
	struct ipc_msg *msg;

	spin_lock_irq(&_ipc->lock, flags);
	msg = _ipc->shared_ctx->dsp_msg;
	if (msg == NULL)
		goto out;

	tracev_ipc("ipc: not rx -> 0x%x", msg->header);

	/* copy the data returned from DSP */
	if (msg->rx_size && msg->rx_size < SOF_IPC_MSG_MAX_SIZE)
		mailbox_dspbox_read(msg->rx_data, SOF_IPC_MSG_MAX_SIZE,
				    0, msg->rx_size);

	/* any callback ? */
	if (msg->cb)
		msg->cb(msg->cb_data, msg->rx_data);

	list_item_append(&msg->list, &_ipc->shared_ctx->empty_list);

out:
	spin_unlock_irq(&_ipc->lock, flags);

	/* unmask GP interrupt #1 */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(1), 0);
}

static void irq_handler(void *arg)
{
	uint32_t ctrl;
	uint32_t status;
	uint32_t msg = 0;

	/* Interrupt arrived, check src */
	ctrl = imx_mu_read(IMX_MU_xCR);
	status = imx_mu_read(IMX_MU_xSR);

	tracev_ipc("ipc: irq isr 0x%x", status);

	/* reply message(done) from host */
	if (status & IMX_MU_xSR_GIPn(1)) {
		/* Disable GP interrupt #1 */
		imx_mu_xcr_rmw(0, IMX_MU_xCR_GIEn(1));

		/* Clear GP pending interrupt #1 */
		imx_mu_xsr_rmw(IMX_MU_xSR_GIPn(1), 0);

		interrupt_clear(PLATFORM_IPC_INTERRUPT);
		do_notify();
	}

	/* new message from host */
	if (status & IMX_MU_xSR_GIPn(0)) {

		/* Disable GP interrupt #0 */
		imx_mu_xcr_rmw(0, IMX_MU_xCR_GIEn(0));

		/* Clear GP pending interrupt #0 */
		imx_mu_xsr_rmw(IMX_MU_xSR_GIPn(0), 0);

		interrupt_clear(PLATFORM_IPC_INTERRUPT);
		/* TODO: place message in Q and process later */
		/* It's not Q ATM, may overwrite */
		if (_ipc->host_pending) {
			trace_ipc_error("ipc: dropping msg 0x%x", msg);
			trace_ipc_error(" isr 0x%x ctrl 0x%x ipcxh 0x%x",
					status, ctrl);
		} else {
			_ipc->host_pending = 1;
			ipc_schedule_process(_ipc);
		}
	}
}

void ipc_platform_do_cmd(struct ipc *ipc)
{
	/* Use struct ipc_data *iipc = ipc_get_drvdata(ipc); if needed */
	struct sof_ipc_reply reply;
	int32_t err;

	/* perform command and return any error */
	err = ipc_cmd();
	if (err > 0) {
		goto done; /* reply created and copied by cmd() */
	} else {
		/* send std error reply */
		reply.error = err;
	}

	/* send std error/ok reply */
	reply.hdr.cmd = SOF_IPC_GLB_REPLY;
	reply.hdr.size = sizeof(reply);
	mailbox_hostbox_write(0, &reply, sizeof(reply));

done:
	ipc->host_pending = 0;

	/* enable GP interrupt #0 - accept new messages */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(0), 0);

	/* request GP interrupt #0 - notify host that reply is ready */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIRn(0), 0);

	// TODO: signal audio work to enter D3 in normal context
	/* are we about to enter D3 ? */
	if (ipc->pm_prepare_D3) {
		while (1)
			wait_for_interrupt(0);
	}
}

void ipc_platform_send_msg(struct ipc *ipc)
{
	struct ipc_msg *msg;
	uint32_t flags;

	spin_lock_irq(&ipc->lock, flags);

	/* any messages to send ? */
	if (list_is_empty(&ipc->shared_ctx->msg_list)) {
		ipc->shared_ctx->dsp_pending = 0;
		goto out;
	}

	/* can't send notification when one is in progress */
	if (imx_mu_read(IMX_MU_xCR) & IMX_MU_xCR_GIRn(1))
		goto out;

	/* now send the message */
	msg = list_first_item(&ipc->shared_ctx->msg_list, struct ipc_msg,
			      list);
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	list_item_del(&msg->list);
	ipc->shared_ctx->dsp_msg = msg;
	tracev_ipc("ipc: msg tx -> 0x%x", msg->header);

	/* now interrupt host to tell it we have sent a message */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIRn(1), 0);

	list_item_append(&msg->list, &ipc->shared_ctx->empty_list);

out:
	spin_unlock_irq(&ipc->lock, flags);
}

int platform_ipc_init(struct ipc *ipc)
{
	_ipc = ipc;

	ipc_set_drvdata(_ipc, NULL);

	/* schedule */
	schedule_task_init(&_ipc->ipc_task, SOF_SCHEDULE_EDF, SOF_TASK_PRI_IPC,
			   ipc_process_task, _ipc, 0, 0);

#ifdef CONFIG_HOST_PTABLE
	/* allocate page table buffer */
	iipc->page_table = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
		PLATFORM_PAGE_TABLE_SIZE);
	if (iipc->page_table)
		bzero(iipc->page_table, PLATFORM_PAGE_TABLE_SIZE);
#endif

	/* configure interrupt */
	interrupt_register(PLATFORM_IPC_INTERRUPT, IRQ_AUTO_UNMASK,
			   irq_handler, NULL);
	interrupt_enable(PLATFORM_IPC_INTERRUPT);

	/* enable GP #0 for Host -> DSP message notification
	 * enable GP #1 for DSP -> Host message notification
	 */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(0) | IMX_MU_xCR_GIEn(1), 0);

	return 0;
}

