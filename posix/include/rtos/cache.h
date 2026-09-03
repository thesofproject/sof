/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file xtos/include/rtos/cache.h
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

#endif /* __SOF_LIB_CACHE_H__ */
