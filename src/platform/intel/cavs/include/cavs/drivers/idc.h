/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __PLATFORM_DRIVERS_IDC_H__

#ifndef __CAVS_DRIVERS_IDC_H__
#define __CAVS_DRIVERS_IDC_H__

#include <stdint.h>

struct idc_msg;

#if CONFIG_MULTICORE

int idc_send_msg(struct idc_msg *msg, uint32_t mode);

int idc_init(void);

#else

static inline int idc_send_msg(struct idc_msg *msg, uint32_t mode) { return 0; }

static inline int idc_init(void) { return 0; }

#endif

#endif /* __CAVS_DRIVERS_IDC_H__ */

#else

#error "This file shouldn't be included from outside of platform/drivers/idc.h"

#endif /* __PLATFORM_DRIVERS_IDC_H__ */
