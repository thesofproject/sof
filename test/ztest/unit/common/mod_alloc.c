// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// These contents may have been developed with support from one or more
// Intel-operated generative artificial intelligence solutions.
//
// Module resource allocator for ztest unit suites, see mod_alloc.h.

#include <zephyr/ztest.h>

#include <rtos/alloc.h>
#include <sof/audio/module_adapter/module/generic.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mod_alloc.h"

/*
 * The definitions below are named after the mod_alloc() API, not after its
 * z_impl_*() back ends. generic.h only maps the two onto each other without
 * CONFIG_USERSPACE; with userspace the API becomes a set of __syscall
 * declarations and these definitions would no longer replace it.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_USERSPACE),
	     "the ztest module allocator does not implement the mod_alloc() syscalls");

/** @brief Book-keeping entry for one live module allocation. */
struct mod_alloc_test_block {
	const void *ptr;
	size_t size;
};

static struct mod_alloc_test_block mod_alloc_blocks[MOD_ALLOC_TEST_MAX_ALLOCATIONS];
static size_t mod_alloc_count;
static size_t mod_alloc_bytes;

/**
 * @brief Record an allocation.
 * @param ptr Allocated pointer.
 * @param size Requested size in bytes.
 * @return true when the allocation was recorded, false when the table is full.
 */
static bool mod_alloc_block_add(const void *ptr, size_t size)
{
	if (mod_alloc_count == ARRAY_SIZE(mod_alloc_blocks)) {
		return false;
	}

	mod_alloc_blocks[mod_alloc_count].ptr = ptr;
	mod_alloc_blocks[mod_alloc_count].size = size;
	mod_alloc_count++;
	mod_alloc_bytes += size;

	return true;
}

/**
 * @brief Drop the record of an allocation.
 * @param ptr Pointer to look up.
 * @return true when @p ptr was a live allocation, false otherwise.
 */
static bool mod_alloc_block_del(const void *ptr)
{
	size_t i;

	for (i = 0; i < mod_alloc_count; i++) {
		if (mod_alloc_blocks[i].ptr != ptr) {
			continue;
		}

		mod_alloc_bytes -= mod_alloc_blocks[i].size;
		mod_alloc_count--;
		mod_alloc_blocks[i] = mod_alloc_blocks[mod_alloc_count];

		return true;
	}

	return false;
}

/**
 * @brief Allocate and record memory, the back end of the mod_alloc() API.
 * @param flags SOF_MEM_FLAG_* allocation flags.
 * @param size Size in bytes, zero is rejected like in the production allocator.
 * @param alignment Alignment in bytes, 0 for the default.
 * @return Pointer to the allocated memory or NULL on failure.
 */
static void *mod_alloc_test_alloc(uint32_t flags, size_t size, size_t alignment)
{
	void *ptr;

	if (!size) {
		return NULL;
	}

	ptr = sof_ctx_alloc(NULL, flags, size, alignment);
	if (!ptr) {
		return NULL;
	}

	if (!mod_alloc_block_add(ptr, size)) {
		sof_ctx_free(NULL, ptr);
		return NULL;
	}

	return ptr;
}

/**
 * @brief Allocate an aligned buffer for a module.
 *
 * Uses the same flags as z_impl_mod_balloc_align().
 *
 * @param mod Module the memory is allocated for, only used for book-keeping.
 * @param size Size in bytes.
 * @param alignment Alignment in bytes, 0 for the default.
 * @return Pointer to the allocated memory or NULL on failure.
 */
void *mod_balloc_align(struct processing_module *mod, size_t size, size_t alignment)
{
	(void)mod;

	return mod_alloc_test_alloc(SOF_MEM_FLAG_USER | SOF_MEM_FLAG_LARGE_BUFFER, size, alignment);
}

/**
 * @brief Allocate memory for a module with explicit allocator flags.
 * @param mod Module the memory is allocated for, only used for book-keeping.
 * @param flags SOF_MEM_FLAG_* allocation flags.
 * @param size Size in bytes.
 * @param alignment Alignment in bytes, 0 for the default.
 * @return Pointer to the allocated memory or NULL on failure.
 */
void *mod_alloc_ext(struct processing_module *mod, uint32_t flags, size_t size, size_t alignment)
{
	(void)mod;

	return mod_alloc_test_alloc(flags, size, alignment);
}

/**
 * @brief Free memory allocated with mod_alloc_ext() or mod_balloc_align().
 * @param mod Module the memory was allocated for, only used for book-keeping.
 * @param ptr Pointer to free, NULL is ignored.
 * @return 0 on success, -EINVAL if @p ptr is not a live module allocation.
 */
int mod_free(struct processing_module *mod, const void *ptr)
{
	(void)mod;

	if (!ptr) {
		return 0;
	}

	if (!mod_alloc_block_del(ptr)) {
		return -EINVAL;
	}

	sof_ctx_free(NULL, (void *)ptr);

	return 0;
}

/**
 * @brief Free every allocation still held by a module.
 * @param mod Module to release, this allocator does not track ownership so all
 *	      live allocations are released.
 */
void mod_free_all(struct processing_module *mod)
{
	(void)mod;

	mod_alloc_test_free_all();
}

size_t mod_alloc_test_allocations(void)
{
	return mod_alloc_count;
}

size_t mod_alloc_test_bytes(void)
{
	return mod_alloc_bytes;
}

void mod_alloc_test_free_all(void)
{
	while (mod_alloc_count) {
		mod_alloc_count--;
		sof_ctx_free(NULL, (void *)mod_alloc_blocks[mod_alloc_count].ptr);
	}

	mod_alloc_bytes = 0;
}

void mod_alloc_test_after(void *fixture)
{
	size_t leaked = mod_alloc_count;
	size_t leaked_bytes = mod_alloc_bytes;

	(void)fixture;

	/* Release the leak so that the next test case starts from a clean heap */
	mod_alloc_test_free_all();

	zassert_equal(leaked, 0, "Test case leaked %zu module allocation(s), %zu bytes", leaked,
		      leaked_bytes);
}
