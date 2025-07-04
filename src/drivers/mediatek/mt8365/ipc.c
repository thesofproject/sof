// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#include <rtos/panic.h>
#include <rtos/interrupt.h>
#include <sof/ipc/driver.h>
#include <sof/ipc/msg.h>
#include <sof/ipc/schedule.h>
#include <rtos/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/mailbox.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <rtos/wait.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <rtos/spinlock.h>
#include <ipc/header.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <platform/drivers/mt_reg_base.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

SOF_DEFINE_REG_UUID(ipc_task);

static struct ipc *local_ipc;

struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

static void irq_handler(void *args)
{
	uint32_t status;

	/* Interrupt arrived, check src */
	status = mailbox_sw_reg_read(SRAM_REG_OP_CPU2DSP);

	tr_dbg(&ipc_tr, "ipc: irq isr 0x%x", status);

	/* reply message(done) from host */
	if (status == ADSP_IPI_OP_RSP) {
		/* clear interrupt */
		io_reg_update_bits(DSP_RG_INT2CIRQ, CPU2DSP_IRQ, 0);
		local_ipc->is_notification_pending = false;
	}

	/* new message from host */
	if (status == ADSP_IPI_OP_REQ) {
		/* clear interrupt */
		io_reg_update_bits(DSP_RG_INT2CIRQ, CPU2DSP_IRQ, 0);
		ipc_schedule_process(local_ipc);
	}
}

/* DSP -> HOST RSP */
void trigger_irq_to_host_rsp(void)
{
	mailbox_sw_reg_write(SRAM_REG_OP_DSP2CPU, ADSP_IPI_OP_RSP);
	/* trigger host irq */
	io_reg_update_bits(DSP_RG_INT2CIRQ, DSP2SPM_IRQ_B, 0);
	io_reg_update_bits(DSP_RG_INT2CIRQ, DSP2CPU_IRQ, DSP2CPU_IRQ);
}

/* DSP -> HOST REQ */
void trigger_irq_to_host_req(void)
{
	mailbox_sw_reg_write(SRAM_REG_OP_DSP2CPU, ADSP_IPI_OP_REQ);
	/* trigger host irq */
	io_reg_update_bits(DSP_RG_INT2CIRQ, DSP2SPM_IRQ_B, 0);
	io_reg_update_bits(DSP_RG_INT2CIRQ, DSP2CPU_IRQ, DSP2CPU_IRQ);
}

enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	struct ipc_cmd_hdr *hdr;

	hdr = mailbox_validate();
	ipc_cmd(hdr);

	return SOF_TASK_STATE_COMPLETED;
}

void ipc_platform_complete_cmd(struct ipc *ipc)
{
	trigger_irq_to_host_rsp();
	while (ipc->pm_prepare_D3) {
		clock_set_freq(CLK_CPU(cpu_get_id()), CLK_SUSPEND_CPU_HZ);
		asm volatile("waiti 15");
	}
}

int ipc_platform_send_msg(const struct ipc_msg *msg)
{
	struct ipc *ipc = ipc_get();

	if (ipc->is_notification_pending)
		return -EBUSY;

	/* now send the message */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);

	tr_dbg(&ipc_tr, "ipc: msg tx -> 0x%x", msg->header);

	ipc->is_notification_pending = true;

	/* now interrupt host to tell it we have sent a message */
	trigger_irq_to_host_req();
	return 0;
}

void ipc_platform_send_msg_direct(const struct ipc_msg *msg)
{
	/* TODO: add support */
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
	uint32_t ipi_irq;

#if CONFIG_HOST_PTABLE
	struct ipc_data *iipc;

	iipc = rzalloc(SOF_MEM_FLAG_KERNEL, sizeof(*iipc));
	if (!iipc) {
		tr_err(&ipc_tr, "Unable to allocate IPC private data");
		return -ENOMEM;
	}
	ipc_set_drvdata(ipc, iipc);
#else
	ipc_set_drvdata(ipc, NULL);
#endif

	local_ipc = ipc;

	/* schedule */
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_uuid), &ipc_task_ops, ipc, 0, 0);

#if CONFIG_HOST_PTABLE
	/* allocate page table buffer */
	iipc->dh_buffer.page_table =
		rzalloc(SOF_MEM_FLAG_KERNEL, PLATFORM_PAGE_TABLE_SIZE);

	iipc->dh_buffer.dmac = dma_get(DMA_DIR_HMEM_TO_LMEM, 0, DMA_DEV_HOST, DMA_ACCESS_SHARED);
	if (!iipc->dh_buffer.dmac) {
		tr_err(&ipc_tr, "Unable to find DMA for host page table");
		sof_panic(SOF_IPC_PANIC_IPC);
	}
#endif

	ipi_irq = mtk_irq_group_id(LX_MCU_IRQ_B);
	interrupt_register(ipi_irq, irq_handler, ipc);
	interrupt_enable(ipi_irq, ipc);

	return 0;
}
