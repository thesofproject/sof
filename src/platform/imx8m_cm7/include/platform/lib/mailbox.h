/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 NXP
 */

#ifdef __SOF_LIB_MAILBOX_H__

#ifndef __PLATFORM_LIB_MAILBOX_H__
#define __PLATFORM_LIB_MAILBOX_H__

/* The i.MX8MP CM7 mailbox region is organized like this:
 *
 * +---------------+-------------------------+
 * | Region name   | Base address  |  Size   |
 * +---------------+---------------+---------+
 * | Outbox region |   0x82000000  | 0x1000  |
 * +---------------+---------------+---------+
 * | Inbox region  |   0x82001000  | 0x1000  |
 * +---------------+---------------+---------+
 * | Stream region |   0x82002000  | 0x1000  |
 * +---------------+---------------+---------+
 *
 * IMPORTANT: all regions should be 32-byte aligned.
 * This is required because cache maintenance might
 * be performed on them.
 */

/* outbox */
#define MAILBOX_DSPBOX_SIZE 0x1000
#define MAILBOX_DSPBOX_BASE 0x82000000
#define MAILBOX_DSPBOX_OFFSET 0

/* inbox */
#define MAILBOX_HOSTBOX_SIZE 0x1000
#define MAILBOX_HOSTBOX_BASE 0x82001000
#define MAILBOX_HOSTBOX_OFFSET (MAILBOX_DSPBOX_OFFSET + MAILBOX_DSPBOX_SIZE)

/* stream */
#define MAILBOX_STREAM_SIZE 0x1000
#define MAILBOX_STREAM_BASE 0x82002000
#define MAILBOX_STREAM_OFFSET (MAILBOX_HOSTBOX_OFFSET + MAILBOX_HOSTBOX_SIZE)

#endif /* __PLATFORM_LIB_MAILBOX_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/mailbox.h"

#endif /* __SOF_LIB_MAILBOX_H__ */
