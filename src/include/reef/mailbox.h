/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_MAILBOX__
#define __INCLUDE_MAILBOX__

#include <platform/mailbox.h>
#include <stdint.h>

/* 1k should be enough for everyone ..... */
#define IPC_MAX_MAILBOX_BYTES 1024

#define mailbox_get_exception_base() \
	MAILBOX_EXCEPTION_BASE

#define mailbox_get_exception_size() \
	MAILBOX_EXCEPTION_SIZE

#define mailbox_get_outbox_base() \
	MAILBOX_OUTBOX_BASE

#define mailbox_get_outbox_size() \
	MAILBOX_OUTBOX_SIZE

#define mailbox_get_inbox_base() \
	MAILBOX_INBOX_BASE

#define mailbox_get_inbox_size() \
	MAILBOX_INBOX_SIZE

#define mailbox_get_debug_base() \
	MAILBOX_DEBUG_BASE

#define mailbox_get_debug_size() \
	MAILBOX_DEBUG_SIZE

#define mailbox_outbox_write(dest, src, bytes) \
	rmemcpy((void*)(MAILBOX_OUTBOX_BASE + dest), src, bytes);

#define mailbox_outbox_read(dest, src, bytes) \
	rmemcpy(dest, (void*)(MAILBOX_OUTBOX_BASE + src), bytes);

#define mailbox_inbox_write(dest, src, bytes) \
	rmemcpy((void*)(MAILBOX_INBOX_BASE + dest), src, bytes);

#define mailbox_inbox_read(dest, src, bytes) \
	rmemcpy(dest, (void*)(MAILBOX_INBOX_BASE + src), bytes);

#endif
