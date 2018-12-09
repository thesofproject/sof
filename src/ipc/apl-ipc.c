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
 *         Keyon Jie <yang.jie@linux.intel.com>
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
#include <uapi/ipc/header.h>
#include <platform/pm_runtime.h>

extern struct ipc *_ipc;

/* test code to check working IRQ */
static void irq_handler(void *arg)
{
	uint32_t dipct;
	uint32_t dipcie;
	uint32_t dipcctl;
	uint32_t msg = 0;

	dipct = ipc_read(IPC_DIPCT);
	dipcie = ipc_read(IPC_DIPCIE);
	dipcctl = ipc_read(IPC_DIPCCTL);

	tracev_ipc("ipc: irq dipct 0x%x dipcie 0x%x dipcctl 0x%x", dipct,
		   dipcie, dipcctl);

	/* new message from host */
	if (dipct & IPC_DIPCT_BUSY && dipcctl & IPC_DIPCCTL_IPCTBIE) {

		/* mask Busy interrupt */
		ipc_write(IPC_DIPCCTL, dipcctl & ~IPC_DIPCCTL_IPCTBIE);

		msg = dipct & IPC_DIPCT_MSG_MASK;

		/* TODO: place message in Q and process later */
		/* It's not Q ATM, may overwrite */
		if (_ipc->host_pending) {
			trace_ipc_error("ipc: dropping msg 0x%x", msg);
			trace_ipc_error(" dipct 0x%x dipcie 0x%x dipcctl 0x%x",
					dipct, dipcie, ipc_read(IPC_DIPCCTL));
		} else {
			_ipc->host_msg = msg;
			_ipc->host_pending = 1;
			ipc_schedule_process(_ipc);
		}
	}

	/* reply message(done) from host */
	if (dipcie & IPC_DIPCIE_DONE && dipcctl & IPC_DIPCCTL_IPCIDIE) {

		/* mask Done interrupt */
		ipc_write(IPC_DIPCCTL, ipc_read(IPC_DIPCCTL) & ~IPC_DIPCCTL_IPCIDIE);

		/* clear DONE bit */
		ipc_write(IPC_DIPCIE, ipc_read(IPC_DIPCIE) | ~IPC_DIPCIE_DONE);

		/* unmask Done interrupt */
		ipc_write(IPC_DIPCCTL, ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCIDIE);
	}

}

void ipc_platform_do_cmd(struct ipc *ipc)
{
	struct ipc_data *iipc = ipc_get_drvdata(ipc);
	struct sof_ipc_reply reply;
	int32_t err;

	trace_ipc("ipc: msg rx -> 0x%x", ipc->host_msg);

	/* perform command and return any error */
	err = ipc_cmd();
	if (err > 0) {
		goto done; /* reply created and copied by cmd() */
	} else if (err < 0) {
		/* send std error reply */
		reply.error = err;
	} else if (err == 0) {
		/* send std reply */
		reply.error = 0;
	}

	/* send std error/ok reply */
	reply.hdr.cmd = SOF_IPC_GLB_REPLY;
	reply.hdr.size = sizeof(reply);
	mailbox_hostbox_write(0, &reply, sizeof(reply));

done:
	ipc->host_pending = 0;

	/* write 1 to clear busy, and trigger interrupt to host*/
	ipc_write(IPC_DIPCT, ipc_read(IPC_DIPCT) |IPC_DIPCT_BUSY );

	/* unmask Busy interrupt */
	ipc_write(IPC_DIPCCTL, ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCTBIE);

	// TODO: signal audio work to enter D3 in normal context
	/* are we about to enter D3 ? */
	if (iipc->pm_prepare_D3) {
		/* no return - memory will be powered off */
		platform_pm_runtime_power_off();
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

	if (ipc_read(IPC_DIPCI) & IPC_DIPCI_BUSY)
		goto out;

	/* now send the message */
	msg = list_first_item(&ipc->shared_ctx->msg_list, struct ipc_msg,
			      list);
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	list_item_del(&msg->list);
	ipc->shared_ctx->dsp_msg = msg;
	tracev_ipc("ipc: msg tx -> 0x%x", msg->header);

	/* now interrupt host to tell it we have message sent */
	ipc_write(IPC_DIPCIE, 0);
	ipc_write(IPC_DIPCI, IPC_DIPCI_BUSY | msg->header);

	list_item_append(&msg->list, &ipc->shared_ctx->empty_list);

out:
	spin_unlock_irq(&ipc->lock, flags);
}

int platform_ipc_init(struct ipc *ipc)
{
	struct ipc_data *iipc;
	uint32_t dir, caps, dev;

	_ipc = ipc;

	/* init ipc data */
	iipc = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
		sizeof(struct ipc_data));
	ipc_set_drvdata(_ipc, iipc);

	/* schedule */
	schedule_task_init(&_ipc->ipc_task, ipc_process_task, _ipc);
	schedule_task_config(&_ipc->ipc_task, TASK_PRI_IPC, 0);

#ifdef CONFIG_HOST_PTABLE
	/* allocate page table buffer */
	iipc->page_table = rballoc(RZONE_SYS, SOF_MEM_CAPS_RAM,
			HOST_PAGE_SIZE);
	if (iipc->page_table)
		bzero(iipc->page_table, HOST_PAGE_SIZE);
#endif

	/* request HDA DMA with shared access privilege */
	caps = 0;
	dir = DMA_DIR_HMEM_TO_LMEM;
	dev = DMA_DEV_HOST;
	iipc->dmac = dma_get(dir, caps, dev, DMA_ACCESS_SHARED);

	/* PM */
	iipc->pm_prepare_D3 = 0;

	/* configure interrupt */
	interrupt_register(PLATFORM_IPC_INTERRUPT, IRQ_AUTO_UNMASK,
			   irq_handler, NULL);
	interrupt_enable(PLATFORM_IPC_INTERRUPT);

	/* enable IPC interrupts from host */
	ipc_write(IPC_DIPCCTL, IPC_DIPCCTL_IPCIDIE | IPC_DIPCCTL_IPCTBIE);

	return 0;
}
