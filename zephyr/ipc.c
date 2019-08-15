// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <kernel.h>

#include <sof/ipc.h>

/* testbench ipc */
extern struct ipc *_ipc;

static struct k_work _ipc_work;

/* private data for IPC */
struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};


/* test code to check working IRQ */
static void ipc_irq_handler(void *arg)
{
	uint32_t dipcctl;

	uint32_t dipct;
	uint32_t dipcie;

	dipct = ipc_read(IPC_DIPCT);
	dipcie = ipc_read(IPC_DIPCIE);
	dipcctl = ipc_read(IPC_DIPCCTL);

	tracev_ipc("ipc: irq dipct 0x%x dipcie 0x%x dipcctl 0x%x", dipct,
		   dipcie, dipcctl);

	/* new message from host */
	if (dipct & IPC_DIPCT_BUSY && dipcctl & IPC_DIPCCTL_IPCTBIE)
	{
		/* mask Busy interrupt */
		ipc_write(IPC_DIPCCTL, dipcctl & ~IPC_DIPCCTL_IPCTBIE);

		/* TODO: place message in Q and process later */
		/* It's not Q ATM, may overwrite */
		if (_ipc->host_pending) {
			trace_ipc_error("ipc: dropping msg");
			trace_ipc_error(" dipct 0x%x dipcie 0x%x dipcctl 0x%x",
					dipct, dipcie, ipc_read(IPC_DIPCCTL));
		} else {
			_ipc->host_pending = 1;
			k_work_submit(&_ipc_work);
		}
	}

	/* reply message(done) from host */
	if (dipcie & IPC_DIPCIE_DONE && dipcctl & IPC_DIPCCTL_IPCIDIE)
	{
		/* mask Done interrupt */
		ipc_write(IPC_DIPCCTL,
			  ipc_read(IPC_DIPCCTL) & ~IPC_DIPCCTL_IPCIDIE);

		/* clear DONE bit - tell host we have completed the operation */
		ipc_write(IPC_DIPCIE,
			  ipc_read(IPC_DIPCIE) | IPC_DIPCIE_DONE);

		/* unmask Done interrupt */
		ipc_write(IPC_DIPCCTL,
			  ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCIDIE);

		/* send next message to host */
		ipc_process_msg_queue();
	}
}

void ipc_platform_do_cmd(struct ipc *ipc)
{
	struct sof_ipc_reply reply;
	int32_t err;

	/* perform command and return any error */
	err = ipc_cmd();

	/* if err > 0, reply created and copied by cmd() */
	if (err <= 0) {
		/* send std error/ok reply */
		reply.error = err;

		reply.hdr.cmd = SOF_IPC_GLB_REPLY;
		reply.hdr.size = sizeof(reply);
		mailbox_hostbox_write(0, &reply, sizeof(reply));
	}

	ipc->host_pending = 0;

	/* write 1 to clear busy, and trigger interrupt to host*/
	ipc_write(IPC_DIPCT, ipc_read(IPC_DIPCT) | IPC_DIPCT_BUSY);

	/* unmask Busy interrupt */
	ipc_write(IPC_DIPCCTL, ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCTBIE);
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

static void _ipc_work_handler(struct k_work *work)
{
	ipc_process_task(_ipc);
}

int platform_ipc_init(struct ipc *ipc)
{
	_ipc = ipc;

	ipc_set_drvdata(_ipc, NULL);

	/* init Zephyr work queue item for interrupt handling */
	k_work_init(&_ipc_work, _ipc_work_handler);

	/* configure interrupt */
	interrupt_register(PLATFORM_IPC_INTERRUPT, IRQ_AUTO_UNMASK,
			   ipc_irq_handler, NULL);
	interrupt_enable(PLATFORM_IPC_INTERRUPT);

	/* enable IPC interrupts from host */
	ipc_write(IPC_DIPCCTL, IPC_DIPCCTL_IPCIDIE | IPC_DIPCCTL_IPCTBIE);

	return 0;
}
