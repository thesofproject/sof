/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#if defined(__XTOS_RTOS_IDC_H__) || defined(__ZEPHYR_RTOS_IDC_H__)

#ifndef __PLATFORM_DRIVERS_IDC_H__
#define __PLATFORM_DRIVERS_IDC_H__

#include <cavs/drivers/idc.h>

#endif /* __PLATFORM_DRIVERS_IDC_H__ */

#else

#error "This file shouldn't be included from outside of Zephyr/XTOS's rtos/idc.h"

#endif
