/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __POSIX_RTOS_PANIC_H__
#define __POSIX_RTOS_PANIC_H__

#include <ipc/trace.h>
#include <stdint.h>
#include <arch/debug/panic.h>

#ifdef __ZEPHYR__
#error "This file should only be included in XTOS builds."
#endif /* __ZEPHYR__ */

#ifdef __clang_analyzer__
#define SOF_NORETURN __attribute__((analyzer_noreturn))
#elif __GNUC__
#define SOF_NORETURN __attribute__((noreturn))
#else
#define SOF_NORETURN
#endif

#ifndef RELATIVE_FILE
#error "This file requires RELATIVE_FILE to be defined. "\
	"Add it to CMake's target with sof_append_relative_path_definitions."
#endif

void dump_panicinfo(void *addr, struct sof_ipc_panic_info *panic_info);
void panic_dump(uint32_t p, struct sof_ipc_panic_info *panic_info,
		uintptr_t *data) SOF_NORETURN;
void __panic(uint32_t p, char *filename, uint32_t linenum) SOF_NORETURN;

/** panic dump filename and linenumber of the call
 *
 * \param x panic code defined in ipc/trace.h
 */
#define sof_panic(x) __panic((x), (RELATIVE_FILE), (__LINE__))

/* runtime assertion */
#ifndef assert
#define assert(cond) (void)((cond) || (sof_panic(SOF_IPC_PANIC_ASSERT), 0))
#endif

#endif /* __POSIX_RTOS_PANIC_H__ */
