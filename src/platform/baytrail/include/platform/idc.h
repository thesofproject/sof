/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __INCLUDE_PLATFORM_IDC_H__
#define __INCLUDE_PLATFORM_IDC_H__

struct idc_msg;

static inline int idc_send_msg(struct idc_msg *msg,
			       uint32_t mode) { return 0; }

static inline int idc_init(void) { return 0; }

#endif
