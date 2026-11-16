/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
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

uint8_t *get_library_mailbox(void);

#define MAILBOX_BASE	get_library_mailbox()

#endif /* __PLATFORM_LIB_MAILBOX_LAYOUT_H__ */
