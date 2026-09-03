/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOF_COMPILER_ATTRIBUTES_H__
#define __SOF_COMPILER_ATTRIBUTES_H__

#include <zephyr/toolchain.h>

/* Get __sparse_cache and __sparse_force definitions if __CHECKER__ is defined */
#include <zephyr/debug/sparse.h>

#ifndef __aligned
#define __aligned(x) __attribute__((__aligned__(x)))
#endif

#ifndef __section
#define __section(x) __attribute__((section(x)))
#endif

/* The fallthrough attribute is supported since GCC 7.0
 * and Clang 10.0.0.
 *
 * Note that Clang sets __GNUC__ == 4 so the GCC version
 * test will not be true here, and must go through
 * the Clang version test.
 */
#if ((defined(__GNUC__) && (__GNUC__ >= 7)) || \
     (defined(__clang__) && (__clang_major__ >= 10))) && !defined(__CHECKER__)
#define COMPILER_FALLTHROUGH __attribute__((fallthrough))
#else
#define COMPILER_FALLTHROUGH  /* fallthrough */
#endif

#endif /* __SOF_COMPILER_ATTRIBUTES_H__ */
