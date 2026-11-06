// SPDX-License-Identifier: BSD-3-Clause
//
//Copyright(c) 2022, 2026 AMD. All rights reserved.
//
//Author:       Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
//              Bala Kishore <balakishore.pati@amd.com>
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

#define HOST_TO_DSP_INTR 1
#define INTERRUPT_DISABLE 0
LOG_MODULE_REGISTER(ipc1, CONFIG_SOF_LOG_LEVEL);
volatile acp_scratch_mem_config_t *pscratch_mem_cfg = (volatile acp_scratch_mem_config_t *)
						      (PU_SCRATCH_REG_BASE + SCRATCH_REG_OFFSET);

#ifdef CONFIG_ZEPHYR_NATIVE_DRIVERS
/* Clear the Acknowledge ( status) for the host to DSP interrupt */
void acp_ack_intr_from_host(void)
{
	/* acknowledge the host interrupt */
	acp_dsp_sw_intr_stat_t sw_intr_stat;

	sw_intr_stat.u32all = 0;
	sw_intr_stat.bits.host_to_dsp0_intr1_stat = INTERRUPT_ENABLE;
	io_reg_write((PU_REGISTER_BASE + ACP_DSP_SW_INTR_STAT), sw_intr_stat.u32all);
}

/* This function triggers a host interrupt from ACP DSP */
void acp_dsp_to_host_intr_trig(void)
{
	acp_sw_intr_trig_t  sw_intr_trig;

	/* Read the Software Interrupt controller register and update */
	sw_intr_trig = (acp_sw_intr_trig_t)io_reg_read(PU_REGISTER_BASE +
						ACP_SW_INTR_TRIG);
	/* Configures the trigger bit in ACP_DSP_SW_INTR_TRIG register */
	sw_intr_trig.bits.trig_dsp0_to_host_intr = INTERRUPT_ENABLE;
	/* Write the Software Interrupt controller register */
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), sw_intr_trig.u32all);
}
#endif /* CONFIG_ZEPHYR_NATIVE_DRIVERS */

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
		if (sof_ipc_host_status()) {
			lock = io_reg_read(PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0);
			while (lock)
				lock = io_reg_read(PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0);
			/* Check if it is response from host */
			if (sof_ipc_host_ack_flag()) {
				/* Clear the ACK from host  */
				sof_ipc_host_ack_clear();
				/* Clear the Host to DSP Status Register */
				acp_ack_intr_from_host();
				/* Configures the trigger bit in ACP_DSP_SW_INTR_TRIG register */
				swintrtrig = (acp_sw_intr_trig_t)
					     io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
				swintrtrig.bits.trig_host_to_dsp0_intr1	= INTERRUPT_DISABLE;
				swintrtrig.bits.trig_dsp0_to_host_intr	= INTERRUPT_DISABLE;
				io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG),
					     swintrtrig.u32all);
			}
			/* Check if new message from host */
			if (sof_ipc_host_msg_flag()) {
				/* Clear the msg bit from host */
				sof_ipc_host_msg_clear();
				/* Clear the Host to DSP Status Register */
				acp_ack_intr_from_host();
				ipc_schedule_process(ipc);
			}
			io_reg_write((PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0), lock);
		} else {
			tr_err(&ipc_tr, "IPC:interrupt without setting flags host status 0x%x",
			       sof_ipc_host_status());
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
	if (ipc->pm_prepare_D3) {
		while (1)
			wait_for_interrupt(0);
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
		sw_intr_stat = (acp_dsp_sw_intr_stat_t)
				io_reg_read(PU_REGISTER_BASE + ACP_DSP_SW_INTR_STAT);
		status =  sw_intr_stat.bits.dsp0_to_host_intr_stat;
		return -EBUSY;
	}
	lock = io_reg_read(PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0);
	while (lock)
		lock = io_reg_read(PU_REGISTER_BASE + ACP_AXI2DAGB_SEM_0);
	/* Write new message in the mailbox */
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	/* Need to set DSP message flag */
	sof_ipc_dsp_msg_set();
	/* now interrupt host to tell it we have sent a message */
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
	/* TODO: add support */
}

