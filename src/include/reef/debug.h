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
#include <stdlib.h>

/* panic reasons */
#define PANIC_MEM	0
#define PANIC_WORK	1

#define dbg() \
	do { \
		volatile uint32_t *m = (uint32_t*)mailbox_get_debug_base(); \
		*(m++) = (__FILE__[0] << 24) + (__FILE__[1] << 16) +\
			 (__FILE__[2] << 8) + (__FILE__[3]); \
		*(m++) = (__func__[0] << 24) + (__func__[1] << 16) + \
			(__func__[2] << 8) + (__func__[3]); \
		*m = __LINE__; \
	} while (0);

#define dbg_at(__x) \
	do { \
		volatile uint32_t *m = (uint32_t*)mailbox_get_debug_base() + __x; \
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

#define dbg_val_at(_v, __x) \
	do { \
		volatile uint32_t *_m = \
			(volatile uint32_t*)mailbox_get_debug_base() + __x; \
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

#define dump_object(__o) \
	dbg(); \
	dump(&__o, sizeof(__o));

#define dump_object_ptr(__o) \
	dbg(); \
	dump(__o, sizeof(*__o));

#define panic(_p) \
	do { \
		extern uint32_t __stack; \
		uint32_t _stack_bottom = (uint32_t)&__stack; \
		uint32_t _stack_top; \
		__asm__ __volatile__ ("mov %0, a1" : "=a" (_stack_top) : : "memory"); \
		dbg() \
		dbg_val(0xdead0000 | _p) \
		dump(_stack_top, _stack_bottom - _stack_top) \
		while(1) {}; \
	} while (0);

#endif
