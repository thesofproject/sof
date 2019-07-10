/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_CACHE_H__
#define __ARCH_CACHE_H__

#include <stdint.h>
#include <stddef.h>

static inline void dcache_writeback_region(void *addr, size_t size) {}
static inline void dcache_invalidate_region(void *addr, size_t size) {}
static inline void icache_invalidate_region(void *addr, size_t size) {}
static inline void dcache_writeback_invalidate_region(void *addr,
	size_t size) {}

#endif /* __ARCH_CACHE_H__ */
