// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Mediatek. All rights reserved.
//
// Author: Allen-KH Cheng <allen-kh.cheng@mediatek.com>
//         Tinghan Shen <tinghan.shen@mediatek.com>

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

/* a3fe3bf2-39a4-4fc3-b341-8a96e0a26759 */
DECLARE_SOF_UUID("ipc-task", ipc_task_uuid, 0xa3fe3bf2, 0x39a4, 0x4fc3,
		 0xb3, 0x41, 0x8a, 0x96, 0xe0, 0xa2, 0x67, 0x59);

static struct ipc *local_ipc;

struct ipc_data {
	struct ipc_data_host_buffer dh_buffer;
};

static void mbox0_handler(void *args)
{
	uint32_t op = io_reg_read(MTK_ADSP_MBOX_IN_CMD(0));

	/* clear interrupt */
	io_reg_write(MTK_ADSP_MBOX_IN_CMD_CLR(0), op);
	ipc_schedule_process(local_ipc);
}

static void mbox1_handler(void *args)
{
	uint32_t op = io_reg_read(MTK_ADSP_MBOX_IN_CMD(1));

	/* clear interrupt */
	io_reg_write(MTK_ADSP_MBOX_IN_CMD_CLR(1), op);
	local_ipc->is_notification_pending = false;
}

void trigger_irq_to_host_rsp(void)
{
	io_reg_write(MTK_ADSP_MBOX_OUT_CMD(0), ADSP_IPI_OP_RSP);
}

void trigger_irq_to_host_req(void)
{
	io_reg_write(MTK_ADSP_MBOX_OUT_CMD(1), ADSP_IPI_OP_REQ);
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
	while (ipc->pm_prepare_D3)
		wait_for_interrupt(0);
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
	int mbox_irq0, mbox_irq1;
	int ret;

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

	/*
	 * AP req -- mbox0 --> DSP
	 *    AP <-- mbox0 --  DSP rsp
	 *    AP <-- mbox1 --  DSP req
	 * AP rsp -- mbox1 --> DSP
	 */
	mbox_irq0 = mtk_irq_group_id(MTK_DSP_IRQ_MBOX0);
	if (mbox_irq0 < 0) {
		tr_err(&ipc_tr, "Invalid ipc mbox 0 IRQ:%d", mbox_irq0);
		panic(SOF_IPC_PANIC_IPC);
	}

	mbox_irq1 = mtk_irq_group_id(MTK_DSP_IRQ_MBOX1);
	if (mbox_irq1 < 0) {
		tr_err(&ipc_tr, "Invalid ipc mbox 0 IRQ:%d", mbox_irq1);
		panic(SOF_IPC_PANIC_IPC);
	}

	ret = interrupt_register(mbox_irq0, mbox0_handler, ipc);
	if (ret < 0) {
		tr_err(&ipc_tr, "Unable to register ipc mbox 0 IRQ");
		panic(SOF_IPC_PANIC_IPC);
	}

	ret = interrupt_register(mbox_irq1, mbox1_handler, ipc);
	if (ret < 0) {
		tr_err(&ipc_tr, "Unable to register ipc mbox 1 IRQ");
		panic(SOF_IPC_PANIC_IPC);
	}

	interrupt_enable(mbox_irq0, ipc);
	interrupt_enable(mbox_irq1, ipc);

	return 0;
}
