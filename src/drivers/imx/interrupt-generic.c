// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2021 NXP
//
// Author: Peng Zhang <peng.zhang_8@nxp.com>

#include <sof/common.h>
#include <rtos/interrupt.h>
#include <sof/lib/cpu.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/spinlock.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(generic_irq_imx, CONFIG_SOF_LOG_LEVEL);

/* fa00558c-d653-4851-a03a-b21f125a9524 */
DECLARE_SOF_UUID("generic-irq-imx", generic_irq_imx_uuid, 0xfa00558c, 0xd653, 0x4851,
		 0xa0, 0x3a, 0xb2, 0x1f, 0x12, 0x5a, 0x95, 0x24);

DECLARE_TR_CTX(noirq_i_tr, SOF_UUID(generic_irq_imx_uuid), LOG_LEVEL_INFO);

/* this is needed because i.MX8 implementation assumes all boards have an irqsteer.
 * Needs to be fixed.
 */
int irqstr_get_sof_int(int irqstr_int)
{
	return irqstr_int;
}

void platform_interrupt_init(void) {}

void platform_interrupt_set(uint32_t irq)
{
	arch_interrupt_set(irq);
}

void platform_interrupt_clear(uint32_t irq, uint32_t mask)
{
	arch_interrupt_clear(irq);
}

uint32_t platform_interrupt_get_enabled(void)
{
	return 0;
}

void interrupt_mask(uint32_t irq, unsigned int cpu) {}

void interrupt_unmask(uint32_t irq, unsigned int cpu) {}
