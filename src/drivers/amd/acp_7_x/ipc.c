// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 AMD. All rights reserved.
 *
 * Author: Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *         Sneha Voona <sneha.voona@amd.com>
 *         DineshKumar Kalva <DineshKumar.Kalva@amd.com>
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
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/platform.h>
#include <platform/ipc.h>
#include <zephyr/logging/log.h>

volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *)(PU_SCRATCH_REG_BASE +
							SCRATCH_REG_OFFSET);

SOF_DEFINE_REG_UUID(ipc_task_amd);
LOG_MODULE_REGISTER(ipc_handler_file, LOG_LEVEL_DBG);

#define HOST_TO_DSP_INTR 1
#define INTERRUPT_DISABLE 0
#define IRQ_NUM_EXT_LEVEL3 3
#define IRQ_EXT_IPC_LEVEL_3 3

/* This function triggers a host interrupt from ACP DSP */
void acp_dsp_to_host_intr_trig(void)
{
	acp_sw_intr_trig_t  sw_intr_trig;
	/* Read the Software Interrupt controller register and update */
	sw_intr_trig = (acp_sw_intr_trig_t) io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
	/* Configures the trigger bit in ACP_DSP_SW_INTR_TRIG register */
	sw_intr_trig.bits.trig_dsp0_to_host_intr = INTERRUPT_ENABLE;
	/* Write the Software Interrupt controller register */
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), sw_intr_trig.u32all);
}

/* Clear the Acknowledge ( status) for the host to DSP interrupt */
static void acp_ack_intr_from_host(void)
{
	/* acknowledge the host interrupt */
	acp_dsp_sw_intr_stat_t sw_intr_stat;

	sw_intr_stat.u32all = 0;
	sw_intr_stat.bits.host_to_dsp0_intr1_stat = INTERRUPT_ENABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_DSP_SW_INTR_STAT), sw_intr_stat.u32all);
}

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
	interrupt_disable(IRQ_EXT_IPC_LEVEL_3, ipc);
	interrupt_unregister(IRQ_EXT_IPC_LEVEL_3, ipc);
	interrupt_register(IRQ_EXT_IPC_LEVEL_3, amd_irq_handler, ipc);
	/* Enabling software interuppts */
	interrupt_enable(IRQ_EXT_IPC_LEVEL_3, ipc);
	return 0;
}

void amd_irq_handler(void *arg)
{
	struct ipc *ipc = arg;
	uint32_t status;
	uint32_t lock;
	acp_dsp_sw_intr_stat_t swintrstat;
	acp_sw_intr_trig_t  swintrtrig;

	swintrstat = (acp_dsp_sw_intr_stat_t)io_reg_read(PU_REGISTER_BASE + ACP_DSP_SW_INTR_STAT);
	status = swintrstat.u32all &  HOST_TO_DSP_INTR;
	if (status) {
		/* Interrupt source */
		lock = io_reg_read(PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0);
		/* Removed unbounded while(lock) loop - using bounded loop below instead */
		int timeout = 20000;

		while (lock && --timeout > 0) {
			lock = io_reg_read(PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0);
		}
		if (timeout == 0) {
			/* Give up, don't block forever */
			LOG_ERR("ACP semaphore timeout in IRQ handler");
			return;
		}
		/***************************************************** */
		/* Configures the trigger bit in ACP_DSP_SW_INTR_TRIG register */
		swintrtrig = (acp_sw_intr_trig_t)
			io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
		swintrtrig.bits.trig_host_to_dsp0_intr1 = INTERRUPT_DISABLE;
		swintrtrig.bits.trig_dsp0_to_host_intr = INTERRUPT_DISABLE;
		io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG),
					 swintrtrig.u32all);

		/* Clear the Host to DSP Status Register */
		acp_ack_intr_from_host();
		/********************************************* */

		if (sof_ipc_host_status()) {
			if (sof_ipc_host_ack_flag()) {
				/* Clear the ACK from host  */
				sof_ipc_host_ack_clear();
			}
			/* Check if new message from host */
			if (sof_ipc_host_msg_flag()) {
				/* Clear the msg bit from host*/
				sof_ipc_host_msg_clear();
				ipc_schedule_process(ipc);
			}
		} else {
			tr_err(&ipc_tr, "IPC:interrupt without setting flags host status 0x%x", sof_ipc_host_status());
			io_reg_write((PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0), 0);
		}
	}
}

