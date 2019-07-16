/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_DEBUG_PANIC_H__
#define __SOF_DEBUG_PANIC_H__

#include <ipc/trace.h>
#include <stdint.h>

void dump_panicinfo(void *addr, struct sof_ipc_panic_info *panic_info);
void panic_rewind(uint32_t p, uint32_t stack_rewind_frames,
		  struct sof_ipc_panic_info *panic_info, uintptr_t *data);
void __panic(uint32_t p, char *filename, uint32_t linenum);

/* panic dump filename and linenumber of the call */
#define panic(x) __panic((x), (__FILE__), (__LINE__))

/* runtime assertion */
#define assert(cond) (void)((cond) || (panic(SOF_IPC_PANIC_ASSERT), 0))

#endif /* __SOF_DEBUG_PANIC_H__ */
