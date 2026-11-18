// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2023, 2026 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
//              Sivasubramanian <sravisar@amd.com>

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
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/platform.h>
#include <platform/ipc.h>

SOF_DEFINE_REG_UUID(ipc_task_amd);

extern volatile acp_scratch_mem_config_t *pscratch_mem_cfg;

inline uint32_t sof_ipc_host_status(void)
{
	return (pscratch_mem_cfg->acp_host_ack_write | pscratch_mem_cfg->acp_host_msg_write);
}

inline uint32_t sof_ipc_host_msg_flag(void)
{
	return pscratch_mem_cfg->acp_host_msg_write;
}

inline uint32_t sof_ipc_host_ack_flag(void)
{
	return pscratch_mem_cfg->acp_host_ack_write;
}

inline uint32_t sof_ipc_dsp_status(void)
{
	return (pscratch_mem_cfg->acp_dsp_msg_write | pscratch_mem_cfg->acp_dsp_ack_write);
}

inline void sof_ipc_host_ack_clear(void)
{
	pscratch_mem_cfg->acp_host_ack_write = 0;
}

inline void sof_ipc_host_msg_clear(void)
{
	pscratch_mem_cfg->acp_host_msg_write = 0;
}

inline void sof_ipc_dsp_ack_set(void)
{
	pscratch_mem_cfg->acp_dsp_ack_write = 1;
}

inline void sof_ipc_dsp_msg_set(void)
{
	pscratch_mem_cfg->acp_dsp_msg_write = 1;
}

enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	struct ipc_cmd_hdr *hdr;

	hdr = mailbox_validate();
	ipc_cmd(hdr);
	return SOF_TASK_STATE_COMPLETED;
}

int platform_ipc_init(struct ipc *ipc)
{
	ipc_set_drvdata(ipc, NULL);
	/* schedule */
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_amd_uuid),
			       &ipc_task_ops, ipc, 0, 0);
	interrupt_disable(IRQ_NUM_EXT_LEVEL3, ipc);
	interrupt_register(IRQ_NUM_EXT_LEVEL3, amd_irq_handler, ipc);
	/* Enabling software interuppts */
	interrupt_enable(IRQ_NUM_EXT_LEVEL3, ipc);
	return 0;
}
