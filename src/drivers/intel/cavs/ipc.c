// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>

#include <cavs/version.h>
#include <sof/drivers/interrupt.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_runtime.h>
#include <sof/lib/uuid.h>
#include <sof/lib/wait.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <ipc/header.h>
#if CAVS_VERSION >= CAVS_VERSION_1_8
#include <ipc/header-intel-cavs.h>
#include <ipc/pm.h>
#include <cavs/drivers/sideband-ipc.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if CAVS_VERSION >= CAVS_VERSION_1_8

#define CAVS_IPC_TYPE_S(x)		((x) & CAVS_IPC_TYPE_MASK)

#endif

/* 8fa1d42f-bc6f-464b-867f-547af08834da */
DECLARE_SOF_UUID("ipc-task", ipc_task_uuid, 0x8fa1d42f, 0xbc6f, 0x464b,
		 0x86, 0x7f, 0x54, 0x7a, 0xf0, 0x88, 0x34, 0xda);

/* No private data for IPC */

#if CONFIG_DEBUG_IPC_COUNTERS
static inline void increment_ipc_received_counter(void)
{
	static uint32_t ipc_received_counter;

	mailbox_sw_reg_write(SRAM_REG_FW_IPC_RECEIVED_COUNT,
			     ipc_received_counter++);
}

static inline void increment_ipc_processed_counter(void)
{
	static uint32_t ipc_processed_counter;

	mailbox_sw_reg_write(SRAM_REG_FW_IPC_PROCESSED_COUNT,
			     ipc_processed_counter++);
}
#endif

/* test code to check working IRQ */
static void ipc_irq_handler(void *arg)
{
	struct ipc *ipc = arg;
	uint32_t dipcctl;

#if CAVS_VERSION == CAVS_VERSION_1_5
	uint32_t dipct;
	uint32_t dipcie;

	dipct = ipc_read(IPC_DIPCT);
	dipcie = ipc_read(IPC_DIPCIE);
	dipcctl = ipc_read(IPC_DIPCCTL);

	tr_dbg(&ipc_tr, "ipc: irq dipct 0x%x dipcie 0x%x dipcctl 0x%x", dipct,
	       dipcie, dipcctl);
#else
	uint32_t dipctdr;
	uint32_t dipcida;

	dipctdr = ipc_read(IPC_DIPCTDR);
	dipcida = ipc_read(IPC_DIPCIDA);
	dipcctl = ipc_read(IPC_DIPCCTL);

	tr_dbg(&ipc_tr, "ipc: irq dipctdr 0x%x dipcida 0x%x dipcctl 0x%x",
	       dipctdr, dipcida, dipcctl);
#endif

	/* new message from host */
#if CAVS_VERSION == CAVS_VERSION_1_5
	if (dipct & IPC_DIPCT_BUSY && dipcctl & IPC_DIPCCTL_IPCTBIE)
#else
	if (dipctdr & IPC_DIPCTDR_BUSY && dipcctl & IPC_DIPCCTL_IPCTBIE)
#endif
	{
		/* mask Busy interrupt */
		ipc_write(IPC_DIPCCTL, dipcctl & ~IPC_DIPCCTL_IPCTBIE);

#if CONFIG_DEBUG_IPC_COUNTERS
		increment_ipc_received_counter();
#endif

		ipc_schedule_process(ipc);
	}

	/* reply message(done) from host */
#if CAVS_VERSION == CAVS_VERSION_1_5
	if (dipcie & IPC_DIPCIE_DONE && dipcctl & IPC_DIPCCTL_IPCIDIE)
#else
	if (dipcida & IPC_DIPCIDA_DONE)
#endif
	{
		/* mask Done interrupt */
		ipc_write(IPC_DIPCCTL,
			  ipc_read(IPC_DIPCCTL) & ~IPC_DIPCCTL_IPCIDIE);

		/* clear DONE bit - tell host we have completed the operation */
#if CAVS_VERSION == CAVS_VERSION_1_5
		ipc_write(IPC_DIPCIE,
			  ipc_read(IPC_DIPCIE) | IPC_DIPCIE_DONE);
#else
		ipc_write(IPC_DIPCIDA,
			  ipc_read(IPC_DIPCIDA) | IPC_DIPCIDA_DONE);
#endif

		ipc->is_notification_pending = false;

		/* unmask Done interrupt */
		ipc_write(IPC_DIPCCTL,
			  ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCIDIE);

		/* send next message to host */
		ipc_send_queued_msg();
	}
}

#if CAVS_VERSION >= CAVS_VERSION_1_8
static struct sof_ipc_cmd_hdr *ipc_cavs_read_set_d0ix(uint32_t dr, uint32_t dd)
{
	struct sof_ipc_pm_gate *cmd = ipc_get()->comp_data;

	cmd->hdr.cmd = SOF_IPC_GLB_PM_MSG | SOF_IPC_PM_GATE;
	cmd->hdr.size = sizeof(*cmd);
	cmd->flags = dd & CAVS_IPC_MOD_SETD0IX_BIT_MASK;

