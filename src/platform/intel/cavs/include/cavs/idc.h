/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __INCLUDE_CAVS_IDC__
#define __INCLUDE_CAVS_IDC__

#include <arch/idc.h>

static inline int idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	return arch_idc_send_msg(msg, mode);
}

static inline int idc_init(void)
{
	return arch_idc_init();
}

#endif
