/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __POSIX_RTOS_PANIC_H__

#ifndef __ARCH_DEBUG_PANIC_H__
#define __ARCH_DEBUG_PANIC_H__

#include <stdint.h>

static inline void arch_dump_regs(void *dump_buf, uintptr_t stack_ptr,
				  uintptr_t *epc1) { }

#endif /* __ARCH_DEBUG_PANIC_H__ */

#else

#error "This file shouldn't be included from outside of XTOS's rtos/panic.h"

#endif /* __POSIX_RTOS_PANIC_H__ */
