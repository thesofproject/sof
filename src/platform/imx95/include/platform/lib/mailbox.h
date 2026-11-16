/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2024 NXP
 */

#ifdef __SOF_LIB_MAILBOX_H__

#ifndef __PLATFORM_LIB_MAILBOX_H__
#define __PLATFORM_LIB_MAILBOX_H__

/* i.MX95's mailbox is organized as follows:
 *
 * +---------------+-------------------------+
 * | Region name   | Base address  |  Size   |
 * +---------------+---------------+---------+
 * | Inbox region  |   0x8fffd000  | 0x1000  |
 * +---------------+---------------+---------+
 * | Outbox region |   0x8fffe000  | 0x1000  |
 * +---------------+---------------+---------+
 * | Stream region |   0x8ffff000  | 0x1000  |
 * +---------------+---------------+---------+
 *
 * IMPORTANT: all regions should be 32-byte aligned.
 * This is required because cache maintenance might
 * be performed on them.
 */

/* inbox */
#define MAILBOX_HOSTBOX_SIZE 0x1000
#define MAILBOX_HOSTBOX_BASE 0x86000000
#define MAILBOX_HOSTBOX_OFFSET 0

/* outbox */
#define MAILBOX_DSPBOX_SIZE 0x1000
#define MAILBOX_DSPBOX_BASE 0x86001000
#define MAILBOX_DSPBOX_OFFSET (MAILBOX_HOSTBOX_OFFSET + MAILBOX_HOSTBOX_SIZE)

/* stream */
#define MAILBOX_STREAM_SIZE 0x1000
#define MAILBOX_STREAM_BASE 0x86002000
#define MAILBOX_STREAM_OFFSET (MAILBOX_DSPBOX_OFFSET + MAILBOX_DSPBOX_SIZE)

#endif /* __PLATFORM_LIB_MAILBOX_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/mailbox.h"

#endif /* __SOF_LIB_MAILBOX_H__ */
