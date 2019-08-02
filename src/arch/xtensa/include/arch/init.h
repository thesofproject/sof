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
#include <ipc/trace.h>
#include <config.h>
#include <xtensa/corebits.h>
#include <xtensa/xtruntime.h>
#include <stddef.h>
#include <stdint.h>

struct sof;

/**
 * \brief Called in the case of exception.
 */
static inline void exception(void)
{
	uintptr_t epc1;

	__asm__ __volatile__("rsr %0, EPC1" : "=a" (epc1) : : "memory");

	/* now panic and rewind 8 stack frames. */
	/* TODO: we could invoke a GDB stub here */
	panic_rewind(SOF_IPC_PANIC_EXCEPTION, 8 * sizeof(uint32_t),
		     NULL, &epc1);
}

/**
 * \brief Registers exception handlers.
 */
static inline void register_exceptions(void)
{

	/* 0 - 9 */
	_xtos_set_exception_handler(
		EXCCAUSE_ILLEGAL, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_SYSCALL, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_ALLOCA, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_DIVIDE_BY_ZERO, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_SPECULATION, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_PRIVILEGED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_UNALIGNED, (void *)&exception);

	/* Reserved				10..11 */

	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_DATA_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_DATA_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_ADDR_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_ADDR_ERROR, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_ITLB_MISS, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_ITLB_MULTIHIT, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_RING, (void *)&exception);

	/* Reserved				19 */

	_xtos_set_exception_handler(
		EXCCAUSE_INSTR_PROHIBITED, (void *)&exception);

	/* Reserved				21..23 */
	_xtos_set_exception_handler(
		EXCCAUSE_DTLB_MISS, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_DTLB_MULTIHIT, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_STORE_RING, (void *)&exception);

	/* Reserved				27 */
	_xtos_set_exception_handler(
		EXCCAUSE_LOAD_PROHIBITED, (void *)&exception);
	_xtos_set_exception_handler(
		EXCCAUSE_STORE_PROHIBITED, (void *)&exception);

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

int slave_core_init(struct sof *sof);

#else

static inline int slave_core_init(struct sof *sof) { return 0; }

#endif /* CONFIG_SMP */

#endif /* __ARCH_INIT_H__ */

#else

#error "This file shouldn't be included from outside of sof/init.h"

#endif /* __SOF_INIT_H__ */
