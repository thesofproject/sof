/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

/**
 * \file include/sof/debug/gdb/gdb.h
 * \brief Entry point of the Zephyr GDB stub (CONFIG_GDBSTUB).
 * \ingroup debug_stream_api
 */

#ifndef __SOF_DEBUG_GDB_GDB_H__
#define __SOF_DEBUG_GDB_GDB_H__

/**
 * \brief Initialize the GDB target debugging stub, see ipc_enter_gdb.
 * \ingroup debug_stream_api
 */
void gdb_init(void);

#endif /* __SOF_DEBUG_GDB_GDB_H__ */
