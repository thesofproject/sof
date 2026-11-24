/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024, 2026 AMD.All rights reserved.
 *
 * Author: SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
 * Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 * Sivasubramanian <sravisar@amd.com>
 */
#ifdef __SOF_PLATFORM_H__

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <rtos/interrupt.h>
#include <rtos/clk.h>
#include <sof/lib/mailbox.h>
#include <stddef.h>
#include <stdint.h>
#include <platform/fw_scratch_mem.h>
#include <platform/platform_misc.h>

#define INTERRUPT_ENABLE 1
struct timer;

#define PLATFORM_DEFAULT_CLOCK CLK_CPU(0)

/* IPC Interrupt */
#define PLATFORM_IPC_INTERRUPT		IRQ_EXT_IPC_LEVEL_3

/* Host page size */
#define HOST_PAGE_SIZE                  65536

/* default dma trace channel */
#define DMA_TRACE_CHANNEL	7

/* debug offset */
#define ACP_SOF_FW_STATUS	0

/* Platform defined panic code */
static inline void platform_panic(uint32_t p)
{
	acp_sw_intr_trig_t  sw_intr_trig;
	volatile acp_scratch_mem_config_t *pscratch_mem_cfg =
		(volatile acp_scratch_mem_config_t *)(PU_SCRATCH_REG_BASE + SCRATCH_REG_OFFSET);

	pscratch_mem_cfg->acp_dsp_msg_write = p;
	mailbox_sw_reg_write(ACP_SOF_FW_STATUS, p);
	/* Read the Software Interrupt controller register and update */
	sw_intr_trig = (acp_sw_intr_trig_t)io_reg_read(PU_REGISTER_BASE + ACP_SW_INTR_TRIG);
	/* Configures the trigger bit in ACP_DSP_SW_INTR_TRIG register */
	sw_intr_trig.bits.trig_dsp0_to_host_intr = INTERRUPT_ENABLE;
	/* Write the Software Interrupt trigger register */
	io_reg_write((PU_REGISTER_BASE + ACP_SW_INTR_TRIG), sw_intr_trig.u32all);
}

/*
 * brief Platform specific CPU entering idle.
 * May be power-optimized using platform specific capabilities.
 * @param level Interrupt level.
 */
void platform_wait_for_interrupt(int level);

extern intptr_t _module_init_start;
extern intptr_t _module_init_end;
#endif

#endif /* __PLATFORM_PLATFORM_H__ */

#else

#error "This file shouldn't be included from outside of sof/kernel/platform.h"

#endif /* __SOF_PLATFORM_H__ */
