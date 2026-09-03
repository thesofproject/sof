/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Curtis <cujomalainey@chromium.org>
 */

/*
 * Header file for host-GDB utilities.
 */

#ifndef __ARCH_DEBUG_GDB_UTILITIES_H__
#define __ARCH_DEBUG_GDB_UTILITIES_H__

void arch_gdb_read_sr(int sr);
void arch_gdb_write_sr(int sr, int *sregs);
unsigned char arch_gdb_load_from_memory(void *mem);
void arch_gdb_memory_load_and_store(void *mem, unsigned char ch);
void arch_gdb_single_step(int *sregs);

#endif /* __ARCH_DEBUG_GDB_UTILITIES_H__ */
