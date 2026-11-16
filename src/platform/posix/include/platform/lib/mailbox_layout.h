/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Google LLC.  All rights reserved.
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

#include <stdint.h>
#include <ipc/header.h>

/*
 * Mailbox base macros must yield byte-pointer arithmetic so that the
 * generic mailbox API in `sof/src/include/sof/lib/mailbox.h`
 * (`MAILBOX_HOSTBOX_BASE + offset`, etc., where `offset` is a byte
 * offset) addresses the intended byte.  The backing storage is
 * declared as `uint32_t[]` only for natural-alignment; addressing is
 * done through a `uint8_t *` cast so byte offsets do not get scaled
 * by `sizeof(uint32_t)`.  Every other SOF platform defines these
 * bases as plain byte addresses (integer literals or
 * `SRAM_INBOX_BASE`), so the cast keeps POSIX consistent with that
 * ABI.
 */
extern uint32_t posix_hostbox[];
#define MAILBOX_HOSTBOX_SIZE SOF_IPC_MSG_MAX_SIZE
#define MAILBOX_HOSTBOX_BASE ((uint8_t *)&posix_hostbox[0])

extern uint32_t posix_dspbox[];
#define MAILBOX_DSPBOX_SIZE 4096
#define MAILBOX_DSPBOX_BASE ((uint8_t *)&posix_dspbox[0])

extern uint32_t posix_stream[];
#define MAILBOX_STREAM_SIZE 4096
#define MAILBOX_STREAM_BASE ((uint8_t *)&posix_stream[0])

extern uint32_t posix_trace[];
#define MAILBOX_TRACE_BASE ((uint8_t *)&posix_trace[0])
#define MAILBOX_TRACE_SIZE 4096

#endif /* __PLATFORM_LIB_MAILBOX_LAYOUT_H__ */
