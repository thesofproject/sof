/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifdef __PLATFORM_DRIVERS_IDC_H__

#ifndef __ACE_DRIVERS_IDC_H__
#define __ACE_DRIVERS_IDC_H__

struct idc_msg;

int idc_send_msg(struct idc_msg *msg, uint32_t mode);

int idc_init(void);

#endif /* __ACE_DRIVERS_IDC_H__ */

#else

#error "This file shouldn't be included from outside of platform/drivers/idc.h"

#endif /* __PLATFORM_DRIVERS_IDC_H__ */
