/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Google LLC.  All rights reserved.
 * Author: Andy Ross <andyross@google.com>
 */
#ifndef PLATFORM_POSIX_DRIVERS_IDC_H
#define PLATFORM_POSIX_DRIVERS_IDC_H

#include <stdint.h>
#include <rtos/idc.h>

struct idc_msg;

static inline int idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	return 0;
}

#endif /* PLATFORM_POSIX_DRIVERS_IDC_H */
