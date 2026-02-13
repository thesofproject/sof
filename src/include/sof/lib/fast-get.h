/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Jyri Sarha <jyri.sarha@linux.intel.com>
 */

#ifndef __SOF_LIB_FAST_GET_H__
#define __SOF_LIB_FAST_GET_H__

#include <stddef.h>

struct k_heap;

/*
 * When built for SOF, fast_get() and fast_put() are only needed when DRAM
 * storage and execution is enabled (CONFIG_COLD_STORE_EXECUTE_DRAM=y), but not
 * when building LLEXT extensions (!defined(LL_EXTENSION_BUILD)), using Zephyr
 * SDK (CONFIG_LLEXT_TYPE_ELF_RELOCATABLE=n while
 * CONFIG_LLEXT_TYPE_ELF_SHAREDLIB=y).
 * For unit-testing full versions of fast_get() and fast_put() are checked by
 * test/ztest/unit/fast-get/ and test/cmocka/src/lib/fast-get/
 */
#if (CONFIG_COLD_STORE_EXECUTE_DRAM &&					\
	(CONFIG_LLEXT_TYPE_ELF_RELOCATABLE || !defined(LL_EXTENSION_BUILD))) || \
	!CONFIG_SOF_FULL_ZEPHYR_APPLICATION
const void *fast_get(struct k_heap *heap, const void * const dram_ptr, size_t size);
void fast_put(struct k_heap *heap, const void *sram_ptr);
#else
static inline const void *fast_get(struct k_heap *heap, const void * const dram_ptr, size_t size)
{
	return dram_ptr;
}
static inline void fast_put(struct k_heap *heap, const void *sram_ptr) {}
#endif

#endif /* __SOF_LIB_FAST_GET_H__ */
