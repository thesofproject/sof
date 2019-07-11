/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

/*
 * Header file for init.S
 */

#ifdef __SOF_GDB_GDB_H__

#ifndef __ARCH_GDB_INIT_H__
#define __ARCH_GDB_INIT_H__

extern void gdb_init_debug_exception(void);

#endif /* __ARCH_GDB_INIT_H__ */

#else

#error "This file shouldn't be included from outside of sof/gdb/gdb.h"

#endif /* __SOF_GDB_GDB_H__ */
