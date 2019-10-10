/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_WAIT_H_
#define __ARCH_WAIT_H_

#include <xtensa/xtruntime.h>
#include <arch/interrupt.h>
#include <sof/panic.h>

#include <ipc/trace.h>

#if defined(PLATFORM_WAITI_DELAY)

static inline void arch_wait_for_interrupt(int level)
{
	int i;

	/* can only enter WFI when at run level 0 i.e. not IRQ level */
	if (arch_interrupt_get_level() > 0)
		panic(SOF_IPC_PANIC_WFI);

	/* this sequence must be atomic on LX6 */
	XTOS_SET_INTLEVEL(5);

	/* LX6 needs a delay */
	for (i = 0; i < 128; i++)
		__asm__ volatile("nop");

	/* and to flush all loads/stores prior to wait */
	__asm__ volatile("isync");
	__asm__ volatile("extw");

	/* now wait */
	__asm__ volatile("waiti 0");
}

#else

static inline void arch_wait_for_interrupt(int level)
{
	/* can only enter WFI when at run level 0 i.e. not IRQ level */
	if (arch_interrupt_get_level() > 0)
		panic(SOF_IPC_PANIC_WFI);

	__asm__ volatile("waiti 0");
}

#endif

static inline void idelay(int n)
{
	while (n--)
		__asm__ volatile("nop");
}

#endif
