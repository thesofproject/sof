/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
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

/* HP SRAM */
#define HP_SRAM_BASE		0xBE000000

/* HP SRAM windows */
/* window 0 */
#define SRAM_SW_REG_BASE	(HP_SRAM_BASE + 0x4000)
#define SRAM_SW_REG_SIZE	0x1000

#define SRAM_OUTBOX_BASE	(SRAM_SW_REG_BASE + SRAM_SW_REG_SIZE)
#define SRAM_OUTBOX_SIZE	0x1000

/* window 1 */
#define SRAM_INBOX_BASE		(SRAM_OUTBOX_BASE + SRAM_OUTBOX_SIZE)
#define SRAM_INBOX_SIZE		0x2000
/* window 2 */
#define SRAM_DEBUG_BASE		(SRAM_INBOX_BASE + SRAM_INBOX_SIZE)
#define SRAM_DEBUG_SIZE		0x800

#define SRAM_EXCEPT_BASE	(SRAM_DEBUG_BASE + SRAM_DEBUG_SIZE)
#define SRAM_EXCEPT_SIZE	0x800

#define SRAM_STREAM_BASE	(SRAM_EXCEPT_BASE + SRAM_EXCEPT_SIZE)
#define SRAM_STREAM_SIZE	0x1000

/* window 3 */
#define SRAM_TRACE_BASE		(SRAM_STREAM_BASE + SRAM_STREAM_SIZE)
#define SRAM_TRACE_SIZE		0x0

#endif /* __PLATFORM_LIB_MAILBOX_LAYOUT_H__ */
