/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * \file arch/xtensa/include/arch/init.h
 * \brief Arch init header file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_INIT_H__

#ifndef __ARCH_INIT_H__
#define __ARCH_INIT_H__

#include <sof/debug/panic.h>
#include <sof/math/numbers.h>
#include <sof/string.h>
#include <ipc/trace.h>
#include <config.h>
#include <xtensa/corebits.h>
#include <xtensa/xtruntime.h>
#include <stddef.h>
#include <stdint.h>

/* When set to 1, then exceptions message contains exception name */
#define DETAILED_EXCEPTIONS 1

/**
 * \brief Called in the case of exception, pass extra information.
 *
 * \param name exception name
 * \param code exception extra numerical information
 */
static inline void exception_ext(const char *name, int code)
{
	uintptr_t epc1;
	struct sof_ipc_panic_info info;
	struct sof_ipc_panic_info *pinfo;

	__asm__ __volatile__("rsr %0, EPC1" : "=a" (epc1) : : "memory");

	if (name) {
		/* fill exception information */
		info.code = SOF_IPC_PANIC_EXCEPTION;
		info.hdr.size = sizeof(info);
		info.linenum = code;
		memcpy_s(info.filename, sizeof(info.filename) - 1,
			 name, rstrlen(name));
		pinfo = &info;
	} else {
		pinfo = NULL;
	}

	/* now panic and rewind 8 stack frames. */
	/* TODO: we could invoke a GDB stub here */
	panic_rewind(SOF_IPC_PANIC_EXCEPTION, 8 * sizeof(uint32_t),
		     pinfo, &epc1);
}

/**
 * \brief Called in the case of exception.
 */
static inline void exception(void)
{
	exception_ext(NULL, 0);
}

/* macro to build detailed exception callbacks */
#if DETAILED_EXCEPTIONS
#define BUILD_EXCEPTION(name) \
	static inline void exception_##name(void) \
	{ \
		exception_ext(#name, 0); \
	}
#define FUN_EXCEPTION(name) exception_##name
#else
#define BUILD_EXCEPTION(name)
#define FUN_EXCEPTION(name) exception
#endif

/* Detailed exception callbacks */

BUILD_EXCEPTION(EXCCAUSE_ILLEGAL)
BUILD_EXCEPTION(EXCCAUSE_SYSCALL)
BUILD_EXCEPTION(EXCCAUSE_INSTR_ERROR)
BUILD_EXCEPTION(EXCCAUSE_LOAD_STORE_ERROR)
BUILD_EXCEPTION(EXCCAUSE_ALLOCA)
BUILD_EXCEPTION(EXCCAUSE_DIVIDE_BY_ZERO)
BUILD_EXCEPTION(EXCCAUSE_PRIVILEGED)
BUILD_EXCEPTION(EXCCAUSE_UNALIGNED)
BUILD_EXCEPTION(EXCCAUSE_INSTR_DATA_ERROR)
BUILD_EXCEPTION(EXCCAUSE_LOAD_STORE_DATA_ERROR)
BUILD_EXCEPTION(EXCCAUSE_INSTR_ADDR_ERROR)
BUILD_EXCEPTION(EXCCAUSE_LOAD_STORE_ADDR_ERROR)
BUILD_EXCEPTION(EXCCAUSE_INSTR_RING)
BUILD_EXCEPTION(EXCCAUSE_INSTR_PROHIBITED)
BUILD_EXCEPTION(EXCCAUSE_LOAD_STORE_RING)
BUILD_EXCEPTION(EXCCAUSE_LOAD_PROHIBITED)
BUILD_EXCEPTION(EXCCAUSE_STORE_PROHIBITED)

/**
 * \brief Registers exception handlers.
 */
static inline void register_exceptions(void)
{

	/* 0 - 9 */
	_xtos_set_exception_handler(
		EXCCAUSE_ILLEGAL, &FUN_EXCEPTION(EXCCAUSE_ILLEGAL));
	_xtos_set_exception_handler(
		EXCCAUSE_SYSCALL, &FUN_EXCEPTION(EXCCAUSE_SYSCALL));
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_ERROR, &FUN_EXCEPTION(EXCCAUSE_INSTR_ERROR));
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_ERROR, &FUN_EXCEPTION(EXCCAUSE_LOAD_STORE_ERROR));
	_xtos_set_exception_handler(
		EXCCAUSE_ALLOCA, &FUN_EXCEPTION(EXCCAUSE_ALLOCA));
	_xtos_set_exception_handler(
		EXCCAUSE_DIVIDE_BY_ZERO, &FUN_EXCEPTION(EXCCAUSE_DIVIDE_BY_ZERO));
	_xtos_set_exception_handler(
		EXCCAUSE_SPECULATION, &exception);
	_xtos_set_exception_handler(
		EXCCAUSE_PRIVILEGED, &FUN_EXCEPTION(EXCCAUSE_PRIVILEGED));
	_xtos_set_exception_handler(
		EXCCAUSE_UNALIGNED, &FUN_EXCEPTION(EXCCAUSE_UNALIGNED));

	/* Reserved				10..11 */

	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_DATA_ERROR, &FUN_EXCEPTION(EXCCAUSE_INSTR_DATA_ERROR));
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_DATA_ERROR, &FUN_EXCEPTION(EXCCAUSE_LOAD_STORE_DATA_ERROR));
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_ADDR_ERROR, &FUN_EXCEPTION(EXCCAUSE_INSTR_ADDR_ERROR));
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_ADDR_ERROR, &FUN_EXCEPTION(EXCCAUSE_LOAD_STORE_ADDR_ERROR));
	_xtos_set_exception_handler(
		EXCCAUSE_ITLB_MISS, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_ITLB_MULTIHIT, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_RING, &FUN_EXCEPTION(EXCCAUSE_INSTR_RING));

	/* Reserved				19 */

	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_PROHIBITED, &FUN_EXCEPTION(EXCCAUSE_INSTR_PROHIBITED));

	/* Reserved				21..23 */
	_xtos_set_exception_handler(
		EXCCAUSE_DTLB_MISS, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_DTLB_MULTIHIT, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_RING, &FUN_EXCEPTION(EXCCAUSE_LOAD_STORE_RING));

	/* Reserved				27 */
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_PROHIBITED, &FUN_EXCEPTION(EXCCAUSE_LOAD_PROHIBITED));
	_xtos_set_exception_handler(
		EXCCAUSE_STORE_PROHIBITED, &FUN_EXCEPTION(EXCCAUSE_STORE_PROHIBITED));

	/* Reserved				30..31 */
	_xtos_set_exception_handler(
		EXCCAUSE_CP0_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP1_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP2_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP3_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP4_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP5_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP6_DISABLED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_CP7_DISABLED, (void *)&exception);

	/* Reserved				40..63 */
}

/**
 * \brief Called from assembler context with no return or parameters.
 */
static inline void __memmap_init(void) { }

#if CONFIG_SMP

int slave_core_init(void);

#else

static inline int slave_core_init(void) { return 0; }

#endif /* CONFIG_SMP */

#endif /* __ARCH_INIT_H__ */

#else

#error "This file shouldn't be included from outside of sof/init.h"

#endif /* __SOF_INIT_H__ */
