// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/drivers/interrupt.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/shim.h>
#include <sof/lib/wait.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <ipc/header.h>
#include <ipc/topology.h>
#include <stdint.h>

extern struct ipc *_ipc;

/* private data for IPC */
struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

static void irq_handler(void *arg)
{
	uint32_t isr, imrd;

	/* Interrupt arrived, check src */
	isr = shim_read(SHIM_ISRD);
	imrd = shim_read(SHIM_IMRD);

	tracev_ipc("ipc: irq isr 0x%x", isr);

	if (isr & SHIM_ISRD_DONE) {

		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_DONE);

		/* clear DONE bit - tell Host we have completed */
		shim_write(SHIM_IPCD, 0);

		/* unmask Done interrupt */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DONE);
	}

	if (isr & SHIM_ISRD_BUSY && !(imrd & SHIM_IMRD_BUSY)) {

		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_BUSY);

		ipc_schedule_process(_ipc);
	}
}

enum task_state ipc_platform_do_cmd(void *data)
{
	struct sof_ipc_cmd_hdr *hdr;
	/* Use struct ipc_data *iipc = ipc_get_drvdata(ipc); if needed */

	/* perform command */
	hdr = mailbox_validate();
	ipc_cmd(hdr);

	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(void *data)
{
	struct ipc *ipc = data;

	/* clear BUSY bit and set DONE bit - accept new messages */
	shim_write(SHIM_IPCX, SHIM_IPCX_DONE);

	/* unmask busy interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_BUSY);

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

	/* can't send nofication when one is in progress */
	if (shim_read(SHIM_IPCD) & (SHIM_IPCD_BUSY | SHIM_IPCD_DONE))
		goto out;

	/* now send the message */
	msg = list_first_item(&_ipc->shared_ctx->msg_list, struct ipc_msg,
			      list);
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	list_item_del(&msg->list);
	tracev_ipc("ipc: msg tx -> 0x%x", msg->header);

	/* now interrupt host to tell it we have message sent */
	shim_write(SHIM_IPCD, SHIM_IPCD_BUSY);

	list_item_append(&msg->list, &_ipc->shared_ctx->empty_list);

out:
	spin_unlock_irq(_ipc->lock, flags);
}

struct ipc_data_host_buffer *ipc_platform_get_host_buffer(struct ipc *ipc)
{
	struct ipc_data *iipc = ipc_get_drvdata(ipc);

	return &iipc->dh_buffer;
}

int platform_ipc_init(struct ipc *ipc)
{
	struct ipc_data *iipc;
	uint32_t imrd, dir, caps, dev;

	_ipc = ipc;

	/* init ipc data */
	iipc = rzalloc(RZONE_SYS, 0, SOF_MEM_CAPS_RAM,
		       sizeof(struct ipc_data));
	ipc_set_drvdata(_ipc, iipc);

	/* schedule */
	schedule_task_init_edf(&_ipc->ipc_task, &ipc_task_ops, _ipc, 0, 0);

#if CONFIG_HOST_PTABLE
	/* allocate page table buffer */
	iipc->dh_buffer.page_table = rzalloc(RZONE_SYS, 0, SOF_MEM_CAPS_RAM,
					     PLATFORM_PAGE_TABLE_SIZE);
	if (iipc->dh_buffer.page_table)
		bzero(iipc->dh_buffer.page_table, PLATFORM_PAGE_TABLE_SIZE);
#endif

	/* request GP DMA with shared access privilege */
	caps = 0;
	dir = DMA_DIR_HMEM_TO_LMEM;
	dev = DMA_DEV_HOST;
	iipc->dh_buffer.dmac = dma_get(dir, caps, dev, DMA_ACCESS_SHARED);

	/* configure interrupt */
	interrupt_register(PLATFORM_IPC_INTERRUPT, irq_handler, ipc);
	interrupt_enable(PLATFORM_IPC_INTERRUPT, ipc);

	/* Unmask Busy and Done interrupts */
	imrd = shim_read(SHIM_IMRD);
	imrd &= ~(SHIM_IMRD_BUSY | SHIM_IMRD_DONE);
	shim_write(SHIM_IMRD, imrd);

	return 0;
}
