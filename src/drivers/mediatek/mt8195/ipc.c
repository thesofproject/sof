// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Mediatek
//
// Author: Allen-KH Cheng <Allen-KH.Cheng@mediatek.com>

#include <sof/debug/panic.h>
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
#include <sof/schedule/task.h>
#include <rtos/spinlock.h>
#include <ipc/header.h>
#include <ipc/topology.h>
#include <ipc/trace.h>
#include <platform/drivers/mt_reg_base.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IPC_DSPMBOX_DSP_RSP 0
#define IPC_DSPMBOX_DSP_REQ 1

/* 389c9186-5a7d-4ad1-a02c-a02ecdadfb33 */
DECLARE_SOF_UUID("ipc-task", ipc_task_uuid, 0x389c9186, 0x5a7d, 0x4ad1,
		 0xa0, 0x2c, 0xa0, 0x2e, 0xcd, 0xad, 0xfb, 0x33);

static struct ipc *local_ipc;

struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

static void mbox0_handler(void *args)
{
	uint32_t op = io_reg_read(MTK_DSP_MBOX_IN_CMD(0));

	/* clear interrupt */
	io_reg_write(MTK_DSP_MBOX_IN_CMD_CLR(0), op);
	ipc_schedule_process(local_ipc);
}

static void mbox1_handler(void *args)
{
	uint32_t op = io_reg_read(MTK_DSP_MBOX_IN_CMD(1));

	/* clear interrupt */
	io_reg_write(MTK_DSP_MBOX_IN_CMD_CLR(1), op);
	local_ipc->is_notification_pending = false;
}

void trigger_irq_to_host_rsp(void)
{
	io_reg_write(MTK_DSP_MBOX_OUT_CMD(0), ADSP_IPI_OP_RSP);
}

void trigger_irq_to_host_req(void)
{
	io_reg_write(MTK_DSP_MBOX_OUT_CMD(1), ADSP_IPI_OP_REQ);
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

	ipc->is_notification_pending = true;

	/* now interrupt host to tell it we have sent a message */
	trigger_irq_to_host_req();
	return 0;
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
	uint32_t mbox_irq0, mbox_irq1;
#if CONFIG_HOST_PTABLE
	struct ipc_data *iipc;

	iipc = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, sizeof(*iipc));
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
		rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM, PLATFORM_PAGE_TABLE_SIZE);

	iipc->dh_buffer.dmac = dma_get(DMA_DIR_HMEM_TO_LMEM, 0, DMA_DEV_HOST, DMA_ACCESS_SHARED);
	if (!iipc->dh_buffer.dmac) {
		tr_err(&ipc_tr, "Unable to find DMA for host page table");
		panic(SOF_IPC_PANIC_IPC);
	}
#endif

	mbox_irq0 = mtk_get_irq_domain_id(LX_MBOX_IRQ0_B);
	mbox_irq1 = mtk_get_irq_domain_id(LX_MBOX_IRQ1_B);
	interrupt_register(mbox_irq0, mbox0_handler, ipc);
	interrupt_register(mbox_irq1, mbox1_handler, ipc);
	interrupt_enable(mbox_irq0, ipc);
	interrupt_enable(mbox_irq1, ipc);

	return 0;
}
