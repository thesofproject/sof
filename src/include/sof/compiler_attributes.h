/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#ifndef __ZEPHYR__

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#endif

#ifndef __aligned
#define __aligned(x) __attribute__((__aligned__(x)))
#endif

#ifndef __section
#define __section(x) __attribute__((section(x)))
#endif

#if defined(__clang__) || !defined(__XCC__)

#define COMPILER_FALLTHROUGH __attribute__((fallthrough))

#else

#define COMPILER_FALLTHROUGH  /* fallthrough */

#endif
