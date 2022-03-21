/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jyri Sarha <jyri.sarha@intel.com>
 */

#ifndef __XTOS_WRAPPER_KERNEL_H__
#define __XTOS_WRAPPER_KERNEL_H__

#include <sof/lib/wait.h>

#include <stdint.h>

static inline void k_msleep(int32_t ms)
{
	wait_delay_ms(ms);
}

static inline void k_usleep(int32_t us)
{
	wait_delay_us(us);
}

#endif /* __XTOS_WRAPPER_KERNEL_H__ */
