/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file include/sof/lib/cache.h
 * \brief Cache header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_LIB_CACHE_H__
#define __SOF_LIB_CACHE_H__

#include <arch/lib/cache.h>

/* writeback and invalidate data */
#define CACHE_WRITEBACK_INV	0

/* invalidate data */
#define CACHE_INVALIDATE	1

/* data cache line alignment */
#if DCACHE_LINE_SIZE > 0
#define PLATFORM_DCACHE_ALIGN	DCACHE_LINE_SIZE
#else
#define PLATFORM_DCACHE_ALIGN	sizeof(void *)
#endif

#endif /* __SOF_LIB_CACHE_H__ */
