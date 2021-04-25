// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/schedule.h>
#include <sof/drivers/mu.h>
#include <sof/lib/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/lib/wait.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <ipc/header.h>
#include <ipc/topology.h>
#include <ipc/trace.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 389c9186-5a7d-4ad1-a02c-a02ecdadfb33 */
DECLARE_SOF_UUID("ipc-task", ipc_task_uuid, 0x389c9186, 0x5a7d, 0x4ad1,
		 0xa0, 0x2c, 0xa0, 0x2e, 0xcd, 0xad, 0xfb, 0x33);

struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

static void irq_handler(void *arg)
{
	struct ipc *ipc = arg;
	uint32_t status;

	/* Interrupt arrived, check src */
	status = imx_mu_read(IMX_MU_xSR);

	tr_dbg(&ipc_tr, "ipc: irq isr 0x%x", status);

	/* reply message(done) from host */
	if (status & IMX_MU_xSR_GIPn(1)) {
		/* Disable GP interrupt #1 */
		imx_mu_xcr_rmw(0, IMX_MU_xCR_GIEn(1));

		/* Clear GP pending interrupt #1 */
		imx_mu_xsr_rmw(IMX_MU_xSR_GIPn(1), 0);

		interrupt_clear(PLATFORM_IPC_INTERRUPT);

		ipc->is_notification_pending = false;

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

		ipc_schedule_process(ipc);
	}
}

int ipc_platform_compact_write_msg(ipc_cmd_hdr *hdr, int words)
{
	return 0; /* number of words read - not currently used on this platform */
}

int ipc_platform_compact_read_msg(ipc_cmd_hdr *hdr, int words)
{
	return 0; /* number of words read - not currently used on this platform */
}

enum task_state ipc_platform_do_cmd(void *data)
{
	ipc_cmd_hdr *hdr;
	/* Use struct ipc_data *iipc = ipc_get_drvdata(ipc); if needed */

	/* perform command */
	hdr = mailbox_validate();
	ipc_cmd(hdr);

	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(void *data)
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

int ipc_platform_send_msg(struct ipc_msg *msg)
{
	struct ipc *ipc = ipc_get();
	int ret = 0;

	/* can't send notification when one is in progress */
	if (ipc->is_notification_pending ||
	    imx_mu_read(IMX_MU_xCR) & IMX_MU_xCR_GIRn(1)) {
		ret = -EBUSY;
		goto out;
	}

	/* now send the message */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	list_item_del(&msg->list);
	tr_dbg(&ipc_tr, "ipc: msg tx -> 0x%x", msg->header);

	ipc->is_notification_pending = true;

	/* now interrupt host to tell it we have sent a message */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIRn(1), 0);

out:

	return ret;
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

	iipc = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(*iipc));
	if (!iipc) {
		tr_err(&ipc_tr, "Unable to allocate IPC private data");
		return -ENOMEM;
	}
	ipc_set_drvdata(ipc, iipc);
#else
	ipc_set_drvdata(ipc, NULL);
#endif

	/* schedule */
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_uuid),
			       &ipc_task_ops, ipc, 0, 0);

#if CONFIG_HOST_PTABLE
	/* allocate page table buffer */
	iipc->dh_buffer.page_table = rzalloc(SOF_MEM_ZONE_SYS, 0,
					     SOF_MEM_CAPS_RAM,
					     PLATFORM_PAGE_TABLE_SIZE);
	if (iipc->dh_buffer.page_table)
		bzero(iipc->dh_buffer.page_table, PLATFORM_PAGE_TABLE_SIZE);
	iipc->dh_buffer.dmac = dma_get(DMA_DIR_HMEM_TO_LMEM, 0, DMA_DEV_HOST,
				       DMA_ACCESS_SHARED);
	if (!iipc->dh_buffer.dmac) {
		tr_err(&ipc_tr, "Unable to find DMA for host page table");
		panic(SOF_IPC_PANIC_IPC);
	}
#endif

	/* configure interrupt */
	interrupt_register(PLATFORM_IPC_INTERRUPT, irq_handler, ipc);
	interrupt_enable(PLATFORM_IPC_INTERRUPT, ipc);

	/* enable GP #0 for Host -> DSP message notification
	 * enable GP #1 for DSP -> Host message notification
	 */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(0) | IMX_MU_xCR_GIEn(1), 0);

	return 0;
}

#if CONFIG_IPC_POLLING

int ipc_platform_poll_init(void)
{
	return 0;
}

/* tell host we have completed command */
void ipc_platform_poll_set_cmd_done(void)
{
	/* enable GP interrupt #0 - accept new messages */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(0), 0);

	/* request GP interrupt #0 - notify host that reply is ready */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIRn(0), 0);
}

/* read the IPC register for any new command messages */
int ipc_platform_poll_is_cmd_pending(void)
{
	uint32_t status;

	/* Interrupt arrived, check src */
	status = imx_mu_read(IMX_MU_xSR);

	/* new message from host */
	if (status & IMX_MU_xSR_GIPn(0)) {

		/* Disable GP interrupt #0 */
		imx_mu_xcr_rmw(0, IMX_MU_xCR_GIEn(0));

		/* Clear GP pending interrupt #0 */
		imx_mu_xsr_rmw(IMX_MU_xSR_GIPn(0), 0);

		interrupt_clear(PLATFORM_IPC_INTERRUPT);

		/*new message */
		return 1;
	}

	/* no new message */
	return 0;
}

int ipc_platform_poll_is_host_ready(void)
{
	uint32_t status;

	/* Interrupt arrived, check src */
	status = imx_mu_read(IMX_MU_xSR);

	/* reply message(done) from host */
	if (status & IMX_MU_xSR_GIPn(1)) {
		/* Disable GP interrupt #1 */
		imx_mu_xcr_rmw(0, IMX_MU_xCR_GIEn(1));

		/* Clear GP pending interrupt #1 */
		imx_mu_xsr_rmw(IMX_MU_xSR_GIPn(1), 0);

		interrupt_clear(PLATFORM_IPC_INTERRUPT);

		/* unmask GP interrupt #1 */
		imx_mu_xcr_rmw(IMX_MU_xCR_GIEn(1), 0);

		/* host done */
		return 1;
	}

	/* host still pending */
	return 0;
}

int ipc_platform_poll_tx_host_msg(struct ipc_msg *msg)
{
	/* can't send notification when one is in progress */
	if (imx_mu_read(IMX_MU_xCR) & IMX_MU_xCR_GIRn(1))
		return 0;

	/* now send the message */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);

	/* now interrupt host to tell it we have sent a message */
	imx_mu_xcr_rmw(IMX_MU_xCR_GIRn(1), 0);

	/* message sent */
	return 1;
}

#endif
