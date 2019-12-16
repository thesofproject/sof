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
#include <sof/lib/wait.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>
#include <sof/spinlock.h>
#include <ipc/header.h>
#if CAVS_VERSION >= CAVS_VERSION_1_8
#include <ipc/header-intel-cavs.h>
#include <ipc/pm.h>
#endif
#include <config.h>
#include <stddef.h>
#include <stdint.h>

#if CAVS_VERSION >= CAVS_VERSION_1_8

#define CAVS_IPC_TYPE_S(x)		((x) & CAVS_IPC_TYPE_MASK)

#endif

extern struct ipc *_ipc;

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
	uint32_t dipcctl;

#if CAVS_VERSION == CAVS_VERSION_1_5
	uint32_t dipct;
	uint32_t dipcie;

	dipct = ipc_read(IPC_DIPCT);
	dipcie = ipc_read(IPC_DIPCIE);
	dipcctl = ipc_read(IPC_DIPCCTL);

	tracev_ipc("ipc: irq dipct 0x%x dipcie 0x%x dipcctl 0x%x", dipct,
		   dipcie, dipcctl);
#else
	uint32_t dipctdr;
	uint32_t dipcida;

	dipctdr = ipc_read(IPC_DIPCTDR);
	dipcida = ipc_read(IPC_DIPCIDA);
	dipcctl = ipc_read(IPC_DIPCCTL);

	tracev_ipc("ipc: irq dipctdr 0x%x dipcida 0x%x dipcctl 0x%x", dipctdr,
		   dipcida, dipcctl);
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

		ipc_schedule_process(_ipc);
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

		/* unmask Done interrupt */
		ipc_write(IPC_DIPCCTL,
			  ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCIDIE);

		/* send next message to host */
		ipc_process_msg_queue();
	}
}

#if CAVS_VERSION >= CAVS_VERSION_1_8
static struct sof_ipc_cmd_hdr *ipc_cavs_read_set_d0ix(uint32_t dr, uint32_t dd)
{
	struct sof_ipc_pm_gate *cmd = _ipc->comp_data;

	cmd->hdr.cmd = SOF_IPC_GLB_PM_MSG | SOF_IPC_PM_GATE;
	cmd->hdr.size = sizeof(*cmd);
	cmd->flags = dd & CAVS_IPC_MOD_SETD0IX_BIT_MASK;

	return &cmd->hdr;
}

static struct sof_ipc_cmd_hdr *ipc_cavs_read_msg(void)
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

	dcache_writeback_region(hdr, hdr->size);

	return hdr;
}
#endif

static enum task_state ipc_platform_do_cmd(void *data)
{
#if !CONFIG_SUECREEK
	struct ipc *ipc = data;
#endif
	struct sof_ipc_cmd_hdr *hdr;
	struct sof_ipc_reply reply;

#if CAVS_VERSION >= CAVS_VERSION_1_8
	hdr = ipc_cavs_read_msg();
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
		/* no return - memory will be powered off and IPC sent */
		platform_pm_runtime_power_off();
	}
#endif

	return SOF_TASK_STATE_COMPLETED;
}

static void ipc_platform_complete_cmd(void *data)
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
		//TODO: add support for Icelake
		while (1)
			wait_for_interrupt(0);
	}
#endif
}

void ipc_platform_send_msg(void)
{
	struct ipc_msg *msg;
	uint32_t flags;

	spin_lock_irq(_ipc->lock, flags);

	/* any messages to send ? */
	if (list_is_empty(&_ipc->shared_ctx->msg_list)) {
		_ipc->shared_ctx->dsp_pending = 0;
		goto out;
	}

#if CAVS_VERSION == CAVS_VERSION_1_5
	if (ipc_read(IPC_DIPCI) & IPC_DIPCI_BUSY)
#else
	if (ipc_read(IPC_DIPCIDR) & IPC_DIPCIDR_BUSY ||
	    ipc_read(IPC_DIPCIDA) & IPC_DIPCIDA_DONE)
#endif
		goto out;

	/* now send the message */
	msg = list_first_item(&_ipc->shared_ctx->msg_list, struct ipc_msg,
			      list);
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	list_item_del(&msg->list);
	_ipc->shared_ctx->dsp_msg = msg;
	tracev_ipc("ipc: msg tx -> 0x%x", msg->header);

	/* now interrupt host to tell it we have message sent */
#if CAVS_VERSION == CAVS_VERSION_1_5
	ipc_write(IPC_DIPCIE, 0);
	ipc_write(IPC_DIPCI, IPC_DIPCI_BUSY | msg->header);
#else
	ipc_write(IPC_DIPCIDD, 0);
	ipc_write(IPC_DIPCIDR, 0x80000000 | msg->header);
#endif

	list_item_append(&msg->list, &_ipc->shared_ctx->empty_list);

out:
	spin_unlock_irq(_ipc->lock, flags);
}

int platform_ipc_init(struct ipc *ipc)
{
	int irq;

	_ipc = ipc;

	ipc_set_drvdata(_ipc, NULL);

	/* schedule */
	schedule_task_init(&_ipc->ipc_task, SOF_SCHEDULE_EDF, SOF_TASK_PRI_IPC,
			   ipc_platform_do_cmd, ipc_platform_complete_cmd, _ipc,
			   0, 0);

	/* configure interrupt */
	irq = interrupt_get_irq(PLATFORM_IPC_INTERRUPT,
				PLATFORM_IPC_INTERRUPT_NAME);
	if (irq < 0)
		return irq;
	interrupt_register(irq, ipc_irq_handler, ipc);
	interrupt_enable(irq, ipc);

	/* enable IPC interrupts from host */
	ipc_write(IPC_DIPCCTL, IPC_DIPCCTL_IPCIDIE | IPC_DIPCCTL_IPCTBIE);

	return 0;
}
