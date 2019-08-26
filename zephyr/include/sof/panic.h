/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 */

#ifndef __INCLUDE_SOF_IPC_PANIC__
#define __INCLUDE_SOF_IPC_PANIC__

#include <stdint.h>
#include <ipc/trace.h>

void __panic(uint32_t p, char *filename, uint32_t linenum);

/* panic dump filename and linenumber of the call */
#define panic(x) __panic((x), (__FILE__), (__LINE__))

/* Because of SOF way of using assert() by including the code inside it
 * we cannot use Zephyr assert directly
 */
#ifdef assert
#undef assert
#endif

#define assert(cond) (void)((cond) || (panic(SOF_IPC_PANIC_ASSERT), 0))

#endif