void ipc_platform_complete_cmd(struct ipc *ipc)
{
	acp_sw_intr_trig_t  sw_intr_trig;

	/* Set Dsp Ack for msg from host */
	sof_ipc_dsp_ack_set();
	/* Configures the trigger bit in ACP_DSP_SW_INTR_TRIG register */
	sw_intr_trig = (acp_sw_intr_trig_t)
		io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
	sw_intr_trig.bits.trig_host_to_dsp0_intr1  = INTERRUPT_DISABLE;
	sw_intr_trig.bits.trig_dsp0_to_host_intr = INTERRUPT_DISABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), sw_intr_trig.u32all);
	/* now interrupt host to tell it we have sent a message */
	acp_dsp_to_host_intr_trig();
	/* Disable the trigger bit in ACP_DSP_SW_INTR_TRIG register */
	sw_intr_trig = (acp_sw_intr_trig_t)io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
	sw_intr_trig.bits.trig_dsp0_to_host_intr = INTERRUPT_DISABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), sw_intr_trig.u32all);
	io_reg_write((PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0), 0);
	LOG_DBG("IPC cmd completed");
	if (ipc->pm_prepare_D3) {
		while (1) {
			wait_for_interrupt(0);
		}
	}
}

int ipc_platform_send_msg(const struct ipc_msg *msg)
{
	int ret = 0;
	acp_sw_intr_trig_t  sw_intr_trig;
	acp_dsp_sw_intr_stat_t sw_intr_stat;
	uint32_t status;
	uint32_t lock;
	/* Check if host cleared the status for previous messages */
	sw_intr_stat = (acp_dsp_sw_intr_stat_t)
		io_reg_read(PU_REGISTER_BASE + ACP_DSP_SW_INTR_STAT);
	status =  sw_intr_stat.bits.dsp0_to_host_intr_stat;
	if (sof_ipc_dsp_status() || status) {
		return -EBUSY;
	}
	lock = io_reg_read(PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0);
	int timeout = 20000;

	while (lock && --timeout > 0) {
		lock = io_reg_read(PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0);
	}
	if (timeout == 0) {
		/* Give up, don't block forever */
		LOG_ERR("ACP semaphore timeout in IPC send");
		return -EBUSY;
	}

	/* Write new message in the mailbox */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	/* Need to set DSP message flag */
	sof_ipc_dsp_msg_set();
	/* Trigger host interrupt to notify new message from DSP */
	/* Configures the trigger bit in ACP_DSP_SW_INTR_TRIG register */
	sw_intr_trig = (acp_sw_intr_trig_t)
		io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
	sw_intr_trig.bits.trig_host_to_dsp0_intr1 = INTERRUPT_DISABLE;
	sw_intr_trig.bits.trig_dsp0_to_host_intr = INTERRUPT_DISABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), sw_intr_trig.u32all);
	acp_dsp_to_host_intr_trig();
	/* Disable the trigger bit in ACP_DSP_SW_INTR_TRIG register */
	sw_intr_trig = (acp_sw_intr_trig_t)io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
	sw_intr_trig.bits.trig_dsp0_to_host_intr = INTERRUPT_DISABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), sw_intr_trig.u32all);
	io_reg_write((PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0), lock);
	return ret;
}

void ipc_platform_send_msg_direct(const struct ipc_msg *msg)
{
	acp_sw_intr_trig_t sw_intr_trig;

	/* Write message directly to mailbox - no status checks for emergency */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);

	/* Set DSP message flag */
	sof_ipc_dsp_msg_set();

	/* Interrupt host immediately */
	acp_dsp_to_host_intr_trig();

	/* Disable the trigger bit */
	sw_intr_trig = (acp_sw_intr_trig_t)io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
	sw_intr_trig.bits.trig_dsp0_to_host_intr = INTERRUPT_DISABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), sw_intr_trig.u32all);
}
