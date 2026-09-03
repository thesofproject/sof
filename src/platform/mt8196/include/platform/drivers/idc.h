/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Hailong Fan <hailong.fan@mediatek.com>
 */

#if defined(__XTOS_RTOS_IDC_H__) || defined(__ZEPHYR_RTOS_IDC_H__)

#ifndef __PLATFORM_DRIVERS_IDC_H__
#define __PLATFORM_DRIVERS_IDC_H__

#include <stdint.h>

struct idc_msg;

static inline int idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	return 0;
}

static inline int idc_init(void)
{
	return 0;
}

#endif /* __PLATFORM_DRIVERS_IDC_H__ */

#else

#error "This file shouldn't be included from outside of Zephyr/XTOS's rtos/idc.h"

#endif
