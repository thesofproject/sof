/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * These contents may have been developed with support from one or more
 * Intel-operated generative artificial intelligence solutions.
 */

#ifndef __TEST_ZTEST_UNIT_COMMON_MOD_ALLOC_H__
#define __TEST_ZTEST_UNIT_COMMON_MOD_ALLOC_H__

#include <stddef.h>

/**
 * @file
 *
 * Module resource allocator for ztest unit suites that exercise code calling
 * the mod_alloc()/mod_free() API but do not link the production allocator in
 * src/audio/module_adapter/module/generic.c (which pulls in the whole module
 * adapter, IPC and objpool infrastructure).
 *
 * The implementation in mod_alloc.c forwards every allocation to the SOF heap
 * through sof_ctx_alloc()/sof_ctx_free(), so a suite exercises whichever SOF
 * allocator it links, for example the real zephyr/lib/alloc.c in either its
 * Zephyr sys_heap or its native_sim host heap
 * (CONFIG_SOF_NATIVE_SIM_HOST_HEAP) flavour.
 *
 * Like the production allocator it tracks the live allocations, so
 * mod_free_all() works and a suite can detect leaked, double-freed or foreign
 * pointers on its own, without relying on ASan/LSan and the host heap. The
 * tracking is not thread safe; unit suites are single threaded.
 */

/** @brief Number of live module allocations a suite may have at any time. */
#ifndef MOD_ALLOC_TEST_MAX_ALLOCATIONS
#define MOD_ALLOC_TEST_MAX_ALLOCATIONS 128
#endif

/**
 * @brief Return the number of module allocations that are currently live.
 * @return Number of pointers handed out and not yet freed.
 */
size_t mod_alloc_test_allocations(void);

/**
 * @brief Return the number of bytes currently allocated by modules.
 * @return Sum of the requested sizes of all live allocations.
 */
size_t mod_alloc_test_bytes(void);

/**
 * @brief Free every live module allocation, whichever module owns it.
 *
 * Used by mod_alloc_test_after() to keep test cases independent and available
 * to suites that install their own ztest teardown.
 */
void mod_alloc_test_free_all(void);

/**
 * @brief ztest after-hook asserting that a test case leaked no module memory.
 *
 * Matches the ztest after_fn signature, so a suite can install it directly:
 *
 *	ZTEST_SUITE(my_suite, NULL, NULL, NULL, mod_alloc_test_after, NULL);
 *
 * Any leaked allocation is freed before the assertion, so one failing test
 * case does not cascade into the following ones.
 *
 * @param fixture ztest suite fixture, unused.
 */
void mod_alloc_test_after(void *fixture);

#endif /* __TEST_ZTEST_UNIT_COMMON_MOD_ALLOC_H__ */
