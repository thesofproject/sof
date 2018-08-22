/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

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
#include <sof/ssp.h>
#include <platform/interrupt.h>
#include <platform/mailbox.h>
#include <platform/shim.h>
#include <platform/dma.h>
#include <platform/platform.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/panic.h>
#include <uapi/ipc.h>
#include <sof/intel-ipc.h>

extern struct ipc *_ipc;

static void do_notify(void)
{
	uint32_t flags;
	struct ipc_msg *msg;

	tracev_ipc("Not");

	spin_lock_irq(&_ipc->lock, flags);
	msg = _ipc->dsp_msg;
	if (msg == NULL)
		goto out;

	/* copy the data returned from DSP */
	if (msg->rx_size && msg->rx_size < SOF_IPC_MSG_MAX_SIZE)
		mailbox_dspbox_read(msg->rx_data, 0, msg->rx_size);

	/* any callback ? */
	if (msg->cb)
		msg->cb(msg->cb_data, msg->rx_data);

	list_item_append(&msg->list, &_ipc->empty_list);

out:
	spin_unlock_irq(&_ipc->lock, flags);

	/* clear DONE bit - tell Host we have completed */
	shim_write(SHIM_IPCD, 0);

	/* unmask Done interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_DONE);
}

static void irq_handler(void *arg)
{
	uint32_t isr;

	tracev_ipc("IRQ");

	/* Interrupt arrived, check src */
	isr = shim_read(SHIM_ISRD);

	if (isr & SHIM_ISRD_DONE) {

		/* Mask Done interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_DONE);
		interrupt_clear(PLATFORM_IPC_INTERUPT);
		do_notify();
	}

	if (isr & SHIM_ISRD_BUSY) {

		/* Mask Busy interrupt before return */
		shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_BUSY);
		interrupt_clear(PLATFORM_IPC_INTERUPT);

		/* TODO: place message in Q and process later */
		/* It's not Q ATM, may overwrite */
		if (_ipc->host_pending)
			trace_ipc_error("Pen");

		_ipc->host_msg = shim_read(SHIM_IPCX);
		_ipc->host_pending = 1;
	}
}

void ipc_platform_do_cmd(struct ipc *ipc)
{
	struct intel_ipc_data *iipc = ipc_get_drvdata(ipc);
	struct sof_ipc_reply reply;
	int32_t err;

	tracev_ipc("Cmd");

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

	/* clear BUSY bit and set DONE bit - accept new messages */
	shim_write(SHIM_IPCX, SHIM_IPCX_DONE);

	// TODO: signal audio work to enter D3 in normal context
	/* are we about to enter D3 ? */
	if (iipc->pm_prepare_D3) {
		while (1) {
			trace_ipc("pme");
			wait_for_interrupt(0);
		}
	}

	tracev_ipc("CmD");
}

void ipc_platform_mask_irq(void)
{
	/* Mask Busy interrupt before return */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) | SHIM_IMRD_BUSY);
}

void ipc_platform_unmask_irq(void)
{
	/* unmask busy interrupt */
	shim_write(SHIM_IMRD, shim_read(SHIM_IMRD) & ~SHIM_IMRD_BUSY);
}

void ipc_platform_send_msg(struct ipc *ipc)
{
	struct ipc_msg *msg;
	uint32_t flags;

	spin_lock_irq(&ipc->lock, flags);

	/* any messages to send ? */
	if (list_is_empty(&ipc->msg_list)) {
		ipc->dsp_pending = 0;
		goto out;
	}

	/* can't send nofication when one is in progress */
	if (shim_read(SHIM_IPCD) & (SHIM_IPCD_BUSY | SHIM_IPCD_DONE))
		goto out;

	/* now send the message */
	msg = list_first_item(&ipc->msg_list, struct ipc_msg, list);
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	list_item_del(&msg->list);
	ipc->dsp_msg = msg;
	tracev_ipc("Msg");

	/* now interrupt host to tell it we have message sent */
	shim_write(SHIM_IPCD, SHIM_IPCD_BUSY);

	list_item_append(&msg->list, &ipc->empty_list);

out:
	spin_unlock_irq(&ipc->lock, flags);
}

int platform_ipc_init(struct ipc *ipc)
{
	struct intel_ipc_data *iipc;
	uint32_t imrd, dir, caps, dev;
	int i;

	_ipc = ipc;

	/* init ipc data */
	iipc = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
		sizeof(struct intel_ipc_data));
	ipc_set_drvdata(_ipc, iipc);
	_ipc->dsp_msg = NULL;
	list_init(&ipc->empty_list);
	list_init(&ipc->msg_list);
	spinlock_init(&ipc->lock);

	for (i = 0; i < MSG_QUEUE_SIZE; i++)
		list_item_prepend(&ipc->message[i].list, &ipc->empty_list);

	/* allocate page table buffer */
	iipc->page_table = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
		PLATFORM_PAGE_TABLE_SIZE);
	if (iipc->page_table)
		bzero(iipc->page_table, PLATFORM_PAGE_TABLE_SIZE);

	/* request GP DMA with shared access privilege */
	caps = 0;
	dir = DMA_DIR_HMEM_TO_LMEM;
	dev = DMA_DEV_HDA;
	iipc->dmac = dma_get(dir, caps, dev, DMA_ACCESS_SHARED);

	/* PM */
	iipc->pm_prepare_D3 = 0;

	/* configure interrupt */
	interrupt_register(PLATFORM_IPC_INTERUPT, irq_handler, NULL);
	interrupt_enable(PLATFORM_IPC_INTERUPT);

	/* Unmask Busy and Done interrupts */
	imrd = shim_read(SHIM_IMRD);
	imrd &= ~(SHIM_IMRD_BUSY | SHIM_IMRD_DONE);
	shim_write(SHIM_IMRD, imrd);

	return 0;
}

