/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2023 NXP
 */

#ifdef __SOF_LIB_MAILBOX_H__

#ifndef __PLATFORM_LIB_MAILBOX_H__
#define __PLATFORM_LIB_MAILBOX_H__

/* i.MX93's mailbox is organized as follows:
 *
 * +---------------+----------------------+------------------------+-----------+
 * | Region name   | Base address  | Offset |  Size   |      Permissions       |
 * +---------------+---------------+--------+---------+------------------------+
 * | Inbox region  |   0xce100000  |   0    | 0x1000  | ROOT: R/W, INMATE: R/W |
 * +---------------+---------------+--------+---------+------------------------+
 * | Outbox region |   0xce101000  | 0x1000 | 0x1000  | ROOT: R/W, INMATE: R/W |
 * +---------------+---------------+--------+---------+------------+-----------+
 * | Stream region |   0xce102000  | 0x2000 | 0x1000  | ROOT: R/W, INMATE: R/W |
 * +---------------+---------------+--------+---------+------------------------+
 *
 *
 * RESTRICTIONS:
 *	* the inbox region always needs to be placed at offset 0
 *	relative to the mailbox memory region used by the Linux host
 *	driver. Other than that, there's no restrictions regarding the
 *	placement of these mailbox regions.
 *
 * Notes:
 *	* if there's a need to add new regions, simply append the
 *	new regions to the above table and create memory regions
 *	inside the Jailhouse configurations for root/inmate cells.
 *	Also, please make sure that the Linux host driver is also
 *	aware of these new regions.
 *
 *	* although these regions are placed one after the other please
 *	note that this isn't a restriction. If there's a need to place
 *	these regions elsewhere please not that the offset is relative
 *	to the mailbox memory region used by the Linux host driver.
 *
 *	* TODO: root and inmate shouldn't have the same permissions, fix
 *	it.
 */

/* inbox */
#define MAILBOX_HOSTBOX_SIZE 0x1000
#define MAILBOX_HOSTBOX_BASE 0xce100000
#define MAILBOX_HOSTBOX_OFFSET 0

/* outbox */
#define MAILBOX_DSPBOX_SIZE 0x1000
#define MAILBOX_DSPBOX_BASE 0xce101000
#define MAILBOX_DSPBOX_OFFSET (MAILBOX_HOSTBOX_OFFSET + MAILBOX_HOSTBOX_SIZE)

/* stream */
#define MAILBOX_STREAM_SIZE 0x1000
#define MAILBOX_STREAM_BASE 0xce102000
#define MAILBOX_STREAM_OFFSET (MAILBOX_DSPBOX_OFFSET + MAILBOX_DSPBOX_SIZE)

#endif /* __PLATFORM_LIB_MAILBOX_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/mailbox.h"

#endif /* __SOF_LIB_MAILBOX_H__ */
