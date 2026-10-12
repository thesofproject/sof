/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Jyri Sarha <jyri.sarha@linux.intel.com>
 */

/**
 * @file sof/lib/fast-get.h
 * @brief Dynamic SRAM staging and cache mechanisms for cold DRAM-resident modules.
 * @ingroup memory_api
 */

#ifndef __SOF_LIB_FAST_GET_H__
#define __SOF_LIB_FAST_GET_H__

#include <stddef.h>

/** \addtogroup memory_api
 *  @{
 */

struct k_mem_domain;
struct mod_alloc_ctx;

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
/**
 * @brief Stage DRAM-resident code or data into fast SRAM for execution.
 * @param[in,out] alloc Pointer to allocation context.
 * @param[in] dram_ptr Pointer to source data resident in DRAM.
 * @param[in] size Size in bytes to stage.
 * @return Pointer to staged SRAM memory, or NULL on allocation failure.
 */
const void *fast_get(struct mod_alloc_ctx *alloc, const void * const dram_ptr, size_t size);

/**
 * @brief Release fast SRAM memory previously allocated by fast_get().
 * @param[in,out] alloc Pointer to allocation context.
 * @param[in,out] mdom Pointer to memory domain. Can be NULL for kernel threads or small allocations
 *                     (<= FAST_GET_MAX_COPY_SIZE); required for large userspace staging buffers to
 *                     remove the partition and prevent memory domain partition leaks.
 * @param[in,out] sram_ptr Pointer to staged SRAM memory to release.
 */
void fast_put(struct mod_alloc_ctx *alloc, struct k_mem_domain *mdom, const void *sram_ptr);
#else
static inline const void *fast_get(struct mod_alloc_ctx *alloc, const void * const dram_ptr,
				   size_t size)
{
	return dram_ptr;
}
static inline void fast_put(struct mod_alloc_ctx *alloc, struct k_mem_domain *mdom,
			    const void *sram_ptr) {}
#endif

/** @}*/

#endif /* __SOF_LIB_FAST_GET_H__ */
