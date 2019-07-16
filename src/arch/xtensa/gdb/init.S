/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

/*
 * Init debug exeption and enable global breakpoints.
 *
 */

#include <arch/debug/gdb/xtensa-defs.h>

.text
.global gdb_init_debug_exception
.align	4

gdb_init_debug_exception:
	entry	a1, 16

	movi	a3, DebugExceptionEntry
	wsr	a3, DEBUG_EXCSAVE

	/* enable breakpoints */
	movi a3, 1
	wsr  a3, DEBUG_IBREAKENABLE

	isync
	rsync
	retw

.size	gdb_init_debug_exception, . - gdb_init_debug_exception
