/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Stub for <zephyr/sys/util.h> used by testbench / posix builds.
 */

#ifndef __POSIX_ZEPHYR_SYS_UTIL_H__
#define __POSIX_ZEPHYR_SYS_UTIL_H__

#include <stddef.h>

#ifndef KB
#define KB(x) (((size_t)(x)) << 10)
#endif

#ifndef MB
#define MB(x) (KB(x) << 10)
#endif

#endif /* __POSIX_ZEPHYR_SYS_UTIL_H__ */
