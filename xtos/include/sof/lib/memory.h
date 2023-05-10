/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_LIB_MEMORY_H__
#define __SOF_LIB_MEMORY_H__

#include <platform/lib/memory.h>

#if !defined(ASSEMBLY) && !defined(LINKER)
#include <sof/compiler_attributes.h>

static inline void __sparse_cache *uncache_to_cache(void *address)
{
	return platform_uncache_to_cache(address);
}

static inline void *cache_to_uncache(void __sparse_cache *address)
{
	return platform_cache_to_uncache(address);
}
#endif /* !ASSEMBLY && !LINKER */

#endif /* __SOF_LIB_MEMORY_H__ */
