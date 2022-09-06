/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifdef __SOF_DRIVERS_INTERRUPT_H__

#ifndef __PLATFORM_DRIVERS_INTERRUPT_H__
#define __PLATFORM_DRIVERS_INTERRUPT_H__

#if !defined(__ASSEMBLER__) && !defined(LINKER)

#include <ace/drivers/interrupt.h>

#include <rtos/bit.h>

#endif

/* Required by sof/drivers/interrupt.h */
#define PLATFORM_IRQ_CHILDREN	32

/* Required by zephyr/wrapper.c */
#define IRQ_NUM_EXT_LEVEL2	4	/* level 2 */
#define IRQ_NUM_EXT_LEVEL5	16	/* level 5 */

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/interrupt.h"

#endif /* __SOF_DRIVERS_INTERRUPT_H__ */
