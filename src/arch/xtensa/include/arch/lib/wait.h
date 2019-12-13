/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_LIB_WAIT_H__
#define __ARCH_LIB_WAIT_H__

#include <sof/debug/panic.h>
#include <sof/drivers/interrupt.h>
#include <sof/lib/clk.h>
#include <ipc/trace.h>
#include <config.h>
#include <xtensa/xtruntime.h>

#if (CONFIG_WAITI_DELAY)

static inline void arch_wait_for_interrupt(int level)
{
	int i;

	/* need to make sure the interrupt level won't be lowered */
	if (arch_interrupt_get_level() > level)
		panic(SOF_IPC_PANIC_WFI);

	/* while in waiti FW should use 120mHz clock on CNL platform */
	clock_set_low_freq();

	/* this sequence must be atomic on LX6 */
	XTOS_SET_INTLEVEL(5);

	/* LX6 needs a delay */
	for (i = 0; i < 128; i++)
		asm volatile("nop");

	/* and to flush all loads/stores prior to wait */
	asm volatile("isync");
	asm volatile("extw");

	/* now wait */
	asm volatile("waiti 0");
}

#else

static inline void arch_wait_for_interrupt(int level)
{
	/* need to make sure the interrupt level won't be lowered */
	if (arch_interrupt_get_level() > level)
		panic(SOF_IPC_PANIC_WFI);

	/* while in waiti FW should use 120mHz clock on CNL platform */
	clock_set_low_freq();

	asm volatile("waiti 0");
}

#endif

static inline void idelay(int n)
{
	while (n--)
		asm volatile("nop");
}

#endif /* __ARCH_LIB_WAIT_H__ */
