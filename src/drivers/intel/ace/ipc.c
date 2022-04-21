// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>
//         Rander Wang <rander.wang@intel.com>

#include <ace/version.h>
#include <sof/drivers/interrupt.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/schedule.h>
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
#include <ipc/regs_ipc_ace.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ENABLE 1
#define DISABLE 0

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
	uint32_t *uncache_counter = cache_to_uncache(&ipc_processed_counter);

	mailbox_sw_reg_write(SRAM_REG_FW_IPC_PROCESSED_COUNT,
			     (*uncache_counter)++);
}

#endif /* CONFIG_DEBUG_IPC_COUNTERS */

/* test code to check working IRQ */
static void ipc_irq_handler(void *arg)
{
	struct ipc *ipc = arg;
	k_spinlock_key_t key;

	key = k_spin_lock(&ipc->lock);

	union DfIPCxTDR dipctdr;
	union DfIPCxCTL dipcctl;
	union DfIPCxIDA dipcida;

	dipctdr.full = ipc_read(DfIPCxTDR_REG);
	dipcctl.full = ipc_read(DfIPCxCTL_REG);
	dipcida.full = ipc_read(DfIPCxIDA_REG);

	/* TODO: add some debug messages here */

	/* new message from host */
	if (dipctdr.part.busy && dipcctl.part.ipctbie) {
		/* mask Busy interrupt */
		ipc_write(DfIPCxCTL_REG, dipcctl.full & ~1);

#if CONFIG_DEBUG_IPC_COUNTERS
		increment_ipc_received_counter();
#endif
		ipc_schedule_process(ipc);
	}

	/* reply message(done) from host */
	if (dipcida.part.done) {
		/* mask Done interrupt */
		ipc_write(IPC_DIPCCTL,
			  ipc_read(IPC_DIPCCTL) & ~IPC_DIPCCTL_IPCIDIE);

		/* clear DONE bit - tell host we have completed the operation */
		ipc_write(DfIPCxIDA_REG,
				  ipc_read(DfIPCxIDA_REG) | IPC_DIPCTDA_DONE);

		ipc->is_notification_pending = false;

		/* unmask Done interrupt */
		ipc_write(DfIPCxCTL_REG,
			  ipc_read(DfIPCxCTL_REG) | IPC_DIPCCTL_IPCIDIE);
	}

	k_spin_unlock(&ipc->lock, key);
}

int ipc_platform_compact_read_msg(ipc_cmd_hdr *hdr, int words)
{
	uint32_t *chdr = (uint32_t *)hdr;

	/* compact messages are 2 words on CAVS 1.8 onwards */
	if (words != 2)
		return 0;

	chdr[0] = ipc_read(DfIPCxTDR_REG);
	chdr[1] = ipc_read(DfIPCxTDDy_REG);

	return 2; /* number of words read */
}

int ipc_platform_compact_write_msg(ipc_cmd_hdr *hdr, int words)
{
	uint32_t *chdr = (uint32_t *)hdr;

	/* compact messages are 2 words on CAVS 1.8 onwards */
	if (words != 2)
		return 0;

	/* command complete will set the busy/done bits */
	ipc_write(IPC_DIPCTDR, chdr[0] & ~IPC_DIPCTDR_BUSY);
	ipc_write(IPC_DIPCTDD, chdr[1]);

	return 2; /* number of words written */
}

enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	ipc_cmd_hdr *hdr;

	hdr = ipc_compact_read_msg();

	/* perform command */
	ipc_cmd(hdr);

	/* are we about to enter D3 ? */
	if (ipc->pm_prepare_D3)
		/* no return - memory will be powered off and IPC sent */
		platform_pm_runtime_power_off();

	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(struct ipc *ipc)
{
	/* write 1 to clear busy, and trigger interrupt to host*/
	ipc_write(IPC_DIPCTDR, ipc_read(IPC_DIPCTDR) | IPC_DIPCTDR_BUSY);
	ipc_write(IPC_DIPCTDA, ipc_read(IPC_DIPCTDA) | IPC_DIPCTDA_DONE);

#if CONFIG_DEBUG_IPC_COUNTERS
	increment_ipc_processed_counter();
#endif

	/* unmask Busy interrupt */
	ipc_write(IPC_DIPCCTL, ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCTBIE);
}

