/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __INCLUDE_PLATFORM_IDC_H__
#define __INCLUDE_PLATFORM_IDC_H__

struct idc_msg;

static inline int idc_send_msg(struct idc_msg *msg,
			       uint32_t mode) { return 0; }

static inline void idc_init(void) { }

#endif
