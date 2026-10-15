/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Espressif Systems / SOF Project
 */

#ifndef __PLATFORM_LIB_MAILBOX_H__
#define __PLATFORM_LIB_MAILBOX_H__

#include <stdint.h>
#include <stddef.h>

extern uint8_t sof_esp32_mailbox[0x4000];
#define MAILBOX_BASE ((uintptr_t)sof_esp32_mailbox)

#define MAILBOX_DSPBOX_OFFSET   0x0
#define MAILBOX_DSPBOX_SIZE     0x1000
#define MAILBOX_DSPBOX_BASE     (MAILBOX_BASE + MAILBOX_DSPBOX_OFFSET)

#define MAILBOX_HOSTBOX_OFFSET  0x1000
#define MAILBOX_HOSTBOX_SIZE    0x1000
#define MAILBOX_HOSTBOX_BASE    (MAILBOX_BASE + MAILBOX_HOSTBOX_OFFSET)

#define MAILBOX_STREAM_OFFSET   0x2000
#define MAILBOX_STREAM_SIZE     0x1000
#define MAILBOX_STREAM_BASE     (MAILBOX_BASE + MAILBOX_STREAM_OFFSET)

#define MAILBOX_DEBUG_OFFSET    0x3000
#define MAILBOX_DEBUG_SIZE      0x800
#define MAILBOX_DEBUG_BASE      (MAILBOX_BASE + MAILBOX_DEBUG_OFFSET)

#define MAILBOX_EXCEPTION_OFFSET 0x3800
#define MAILBOX_EXCEPTION_SIZE  0x400
#define MAILBOX_EXCEPTION_BASE  (MAILBOX_BASE + MAILBOX_EXCEPTION_OFFSET)

#define MAILBOX_TRACE_OFFSET    0x3C00
#define MAILBOX_TRACE_SIZE      0x400
#define MAILBOX_TRACE_BASE      (MAILBOX_BASE + MAILBOX_TRACE_OFFSET)

static inline void mailbox_sw_reg_write(size_t offset, uint32_t src) {}
static inline void mailbox_sw_regs_write(size_t offset, const void *src, size_t bytes) {}
static inline uint32_t mailbox_sw_reg_read(size_t offset) { return 0; }
static inline uint64_t mailbox_sw_reg_read64(size_t offset) { return 0; }

#endif /* __PLATFORM_LIB_MAILBOX_H__ */

