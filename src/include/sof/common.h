/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifndef __SOF_COMMON_H__
#define __SOF_COMMON_H__

#if !defined(LINKER)

/* Align the number to the nearest alignment value */
#define IS_ALIGNED(size, alignment) ((size) % (alignment) == 0)

/* Treat zero as a special case because it wraps around */
#define is_power_of_2(x) ((x) && !((x) & ((x) - 1)))

#define compile_check(x) (sizeof(struct{int _f : 1 - !(x); }))
#define compile_fail_zero_or_false_fn(x) ({int _f[(x) - 1]; 0 & _f[0]; })

#define ALIGN_UP_INTERNAL(val, align) (((val) + (align) - 1) & ~((align) - 1))

#define VERIFY_ALIGN
#ifdef VERIFY_ALIGN

/* Using this when 'alignment' is a constant and when compiling with gcc
 * -O0 saves about 30 bytes of .text and a few CPU cycles compared to
 * the ALIGN_UP() combined check. There's no .text difference when
 * optimizing.
 */
#define ALIGN_UP_COMPILE(size, alignment)					\
	(compile_check(is_power_of_2(alignment)) ?		\
	 ALIGN_UP_INTERNAL(size, alignment) : 0xBADCAFE)

#ifdef __XCC__

/*
 * xcc doesn't support __builtin_constant_p() so we can only do run-time
 * verification
 */

#define ALIGN_UP(size, alignment) ({						\
	if (!is_power_of_2(alignment))					\
		panic(SOF_IPC_PANIC_ASSERT);					\
	ALIGN_UP_INTERNAL(size, alignment);					\
})

#define ALIGN_DOWN(size, alignment) ({						\
	if (!is_power_of_2(alignment))					\
		panic(SOF_IPC_PANIC_ASSERT);					\
	(size) & ~((alignment) - 1);						\
})

#else /* not __XCC__ */

/* If we can't tell at compile time, assume it's OK and defer to run-time */
#define COMPILE_TIME_ALIGNED(align) (!__builtin_constant_p(align) ||		\
				     is_power_of_2(align))

#define ALIGN_UP(size, alignment) ({						\
	if (compile_fail_zero_or_false_fn(COMPILE_TIME_ALIGNED(alignment)) ||	\
	    !is_power_of_2(alignment))						\
		panic(SOF_IPC_PANIC_ASSERT);					\
	ALIGN_UP_INTERNAL(size, alignment);					\
})

#define ALIGN_DOWN(size, alignment) ({						\
	if (compile_fail_zero_or_false_fn(COMPILE_TIME_ALIGNED(alignment)) ||	\
	    !is_power_of_2(alignment))					\
		panic(SOF_IPC_PANIC_ASSERT);					\
	(size) & ~((alignment) - 1);						\
})

#endif /* not __XCC__ */

#else /* not RUNTIME_ALIGN_CHECK */

#define ALIGN_UP(size, alignment) ALIGN_UP_INTERNAL(size, alignment)
#define ALIGN_UP_COMPILE ALIGN_UP
#define ALIGN_DOWN(size, alignment) ((size) & ~((alignment) - 1))

#endif

/* This most basic ALIGN() must be used in header files that are
 * included in both C and assembly code. memory.h files require this
 * exact spelling matching the linker function because memory.h values
 * are _also_ copied unprocessed to the .x[.in] linker script
 */
#define ALIGN(val, align) ALIGN_UP_INTERNAL(val, align)
#define DIV_ROUND_UP(val, div) (((val) + (div) - 1) / (div))

#if !defined(__ASSEMBLER__)

#include <sof/trace/preproc.h>
#include <sof/compiler_attributes.h>
#include <stddef.h>

/* use same syntax as Linux for simplicity */
#ifndef __ZEPHYR__ /* Already present and compatible via Zephyr headers */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
#define container_of(ptr, type, member) \
	({const typeof(((type *)0)->member)*__memberptr = (ptr); \
	(type *)((char *)__memberptr - offsetof(type, member)); })

#define ffs(i) __builtin_ffs(i)
#define ffsl(i) __builtin_ffsl(i)
#define ffsll(i) __builtin_ffsll(i)

#define clz(i) __builtin_clz(i)
#define clzl(i) __builtin_clzl(i)
#define clzll(i) __builtin_clzll(i)

#define popcount(x) __builtin_popcount(x)

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
#ifndef __ZEPHYR__ /* Already present and compatible via Zephyr headers */
#define IS_ENABLED(config) IS_ENABLED_STEP_1(config)
#define IS_ENABLED_DUMMY_1 0,
#define IS_ENABLED_STEP_1(config) IS_ENABLED_STEP_2(IS_ENABLED_DUMMY_ ## config)
#define IS_ENABLED_STEP_2(values) IS_ENABLED_STEP_3(values 1, 0)
#define IS_ENABLED_STEP_3(ignore, value, ...) (!!(value))
#endif

#if defined __XCC__
/* XCC does not currently check alignment for packed data so no need for any
 * compiler hints.
 */
#define ASSUME_ALIGNED(x, a)	x
#else
/* GCC 9.x has checks for pointer alignment within packed structures when
 * cast from one type to another. This macro should only be used after checking
 * alignment. This compiler builtin may not be available on all compilers so
 * this macro can be defined as NULL if needed.
 */
#define ASSUME_ALIGNED(x, a) __builtin_assume_aligned(x, a)
#endif /* __XCC__ */

#endif /* __ASSEMBLER__ */
#else /* LINKER */

#define ALIGN_UP_INTERNAL(val, align) (((val) + (align) - 1) & ~((align) - 1))

#endif /* LINKER */
#endif /* __SOF_COMMON_H__ */
