/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file posix/include/rtos/cache.h
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

/*
 * The host has a single address space, so the cached and uncached aliases
 * of an address are the same.
 */
#define uncache_to_cache(address)	address
#define cache_to_uncache(address)	address
#define is_uncached(address)		1

#endif /* __SOF_LIB_CACHE_H__ */