	return &cmd->hdr;
}

static struct sof_ipc_cmd_hdr *ipc_compact_read_msg(void)
{
	struct sof_ipc_cmd_hdr *hdr;
	uint32_t dr;
	uint32_t dd;

	dr = ipc_read(IPC_DIPCTDR);
	dd = ipc_read(IPC_DIPCTDD);

	/* if there is no cAVS module IPC in regs go the previous path */
	if (!(dr & CAVS_IPC_MSG_TGT))
		return mailbox_validate();

	switch (CAVS_IPC_TYPE_S(dr)) {
	case CAVS_IPC_MOD_SET_D0IX:
		hdr = ipc_cavs_read_set_d0ix(dr, dd);
		break;
	default:
		return NULL;
	}

	platform_shared_commit(hdr, hdr->size);

	return hdr;
}
#endif

enum task_state ipc_platform_do_cmd(void *data)
{
#if !CONFIG_SUECREEK
	struct ipc *ipc = data;
#endif
	struct sof_ipc_cmd_hdr *hdr;
	struct sof_ipc_reply reply;

#if CAVS_VERSION >= CAVS_VERSION_1_8
	hdr = ipc_compact_read_msg();
#else
	hdr = mailbox_validate();
#endif
	/* perform command */
	if (hdr)
		ipc_cmd(hdr);
	else {
		/* send invalid command error in reply */
		reply.error = -EINVAL;
		reply.hdr.cmd = SOF_IPC_GLB_REPLY;
		reply.hdr.size = sizeof(reply);
		mailbox_hostbox_write(0, &reply, sizeof(reply));
	}

	/* are we about to enter D3 ? */
#if !CONFIG_SUECREEK
	if (ipc->pm_prepare_D3) {
		platform_shared_commit(ipc, sizeof(*ipc));

		/* no return - memory will be powered off and IPC sent */
		platform_pm_runtime_power_off();
	}

	platform_shared_commit(ipc, sizeof(*ipc));
#endif

	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(void *data)
{
#if CONFIG_SUECREEK
	struct ipc *ipc = data;
#endif

	/* write 1 to clear busy, and trigger interrupt to host*/
#if CAVS_VERSION == CAVS_VERSION_1_5
	ipc_write(IPC_DIPCT, ipc_read(IPC_DIPCT) | IPC_DIPCT_BUSY);
#else
	ipc_write(IPC_DIPCTDR, ipc_read(IPC_DIPCTDR) | IPC_DIPCTDR_BUSY);
	ipc_write(IPC_DIPCTDA, ipc_read(IPC_DIPCTDA) | IPC_DIPCTDA_DONE);
#endif

#if CONFIG_DEBUG_IPC_COUNTERS
	increment_ipc_processed_counter();
#endif

	/* unmask Busy interrupt */
	ipc_write(IPC_DIPCCTL, ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCTBIE);

#if CONFIG_SUECREEK
	if (ipc->pm_prepare_D3) {
		platform_shared_commit(ipc, sizeof(*ipc));

		//TODO: add support for Icelake
		while (1)
			wait_for_interrupt(0);
	}

	platform_shared_commit(ipc, sizeof(*ipc));
#endif
}

int ipc_platform_send_msg(struct ipc_msg *msg)
{
	struct ipc *ipc = ipc_get();
	int ret = 0;

	if (ipc->is_notification_pending ||
#if CAVS_VERSION == CAVS_VERSION_1_5
	    ipc_read(IPC_DIPCI) & IPC_DIPCI_BUSY) {
#else
	    ipc_read(IPC_DIPCIDR) & IPC_DIPCIDR_BUSY ||
	    ipc_read(IPC_DIPCIDA) & IPC_DIPCIDA_DONE) {
#endif
		ret = -EBUSY;
		goto out;
	}

	/* now send the message */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	list_item_del(&msg->list);
	tr_dbg(&ipc_tr, "ipc: msg tx -> 0x%x", msg->header);

	ipc->is_notification_pending = true;

	/* now interrupt host to tell it we have message sent */
#if CAVS_VERSION == CAVS_VERSION_1_5
	ipc_write(IPC_DIPCIE, 0);
	ipc_write(IPC_DIPCI, IPC_DIPCI_BUSY | msg->header);
#else
	ipc_write(IPC_DIPCIDD, 0);
	ipc_write(IPC_DIPCIDR, 0x80000000 | msg->header);
#endif

	platform_shared_commit(msg, sizeof(*msg));

out:
	platform_shared_commit(ipc, sizeof(*ipc));

	return ret;
}

int platform_ipc_init(struct ipc *ipc)
{
	int irq;

	ipc_set_drvdata(ipc, NULL);

	/* schedule */
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_uuid),
			       &ipc_task_ops, ipc, 0, 0);

	/* configure interrupt */
	irq = interrupt_get_irq(PLATFORM_IPC_INTERRUPT,
				PLATFORM_IPC_INTERRUPT_NAME);
	if (irq < 0)
		return irq;
	interrupt_register(irq, ipc_irq_handler, ipc);
	interrupt_enable(irq, ipc);

	/* enable IPC interrupts from host */
	ipc_write(IPC_DIPCCTL, IPC_DIPCCTL_IPCIDIE | IPC_DIPCCTL_IPCTBIE);

	platform_shared_commit(ipc, sizeof(*ipc));

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

	/* write 1 to clear busy, and trigger interrupt to host*/