int ipc_platform_send_msg(const struct ipc_msg *msg)
{
	struct ipc *ipc = ipc_get();
	ipc_cmd_hdr *hdr;

	if (ipc->is_notification_pending ||
	    ipc_read(IPC_DIPCIDR) & IPC_DIPCIDR_BUSY ||
	    ipc_read(IPC_DIPCIDA) & IPC_DIPCIDA_DONE) {
		return -EBUSY;
	}

	tr_dbg(&ipc_tr, "ipc: msg tx -> 0x%x", msg->header);

	ipc->is_notification_pending = true;

	/* prepare the message and copy to mailbox */
	hdr = ipc_prepare_to_send(msg);

	/* now interrupt host to tell it we have message sent */
	ipc_write(IPC_DIPCIDD, hdr->dat[1]);
	ipc_write(IPC_DIPCIDR, IPC_DIPCIDR_BUSY | hdr->dat[0]);

	/* Clear request */
	ipc_write(IPC_DIPCTDR, ipc_read(IPC_DIPCTDR) | IPC_DIPCTDR_BUSY);
	ipc_write(IPC_DIPCTDA, ipc_read(IPC_DIPCTDA) & ~IPC_DIPCTDA_DONE);
	return 0;
}

/*
 * Used to enable IPC interrupt in the controller.
 * TODO: make it a generic function as ACE FW ipc_controller_irq()
 */
static inline void
ipc_controller_irq(bool operation)
{
	uint32_t dw_num = 0x00;
	uint32_t mask = 1;
	volatile uint16_t * const instancePtr =
		(volatile uint16_t *) (MS_REG_HIPCIS);
	volatile uint32_t * const dw_en = (uint32_t *)MS_BLOCK_DINTIP;
	volatile uint32_t * const dw_mask = (uint32_t *)MS_REG_IRQ_INTMASK_L0;

	if (operation == ENABLE) {
		*instancePtr |= mask;
		*dw_en |= BIT(dw_num);
		*dw_mask &= ~BIT(dw_num);
	} else {
		*instancePtr &= ~mask;
		*dw_en &= ~BIT(dw_num);
		*dw_mask |= BIT(dw_num);
	}
}

int platform_ipc_init(struct ipc *ipc)
{
	ipc_set_drvdata(ipc, NULL);

	/* schedule task */
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_uuid),
			       &ipc_task_ops, ipc, 0, 0);

	interrupt_register(PLATFORM_IPC_INTERRUPT, ipc_irq_handler, ipc);
	interrupt_enable(LVL_2_IRQ_NUMBER, ipc);

	/* enable IPC interrupts from host */
	ipc_write(DfIPCxCTL_REG, IPC_DIPCCTL_IPCIDIE | IPC_DIPCCTL_IPCTBIE);
	ipc_controller_irq(ENABLE);

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
	ipc_write(IPC_DIPCTDR, ipc_read(IPC_DIPCTDR) | IPC_DIPCTDR_BUSY);
	ipc_write(IPC_DIPCTDA, ipc_read(IPC_DIPCTDA) | IPC_DIPCTDA_DONE);

	/* unmask Busy interrupt */
	ipc_write(IPC_DIPCCTL, ipc_read(IPC_DIPCCTL) | IPC_DIPCCTL_IPCTBIE);
}

/* read the IPC register for any new command messages */
int ipc_platform_poll_is_cmd_pending(void)
{
	uint32_t dipcctl = ipc_read(IPC_DIPCCTL);
	uint32_t dipctdr = ipc_read(IPC_DIPCTDR);

	/* new message from host */
	if (dipctdr & IPC_DIPCTDR_BUSY && dipcctl & IPC_DIPCCTL_IPCTBIE) {
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
	uint32_t dipcida = ipc_read(IPC_DIPCIDA);

	/* reply message(done) from host */
	if (dipcida & IPC_DIPCIDA_DONE) {
		/* mask Done interrupt */
		ipc_write(IPC_DIPCCTL,
			  ipc_read(IPC_DIPCCTL) & ~IPC_DIPCCTL_IPCIDIE);

		/* clear DONE bit - tell host we have completed the operation */
		ipc_write(IPC_DIPCIDA,
			  ipc_read(IPC_DIPCIDA) | IPC_DIPCIDA_DONE);

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
	if (ipc_read(IPC_DIPCIDR) & IPC_DIPCIDR_BUSY ||
	    ipc_read(IPC_DIPCIDA) & IPC_DIPCIDA_DONE)
		/* cant send message atm */
		return 0;

	/* now send the message */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);

	/* now interrupt host to tell it we have message sent */
	ipc_write(IPC_DIPCIDD, 0);
	ipc_write(IPC_DIPCIDR, IPC_DIPCIDR_BUSY | msg->header);

	/* message sent */
	return 1;
}

#endif /* CONFIG_IPC_POLLING */
