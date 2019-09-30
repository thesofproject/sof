// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/drivers/interrupt.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/mu.h>
#include <sof/lib/alloc.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/wait.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <ipc/header.h>
#include <ipc/topology.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

extern struct ipc *_ipc;

/* No private data for IPC */

static void do_notify(void)
{
	uint32_t flags;
	struct ipc_msg *msg;

	spin_lock_irq(_ipc->lock, flags);
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
	spin_unlock_irq(_ipc->lock, flags);

	/* unmask GP interrupt #1 */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(1), 0);
}

static void irq_handler(void *arg)
{
	uint32_t status;

	/* Interrupt arrived, check src */
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

		ipc_schedule_process(_ipc);
	}
}

static enum task_state ipc_platform_do_cmd(void *data)
{
	struct sof_ipc_cmd_hdr *hdr;
	/* Use struct ipc_data *iipc = ipc_get_drvdata(ipc); if needed */

	/* perform command */
	hdr = mailbox_validate();
	ipc_cmd(hdr);

	return SOF_TASK_STATE_COMPLETED;
}

static void ipc_platform_complete_cmd(void *data)
{
	struct ipc *ipc = data;

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

	spin_lock_irq(ipc->lock, flags);

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
	spin_unlock_irq(ipc->lock, flags);
}

int platform_ipc_init(struct ipc *ipc)
{
	_ipc = ipc;

	ipc_set_drvdata(_ipc, NULL);

	/* schedule */
	schedule_task_init(&_ipc->ipc_task, SOF_SCHEDULE_EDF, SOF_TASK_PRI_IPC,
			   ipc_platform_do_cmd, ipc_platform_complete_cmd, _ipc,
			   0, 0);

#if CONFIG_HOST_PTABLE
	/* allocate page table buffer */
	iipc->page_table = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
		PLATFORM_PAGE_TABLE_SIZE);
	if (iipc->page_table)
		bzero(iipc->page_table, PLATFORM_PAGE_TABLE_SIZE);
#endif

	/* configure interrupt */
	interrupt_register(PLATFORM_IPC_INTERRUPT, IRQ_AUTO_UNMASK,
			   irq_handler, _ipc);
	interrupt_enable(PLATFORM_IPC_INTERRUPT, _ipc);

	/* enable GP #0 for Host -> DSP message notification
	 * enable GP #1 for DSP -> Host message notification
	 */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(0) | IMX_MU_xCR_GIEn(1), 0);

	return 0;
}