#if CAVS_VERSION == CAVS_VERSION_1_5
	ipc_write(IPC_DIPCT, ipc_read(IPC_DIPCT) | IPC_DIPCT_BUSY);
#else
	ipc_write(IPC_DIPCTDR, ipc_read(IPC_DIPCTDR) | IPC_DIPCTDR_BUSY);
	ipc_write(IPC_DIPCTDA, ipc_read(IPC_DIPCTDA) | IPC_DIPCTDA_DONE);
#endif

	/* unmask Busy interrupt */
	ipc_write(IPC_DIPCCTL, ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCTBIE);
}

/* read the IPC register for any new command messages */
int ipc_platform_poll_is_cmd_pending(void)
{
	uint32_t dipcctl;

#if CAVS_VERSION == CAVS_VERSION_1_5
	uint32_t dipct;

	dipct = ipc_read(IPC_DIPCT);
	dipcctl = ipc_read(IPC_DIPCCTL);


#else
	uint32_t dipctdr;

	dipctdr = ipc_read(IPC_DIPCTDR);
	dipcctl = ipc_read(IPC_DIPCCTL);
#endif

	/* new message from host */
#if CAVS_VERSION == CAVS_VERSION_1_5
	if (dipct & IPC_DIPCT_BUSY && dipcctl & IPC_DIPCCTL_IPCTBIE)
#else
	if (dipctdr & IPC_DIPCTDR_BUSY && dipcctl & IPC_DIPCCTL_IPCTBIE)
#endif
	{
		/* mask Busy interrupt */
		ipc_write(IPC_DIPCCTL, dipcctl & ~IPC_DIPCCTL_IPCTBIE);

		/* new message */
		return 1;
	}

	/* no new message */
	return 0;
}

int ipc_platform_poll_is_host_ready(void)
{
#if CAVS_VERSION == CAVS_VERSION_1_5
	uint32_t dipcie;
	uint32_t dipcctl;

	dipcie = ipc_read(IPC_DIPCIE);
	dipcctl = ipc_read(IPC_DIPCCTL);


#else
	uint32_t dipcida;

	dipcida = ipc_read(IPC_DIPCIDA);
#endif

	/* reply message(done) from host */
#if CAVS_VERSION == CAVS_VERSION_1_5
	if (dipcie & IPC_DIPCIE_DONE && dipcctl & IPC_DIPCCTL_IPCIDIE)
#else
	if (dipcida & IPC_DIPCIDA_DONE)
#endif
	{
		/* mask Done interrupt */
		ipc_write(IPC_DIPCCTL,
			  ipc_read(IPC_DIPCCTL) & ~IPC_DIPCCTL_IPCIDIE);

		/* clear DONE bit - tell host we have completed the operation */
#if CAVS_VERSION == CAVS_VERSION_1_5
		ipc_write(IPC_DIPCIE,
			  ipc_read(IPC_DIPCIE) | IPC_DIPCIE_DONE);
#else
		ipc_write(IPC_DIPCIDA,
			  ipc_read(IPC_DIPCIDA) | IPC_DIPCIDA_DONE);
#endif

		/* unmask Done interrupt */
		ipc_write(IPC_DIPCCTL,
			  ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCIDIE);

		/* host has completed */
		return 1;
	}

	/* host still pending */
	return 0;
}



int ipc_platform_poll_tx_host_msg(struct ipc_msg *msg)
{

#if CAVS_VERSION == CAVS_VERSION_1_5
	if (ipc_read(IPC_DIPCI) & IPC_DIPCI_BUSY)
#else
	if (ipc_read(IPC_DIPCIDR) & IPC_DIPCIDR_BUSY ||
	    ipc_read(IPC_DIPCIDA) & IPC_DIPCIDA_DONE)
#endif
		/* cant send message atm */
		return 0;

	/* now send the message */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);

	/* now interrupt host to tell it we have message sent */
#if CAVS_VERSION == CAVS_VERSION_1_5
	ipc_write(IPC_DIPCIE, 0);
	ipc_write(IPC_DIPCI, IPC_DIPCI_BUSY | msg->header);
#else
	ipc_write(IPC_DIPCIDD, 0);
	ipc_write(IPC_DIPCIDR, 0x80000000 | msg->header);
#endif

	/* message sent */
	platform_shared_commit(msg, sizeof(*msg));
	return 1;
}

#endif
