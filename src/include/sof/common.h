/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_COMMON_H__
#define __SOF_COMMON_H__

#include <sof/preproc.h>
#include <config.h>

/* use same syntax as Linux for simplicity */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define container_of(ptr, type, member) \
	({const typeof(((type *)0)->member)*__memberptr = (ptr); \
	(type *)((char *)__memberptr - offsetof(type, member)); })

/* Align the number to the nearest alignment value */
#define ALIGN_UP(size, alignment) \
	(((size) % (alignment) == 0) ? (size) : \
	((size) - ((size) % (alignment)) + (alignment)))
#define ALIGN_DOWN(size, alignment) \
	((size) - ((size) % (alignment)))
#define ALIGN ALIGN_UP

#define __packed __attribute__((packed))

#define __aligned(x) __attribute__((__aligned__(x)))

/* count number of var args */
#define PP_NARG(...) (sizeof((unsigned int[]){0, ##__VA_ARGS__}) \
	/ sizeof(unsigned int) - 1)

/* compile-time assertion */
#define STATIC_ASSERT(COND, MESSAGE)	\
	__attribute__((unused))		\
	typedef char META_CONCAT(assertion_failed_, MESSAGE)[(COND) ? 1 : -1]

/** \name Declare module macro
 *  \brief Usage at the end of an independent module file:
 *         DECLARE_MODULE(sys_*_init);
 *  @{
 */
#ifdef UNIT_TEST
#define DECLARE_MODULE(init)
#elif CONFIG_LIBRARY
/* In case of shared libs components are initialised in dlopen */
#define DECLARE_MODULE(init) __attribute__((constructor)) \
	static void _module_init(void) { init(); }
#else
#define DECLARE_MODULE(init) __attribute__((__used__)) \
	__attribute__((section(".module_init"))) static void(*f)(void) = init
#endif

#endif /* __SOF_COMMON_H__ */
