/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_MAILBOX__
#define __INCLUDE_MAILBOX__

#include <stdint.h>
#include <platform/mailbox.h>

#define mailbox_get_exception_base() \
	(uint32_t*)MAILBOX_BASE;

#endif
