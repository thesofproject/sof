/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __SOF_COMMON_H__
#define __SOF_COMMON_H__

#include <sof/trace/preproc.h>
#include <stddef.h>

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

#define ffs(i) __builtin_ffs(i)
#define ffsl(i) __builtin_ffsl(i)
#define ffsll(i) __builtin_ffsll(i)

/* count number of var args */
#define PP_NARG(...) (sizeof((unsigned int[]){0, ##__VA_ARGS__}) \
	/ sizeof(unsigned int) - 1)

/* compile-time assertion */
#define STATIC_ASSERT(COND, MESSAGE)	\
	__attribute__((unused))		\
	typedef char META_CONCAT(assertion_failed_, MESSAGE)[(COND) ? 1 : -1]

/* Allows checking preprocessor symbols in compile-time.
 * Returns true for config with value 1, false for undefined or any other value.
 *
 * Expression like (CONFIG_MY ? a : b) rises compilation error when CONFIG_MY
 * is undefined, (IS_ENABLED(CONFIG_MY) ? a : b) can be used instead of it.
 *
 * It is intended to be used with Kconfig's bool configs - it should rise
 * error for other types, except positive integers, but it shouldn't be
 * used with them.
 */
#define IS_ENABLED(config) IS_ENABLED_STEP_1(config)
#define IS_ENABLED_DUMMY_1 0,
#define IS_ENABLED_STEP_1(config) IS_ENABLED_STEP_2(IS_ENABLED_DUMMY_ ## config)
#define IS_ENABLED_STEP_2(values) IS_ENABLED_STEP_3(values 1, 0)
#define IS_ENABLED_STEP_3(ignore, value, ...) (!!(value))

#endif /* __SOF_COMMON_H__ */
