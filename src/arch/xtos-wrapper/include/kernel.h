/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Jyri Sarha <jyri.sarha@intel.com>
 */

#ifndef __XTOS_WRAPPER_KERNEL_H__
#define __XTOS_WRAPPER_KERNEL_H__

#include <sof/platform.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#include <stddef.h>
#include <stdint.h>

extern struct tr_ctx wait_tr;

static inline void k_yield(void)
{
	tr_dbg(&wait_tr, "WFE");
#if CONFIG_DEBUG_LOCKS
	if (lock_dbg_atomic)
		tr_err_atomic(&wait_tr, "atm");
#endif
	platform_wait_for_interrupt(0);
	tr_dbg(&wait_tr, "WFX");
}

#endif /* __XTOS_WRAPPER_KERNEL_H__ */
