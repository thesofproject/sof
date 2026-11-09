/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 */

#ifndef __PLATFORM_LIB_MAILBOX_H__
#define __PLATFORM_LIB_MAILBOX_H__

/* Dummy mailbox header for qemu_xtensa */
#define MAILBOX_HOSTBOX_BASE 0x10000000
#define MAILBOX_HOSTBOX_SIZE 0x1000
#define MAILBOX_DSPBOX_BASE  0x10005000
#define MAILBOX_DSPBOX_SIZE  0x1000
#define MAILBOX_STREAM_BASE  0x10001000
#define MAILBOX_STREAM_SIZE  0x1000
#define MAILBOX_TRACE_BASE   0x10002000
#define MAILBOX_TRACE_SIZE   0x1000
#define MAILBOX_EXCEPTION_BASE 0x10003000
#define MAILBOX_EXCEPTION_SIZE 0x1000
#define MAILBOX_DEBUG_BASE   0x10004000
#define MAILBOX_DEBUG_SIZE   0x1000
#define MAILBOX_SW_REG_BASE  0x10005000
#define MAILBOX_SW_REG_SIZE  0x1000

#include <stddef.h>
#include <stdint.h>

static inline void mailbox_sw_regs_write(size_t offset, const void *src, size_t bytes) {}
static inline void mailbox_sw_reg_write(size_t offset, uint32_t val) {}
static inline void mailbox_sw_reg_write64(size_t offset, uint64_t val) {}
static inline uint32_t mailbox_sw_reg_read(size_t offset) { return 0; }
static inline uint64_t mailbox_sw_reg_read64(size_t offset) { return 0; }

#endif /* __PLATFORM_LIB_MAILBOX_H__ */
