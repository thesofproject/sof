/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __PLATFORM_DRIVERS_INTERRUPT_H__

#ifndef __CAVS_DRIVERS_INTERRUPT_H__
#define __CAVS_DRIVERS_INTERRUPT_H__

extern char irq_name_level2[];
extern char irq_name_level3[];
extern char irq_name_level4[];
extern char irq_name_level5[];

#endif /* __CAVS_DRIVERS_INTERRUPT_H__ */

#else

#error "This file shouldn't be included from outside of platform/drivers/interrupt.h"

#endif /* __PLATFORM_DRIVERS_INTERRUPT_H__ */
