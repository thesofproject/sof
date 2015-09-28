/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_DEBUG__
#define __INCLUDE_DEBUG__

#include <reef/mailbox.h>
#include <stdint.h>

#define dbg() \
	do { \
		volatile uint32_t *m = (uint32_t*)mailbox_get_debug_base(); \
		*(m++) = (__FILE__[0] << 24) + (__FILE__[1] << 16) +\
			 (__FILE__[2] << 8) + (__FILE__[3]); \
		*(m++) = (__func__[0] << 24) + (__func__[1] << 16) + \
			(__func__[2] << 8) + (__func__[3]); \
		*m = __LINE__; \
	} while (0);

#define dbg_val(_v) \
	do { \
		volatile uint32_t *_m = \
			(volatile uint32_t*)mailbox_get_debug_base(); \
		*_m = _v; \
	} while (0);


#define dump(addr, count) \
	do { \
		volatile uint32_t *m = (uint32_t*)mailbox_get_debug_base(); \
		volatile uint32_t *a = (uint32_t*)addr; \
		volatile int c = count; \
		while (c--) \
			*(m++) = *(a++); \
	} while (0);

#define panic(_p) \
	dbg() \
	dbg_val(_p) \
	*((uint32_t*)0) = 0;

#endif
