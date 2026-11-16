/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 */

/**
 * \file platform/lib/mailbox_layout.h
 * \brief Layout of the host memory windows used by the mailbox
 *
 * User-space code can include this, through sof/lib/mailbox.h. The rest
 * of the platform memory map is in platform/lib/memory.h.
 */

#ifndef __PLATFORM_LIB_MAILBOX_LAYOUT_H__
#define __PLATFORM_LIB_MAILBOX_LAYOUT_H__

/* the mailbox is at the start of SDRAM1 */
#define SDRAM1_BASE	0x92C00000

/* Mailbox configuration */
#define SRAM_OUTBOX_BASE	SDRAM1_BASE
#define SRAM_OUTBOX_SIZE	0x1000
#define SRAM_OUTBOX_OFFSET	0

#define SRAM_INBOX_BASE		(SRAM_OUTBOX_BASE + SRAM_OUTBOX_SIZE)
#define SRAM_INBOX_SIZE		0x1000
#define SRAM_INBOX_OFFSET	SRAM_OUTBOX_SIZE

#define SRAM_DEBUG_BASE		(SRAM_INBOX_BASE + SRAM_INBOX_SIZE)
#define SRAM_DEBUG_SIZE		0x800
#define SRAM_DEBUG_OFFSET	(SRAM_INBOX_OFFSET + SRAM_INBOX_SIZE)

#define SRAM_EXCEPT_BASE	(SRAM_DEBUG_BASE + SRAM_DEBUG_SIZE)
#define SRAM_EXCEPT_SIZE	0x800
#define SRAM_EXCEPT_OFFSET	(SRAM_DEBUG_OFFSET + SRAM_DEBUG_SIZE)

#define SRAM_STREAM_BASE	(SRAM_EXCEPT_BASE + SRAM_EXCEPT_SIZE)
#define SRAM_STREAM_SIZE	0x1000
#define SRAM_STREAM_OFFSET	(SRAM_EXCEPT_OFFSET + SRAM_EXCEPT_SIZE)

#define SRAM_TRACE_BASE		(SRAM_STREAM_BASE + SRAM_STREAM_SIZE)
#define SRAM_TRACE_SIZE		0x1000
#define SRAM_TRACE_OFFSET (SRAM_STREAM_OFFSET + SRAM_STREAM_SIZE)

#endif /* __PLATFORM_LIB_MAILBOX_LAYOUT_H__ */
