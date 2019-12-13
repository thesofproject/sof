// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/debug/panic.h>
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
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

extern struct ipc *_ipc;

struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

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

		/* unmask GP interrupt #1 */
		imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(1), 0);
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

void ipc_platform_send_msg(void)
{
	struct ipc_msg *msg;
	uint32_t flags;

	spin_lock_irq(_ipc->lock, flags);

	/* any messages to send ? */
	if (list_is_empty(&_ipc->shared_ctx->msg_list))
		goto out;

	/* can't send notification when one is in progress */
	if (imx_mu_read(IMX_MU_xCR) & IMX_MU_xCR_GIRn(1))
		goto out;

	/* now send the message */
	msg = list_first_item(&_ipc->shared_ctx->msg_list, struct ipc_msg,
			      list);
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	list_item_del(&msg->list);
	tracev_ipc("ipc: msg tx -> 0x%x", msg->header);

	/* now interrupt host to tell it we have sent a message */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIRn(1), 0);

	list_item_append(&msg->list, &_ipc->shared_ctx->empty_list);

out:
	spin_unlock_irq(_ipc->lock, flags);
}

#if CONFIG_HOST_PTABLE
struct ipc_data_host_buffer *ipc_platform_get_host_buffer(struct ipc *ipc)
{
	struct ipc_data *iipc = ipc_get_drvdata(ipc);

	return &iipc->dh_buffer;
}
#endif

int platform_ipc_init(struct ipc *ipc)
{
#if CONFIG_HOST_PTABLE
	struct ipc_data *iipc;

	iipc = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(*iipc));
	if (!iipc) {
		trace_ipc_error("Unable to allocate IPC private data");
		return -ENOMEM;
	}
	ipc_set_drvdata(ipc, iipc);
#else
	ipc_set_drvdata(ipc, NULL);
#endif
	_ipc = ipc;

	/* schedule */
	schedule_task_init(&_ipc->ipc_task, SOF_SCHEDULE_EDF, SOF_TASK_PRI_IPC,
			   ipc_platform_do_cmd, ipc_platform_complete_cmd, _ipc,
			   0, 0);

#if CONFIG_HOST_PTABLE
	/* allocate page table buffer */
	iipc->dh_buffer.page_table = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
					     PLATFORM_PAGE_TABLE_SIZE);
	if (iipc->dh_buffer.page_table)
		bzero(iipc->dh_buffer.page_table, PLATFORM_PAGE_TABLE_SIZE);
	iipc->dh_buffer.dmac = dma_get(DMA_DIR_HMEM_TO_LMEM, 0, DMA_DEV_HOST,
				       DMA_ACCESS_SHARED);
	if (!iipc->dh_buffer.dmac) {
		trace_ipc_error("Unable to find DMA for host page table");
		panic(SOF_IPC_PANIC_IPC);
	}
#endif

	/* configure interrupt */
	interrupt_register(PLATFORM_IPC_INTERRUPT, irq_handler, _ipc);
	interrupt_enable(PLATFORM_IPC_INTERRUPT, _ipc);

	/* enable GP #0 for Host -> DSP message notification
	 * enable GP #1 for DSP -> Host message notification
	 */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(0) | IMX_MU_xCR_GIEn(1), 0);

	return 0;
}

