/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __POSIX_RTOS_IDC_H__

#ifndef __PLATFORM_DRIVERS_IDC_H__
#define __PLATFORM_DRIVERS_IDC_H__

#include <stdint.h>

struct idc_msg;

static inline int idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	return 0;
}

static inline void idc_process_msg_queue(void)
{
}

static inline int idc_init(void)
{
	return 0;
}

static inline int idc_restore(void)
{
	return 0;
}

#endif /* __PLATFORM_DRIVERS_IDC_H__ */

#else

#error "This file shouldn't be included from outside of Zephyr/XTOS's rtos/idc.h"

#endif
