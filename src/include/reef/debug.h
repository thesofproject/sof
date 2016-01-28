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
#include <platform/platform.h>
#include <stdint.h>
#include <stdlib.h>

/* panic reasons */
#define PANIC_MEM	0
#define PANIC_WORK	1
#define PANIC_IPC	2
#define PANIC_ARCH	3
#define PANIC_PLATFORM	4
#define PANIC_TASK	5
#define PANIC_EXCEPTION	6

#define DEBUG

#ifdef DEBUG

#define dbg() \
	do { \
		volatile uint32_t *__m = (uint32_t*)mailbox_get_debug_base(); \
		*(__m++) = (__FILE__[0] << 24) + (__FILE__[1] << 16) +\
			 (__FILE__[2] << 8) + (__FILE__[3]); \
		*(__m++) = (__func__[0] << 24) + (__func__[1] << 16) + \
			(__func__[2] << 8) + (__func__[3]); \
		*__m = __LINE__; \
	} while (0);

#define dbg_at(__x) \
	do { \
		volatile uint32_t *__m = (uint32_t*)mailbox_get_debug_base() + __x; \
		*(__m++) = (__FILE__[0] << 24) + (__FILE__[1] << 16) +\
			 (__FILE__[2] << 8) + (__FILE__[3]); \
		*(__m++) = (__func__[0] << 24) + (__func__[1] << 16) + \
			(__func__[2] << 8) + (__func__[3]); \
		*__m = __LINE__; \
	} while (0);

#define dbg_val(__v) \
	do { \
		volatile uint32_t *__m = \
			(volatile uint32_t*)mailbox_get_debug_base(); \
		*__m = __v; \
	} while (0);

#define dbg_val_at(__v, __x) \
	do { \
		volatile uint32_t *__m = \
			(volatile uint32_t*)mailbox_get_debug_base() + __x; \
		*__m = __v; \
	} while (0);

#define dump(addr, count) \
	do { \
		volatile uint32_t *__m = (uint32_t*)mailbox_get_debug_base(); \
		volatile uint32_t *__a = (uint32_t*)addr; \
		volatile int __c = count; \
		while (__c--) \
			*(__m++) = *(__a++); \
	} while (0);

#define dump_object(__o) \
	dbg(); \
	dump(&__o, sizeof(__o) >> 2);

#define dump_object_ptr(__o) \
	dbg(); \
	dump(__o, sizeof(*__o) >> 2);

#else

#define dbg()
#define dbg_at(__x)
#define dbg_val(__v)
#define dbg_val_at(__v, __x)
#define dump(addr, count)
#define dump_object(__o)
#define dump_object_ptr(__o)
#endif

#define panic(_p) \
	do { \
		dbg_val(0xdead0000 | _p) \
		platform_panic(0xdead0000 | _p); \
		while(1) {}; \
	} while (0);

#define panic_dump_stack(_p) \
	do { \
		extern uint32_t __stack; \
		extern uint32_t _stack_sentry; \
		uint32_t _stack_bottom = (uint32_t)&__stack; \
		uint32_t _stack_limit = (uint32_t)&_stack_sentry; \
		uint32_t _stack_top; \
		\
		__asm__ __volatile__ ("mov %0, a1" : "=a" (_stack_top) : : "memory"); \
		dbg_val(0xdead0000 | _p) \
		platform_panic(0xdead0000 | _p); \
		dbg_val(_stack_top) \
		dbg_val(_stack_bottom) \
		\
		if (_stack_bottom <= _stack_limit) { \
			dbg_val(0x51ac0000 | _p) \
			_stack_bottom = _stack_limit; \
		} \
		dump(_stack_top, _stack_bottom - _stack_top) \
		\
		while(1) {}; \
	} while (0);

#endif
