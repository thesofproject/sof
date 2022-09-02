// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <rtos/interrupt.h>

#if CONFIG_WAKEUP_HOOK
void arch_interrupt_on_wakeup(void)
{
	platform_interrupt_on_wakeup();
}
#endif
