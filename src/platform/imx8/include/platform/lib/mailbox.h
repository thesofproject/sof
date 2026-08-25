/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2019 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifdef __SOF_LIB_MAILBOX_H__

#ifndef __PLATFORM_LIB_MAILBOX_H__
#define __PLATFORM_LIB_MAILBOX_H__

#include <rtos/cache.h>
#include <sof/lib/memory.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The Window Region on i.MX8 SRAM is organised like this :-
 * +--------------------------------------------------------------------------+
 * | Offset              | Region         |  Size                             |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_TRACE_BASE     | Trace Buffer   |  SRAM_TRACE_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_DEBUG_BASE     | Debug data     |  SRAM_DEBUG_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_INBOX_BASE     | Inbox          |  SRAM_INBOX_SIZE                  |
 * +---------------------+----------------+-----------------------------------+
 * | SRAM_OUTBOX_BASE    | Outbox         |  SRAM_MAILBOX_SIZE                |
 * +---------------------+----------------+-----------------------------------+
 */

#define MAILBOX_DSPBOX_SIZE		SRAM_OUTBOX_SIZE
#define MAILBOX_DSPBOX_BASE		SRAM_OUTBOX_BASE
#define MAILBOX_DSPBOX_OFFSET		SRAM_OUTBOX_OFFSET

#define MAILBOX_HOSTBOX_SIZE		SRAM_INBOX_SIZE
#define MAILBOX_HOSTBOX_BASE		SRAM_INBOX_BASE
#define MAILBOX_HOSTBOX_OFFSET		SRAM_INBOX_OFFSET

/* IPC4 compact-header slots preceding each payload box */
#define MAILBOX_DSPBOX_HDR_BASE		SRAM_OUTBOX_HDR_BASE
#define MAILBOX_HOSTBOX_HDR_BASE	SRAM_INBOX_HDR_BASE

#define MAILBOX_DEBUG_SIZE		SRAM_DEBUG_SIZE
#define MAILBOX_DEBUG_BASE		SRAM_DEBUG_BASE
#define MAILBOX_DEBUG_OFFSET		SRAM_DEBUG_OFFSET

#define MAILBOX_TRACE_SIZE		SRAM_TRACE_SIZE
#define MAILBOX_TRACE_BASE		SRAM_TRACE_BASE
#define MAILBOX_TRACE_OFFSET		SRAM_TRACE_OFFSET

#define MAILBOX_EXCEPTION_SIZE		SRAM_EXCEPT_SIZE
#define MAILBOX_EXCEPTION_BASE		SRAM_EXCEPT_BASE
#define MAILBOX_EXCEPTION_OFFSET	SRAM_EXCEPT_OFFSET

#define MAILBOX_STREAM_SIZE		SRAM_STREAM_SIZE
#define MAILBOX_STREAM_BASE		SRAM_STREAM_BASE
#define MAILBOX_STREAM_OFFSET		SRAM_STREAM_OFFSET

static inline void mailbox_sw_reg_write(size_t offset, uint32_t src)
{
	volatile uint32_t *ptr;

	ptr = (volatile uint32_t *)(MAILBOX_DEBUG_BASE + offset);
	*ptr = src;
}

/* IPC4 SW registers (copier LLP); carved out of mailbox SRAM */
#define MAILBOX_SW_REG_BASE	SRAM_SW_REG_BASE
#define MAILBOX_SW_REG_SIZE	SRAM_SW_REG_SIZE

static inline void mailbox_sw_reg_write64(size_t offset, uint64_t src)
{
	volatile uint64_t *ptr;

	ptr = (volatile uint64_t *)(MAILBOX_SW_REG_BASE + offset);
	*ptr = src;
	dcache_writeback_region((__sparse_force void __sparse_cache *)(MAILBOX_SW_REG_BASE +
								       offset),
				sizeof(src));
}

static inline uint32_t mailbox_sw_reg_read(size_t offset)
{
	volatile uint32_t *ptr;

	ptr = (volatile uint32_t *)(MAILBOX_SW_REG_BASE + offset);
	dcache_invalidate_region((__sparse_force void __sparse_cache *)(MAILBOX_SW_REG_BASE +
								       offset),
				 sizeof(uint32_t));

	return *ptr;
}

static inline uint64_t mailbox_sw_reg_read64(size_t offset)
{
	volatile uint64_t *ptr;

	ptr = (volatile uint64_t *)(MAILBOX_SW_REG_BASE + offset);
	dcache_invalidate_region((__sparse_force void __sparse_cache *)(MAILBOX_SW_REG_BASE +
								       offset),
				 sizeof(uint64_t));

	return *ptr;
}

static inline void mailbox_sw_regs_write(size_t offset, const void *src, size_t bytes)
{
	volatile uint8_t *dst = (volatile uint8_t *)(MAILBOX_SW_REG_BASE + offset);
	const uint8_t *s = src;
	size_t i;

	for (i = 0; i < bytes; i++)
		dst[i] = s[i];

	dcache_writeback_region((__sparse_force void __sparse_cache *)(MAILBOX_SW_REG_BASE +
								       offset),
				bytes);
}

#endif /* __PLATFORM_LIB_MAILBOX_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/mailbox.h"

#endif /* __SOF_LIB_MAILBOX_H__ */
