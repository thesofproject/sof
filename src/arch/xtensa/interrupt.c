// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/drivers/interrupt.h>
#include <sof/lib/clk.h>

static void (*handlers[XCHAL_NUM_INTERRUPTS])(void *arg);
static void *handlers_args[XCHAL_NUM_INTERRUPTS];

/* We set clock to 120mHz before waiti,
 * so on interrupt we have to set it back to 400mHz,
 * then invoke registered interrupt handler.
 */
static void proxy_handler(void *arg)
{
	void (*f)(void *) = handlers[(uint32_t)arg];

	clock_set_high_freq();
	if (f)
		f(handlers_args[(uint32_t)arg]);
}

void arch_interrupt_set_proxy(int irq,
	void (*handler)(void *arg), void *arg)
{
	handlers[irq] = handler;
	handlers_args[irq] = arg;
	_xtos_set_interrupt_handler_arg(irq, proxy_handler, (void *)irq);
}
