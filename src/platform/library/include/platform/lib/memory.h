/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_LIB_MEMORY_H__

#ifndef __PLATFORM_LIB_MEMORY_H__
#define __PLATFORM_LIB_MEMORY_H__

#include <inttypes.h>
#include <stddef.h>

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <execinfo.h>
#include <sof/debug/panic.h>
#include <sof/lib/cache.h>

struct sof;

#define PLATFORM_DCACHE_ALIGN	sizeof(void *)

#define SOF_STACK_SIZE		0x1000

uint8_t *get_library_mailbox(void);

#define MAILBOX_BASE	get_library_mailbox()

#define PLATFORM_HEAP_SYSTEM		2
#define PLATFORM_HEAP_SYSTEM_RUNTIME	CONFIG_CORE_COUNT
#define PLATFORM_HEAP_RUNTIME		1
#define PLATFORM_HEAP_BUFFER		2
#define PLATFORM_HEAP_SYSTEM_SHARED	1
#define PLATFORM_HEAP_RUNTIME_SHARED	1

#define SHARED_DATA __attribute__((aligned(64)))

#ifdef TESTBENCH_CACHE_CHECK
/*
 * Use uncache address from caller and return cache[core] address. This can
 * result in.
 *
 * 1) Creating a new cache mapping for a heap object.
 * 2) Creating a new cache and unache mapping for a DATA section object.
 */
static inline void *_uncache_to_cache(void *address, const char *file,
				      const char *func, int line, size_t size)
{
	struct tb_cache_elem *elem;
	int core = _cache_find_core(func, line);

	fprintf(stdout, "\n\n");

	/* uncache area not found so this must be DATA section*/
	fprintf(stdout, "uncache -> cache: %s() line %d size %zu - %s\n", func,
		line, size, file);

	_cache_dump_address_type(address, size);
	_cache_dump_backtrace();
	_cache_dump_cacheline("uncache -> cache", address, 0, size, size, NULL);

	/* find elem with uncache address */
	elem = _cache_get_elem_from_uncache(address);
	if (!elem) {
		/* no elem found so create one */
		elem = _cache_new_uelem(address, core, func, line,
				CACHE_DATA_TYPE_DATA_UNCACHE, size,
				CACHE_ACTION_NONE);
		if (!elem)
			return NULL;

	}

	return elem->cache[core].data;
}

#define uncache_to_cache(address)	\
	_uncache_to_cache(address, __FILE__, __func__, __LINE__, sizeof(*address))

/*
 * Use uncache address from caller and return cache[core] address. This can
 * result in.
 *
 * 1) Creating a new cache mapping for a heap object.
 * 2) Creating a new cache and unache mapping for a DATA section object.
 */
static inline void *_cache_to_uncache(void *address, const char *file,
				      const char *func, int line, size_t size)
{
	struct tb_cache_elem *elem;
	int core = _cache_find_core(func, line);

	fprintf(stdout, "\n\n");

	/* uncache area not found so this must be DATA section*/
	fprintf(stdout, "cache -> uncache: %s() line %d\n new object size %zu - %s\n",
		func, line, size, file);

	_cache_dump_address_type(address, size);
	_cache_dump_backtrace();
	_cache_dump_cacheline("cache -> uncache", address, 0, size, size, NULL);

	elem = _cache_get_elem_from_cache(address, core);
	if (!elem) {
		/* no elem found so create one */
		elem = _cache_new_celem(address, core, func, line,
				CACHE_DATA_TYPE_DATA_CACHE, size,
				CACHE_ACTION_NONE);
		if (!elem)
			return NULL;
	}

	return elem->uncache.data;
}

#define cache_to_uncache(address) \
	_cache_to_uncache(address, __FILE__, __func__, __LINE__, sizeof(*address))

static inline int _is_uncache(void *address, const char *file,
			      const char *func, int line, size_t size)
{
	struct tb_cache_elem *elem;

	fprintf(stdout, "\n\n");

	/* find elem with uncache address - NOT FOOLPROOF on host */
	elem = _cache_get_elem_from_uncache(address);
	if (elem) {
		fprintf(stdout, "is uncache found: %s() line %d - %s\n",
			func, line, file);
		return 1;
	}

	fprintf(stdout, "is uncache not found: %s() line %d - %s\n",
		func, line, file);
	return 0;
}

#define cache_to_uncache_init(address)	address

/* check for memory type - not foolproof but enough to help developers */
#define is_uncached(address)						\
	_is_uncache(address, __FILE__, __func__, __LINE__, sizeof(*address))

#define platform_shared_get(ptr, bytes)					\
	({fprintf(stdout, "platform_get_shared\n");			\
	dcache_invalidate_region(ptr, bytes);				\
	_cache_to_uncache(ptr, __FILE__, __func__, __LINE__,		\
			  sizeof(*ptr)); })

#define platform_rfree_prepare(ptr) \
	({fprintf(stdout, "prepare free %s() line %d size - %s\n",	\
		__func__, __LINE__, sizeof(*ptr), __FILE__); ptr; })

#else

#define cache_to_uncache_init(address)	address
#define cache_to_uncache(address)	address
#define uncache_to_cache(address)	address
#define is_uncached(address)		0
#define platform_shared_get(ptr, bytes)	ptr
#define platform_rfree_prepare(ptr)	ptr

#endif /* CONFIG_TESTBENCH_CACHE_CHECK */

void platform_init_memmap(struct sof *sof);

#define ARCH_OOPS_SIZE	0

static inline void *arch_get_stack_entry(void)
{
	return NULL;
}

static inline uint32_t arch_get_stack_size(void)
{
	return 0;
}

/* NOTE - FAKE Memory configurations are used by UT's to test allocator */

#define SRAM_BANK_SIZE	0x10000
#define LP_SRAM_SIZE SRAM_BANK_SIZE
/* use big enough HP_SRAM_SIZE to build all components for the test bench at once */
#define HP_SRAM_SIZE (SRAM_BANK_SIZE * 47)

#define HP_SRAM_BASE	0xBE000000
#define LP_SRAM_BASE	0xBE800000

#define SOF_FW_END		(HP_SRAM_BASE + HP_SRAM_SIZE)

/* Heap section sizes for system runtime heap for primary core */
#define HEAP_SYS_RT_0_COUNT64		128
#define HEAP_SYS_RT_0_COUNT512		16
#define HEAP_SYS_RT_0_COUNT1024		4

/* Heap section sizes for system runtime heap for secondary core */
#define HEAP_SYS_RT_X_COUNT64		64
#define HEAP_SYS_RT_X_COUNT512		8
#define HEAP_SYS_RT_X_COUNT1024		4

/* Heap section sizes for module pool */
#define HEAP_COUNT64		128
#define HEAP_COUNT128	64
#define HEAP_COUNT256	128
#define HEAP_COUNT512	8
#define HEAP_COUNT1024	4
#define HEAP_COUNT2048	1
#define HEAP_COUNT4096	1

/* Heap configuration */
#define HEAP_RUNTIME_SIZE \
	(HEAP_COUNT64 * 64 + HEAP_COUNT128 * 128 + \
	HEAP_COUNT256 * 256 + HEAP_COUNT512 * 512 + \
	HEAP_COUNT1024 * 1024 + HEAP_COUNT2048 * 2048 + \
	HEAP_COUNT4096 * 4096)

/* Heap section sizes for runtime shared heap */
#define HEAP_RUNTIME_SHARED_COUNT64	(64 + 32 * CONFIG_CORE_COUNT)
#define HEAP_RUNTIME_SHARED_COUNT128	64
#define HEAP_RUNTIME_SHARED_COUNT256	4
#define HEAP_RUNTIME_SHARED_COUNT512	16
#define HEAP_RUNTIME_SHARED_COUNT1024	4

#define HEAP_RUNTIME_SHARED_SIZE \
	(HEAP_RUNTIME_SHARED_COUNT64 * 64 + HEAP_RUNTIME_SHARED_COUNT128 * 128 + \
	HEAP_RUNTIME_SHARED_COUNT256 * 256 + HEAP_RUNTIME_SHARED_COUNT512 * 512 + \
	HEAP_RUNTIME_SHARED_COUNT1024 * 1024)

/* Heap section sizes for system shared heap */
#define HEAP_SYSTEM_SHARED_SIZE		0x1500

#define HEAP_BUFFER_BLOCK_SIZE		0x100
#define HEAP_BUFFER_COUNT_MAX	(HP_SRAM_SIZE / HEAP_BUFFER_BLOCK_SIZE)

#define HEAP_SYSTEM_M_SIZE		0x4000 /* heap primary core size */
#define HEAP_SYSTEM_S_SIZE		0x3000 /* heap secondary core size */

#define HEAP_SYSTEM_T_SIZE \
	(HEAP_SYSTEM_M_SIZE + ((CONFIG_CORE_COUNT - 1) * HEAP_SYSTEM_S_SIZE))

#define HEAP_SYS_RUNTIME_M_SIZE \
	(HEAP_SYS_RT_0_COUNT64 * 64 + HEAP_SYS_RT_0_COUNT512 * 512 + \
	HEAP_SYS_RT_0_COUNT1024 * 1024)

#define HEAP_SYS_RUNTIME_S_SIZE \
	(HEAP_SYS_RT_X_COUNT64 * 64 + HEAP_SYS_RT_X_COUNT512 * 512 + \
	HEAP_SYS_RT_X_COUNT1024 * 1024)

#define HEAP_SYS_RUNTIME_T_SIZE \
	(HEAP_SYS_RUNTIME_M_SIZE + ((CONFIG_CORE_COUNT - 1) * \
	HEAP_SYS_RUNTIME_S_SIZE))

/* Heap section sizes for module pool */
#define HEAP_RT_LP_COUNT8			0
#define HEAP_RT_LP_COUNT16			256
#define HEAP_RT_LP_COUNT32			128
#define HEAP_RT_LP_COUNT64			64
#define HEAP_RT_LP_COUNT128			64
#define HEAP_RT_LP_COUNT256			96
#define HEAP_RT_LP_COUNT512			8
#define HEAP_RT_LP_COUNT1024			4

/* Heap configuration */
#define SOF_LP_DATA_SIZE			0x4000

#define HEAP_LP_SYSTEM_BASE		(LP_SRAM_BASE + SOF_LP_DATA_SIZE)
#define HEAP_LP_SYSTEM_SIZE		0x1000

#define HEAP_LP_RUNTIME_BASE \
	(HEAP_LP_SYSTEM_BASE + HEAP_LP_SYSTEM_SIZE)
#define HEAP_LP_RUNTIME_SIZE \
	(HEAP_RT_LP_COUNT8 * 8 + HEAP_RT_LP_COUNT16 * 16 + \
	HEAP_RT_LP_COUNT32 * 32 + HEAP_RT_LP_COUNT64 * 64 + \
	HEAP_RT_LP_COUNT128 * 128 + HEAP_RT_LP_COUNT256 * 256 + \
	HEAP_RT_LP_COUNT512 * 512 + HEAP_RT_LP_COUNT1024 * 1024)

#define HEAP_LP_BUFFER_BLOCK_SIZE		0x180
#define HEAP_LP_BUFFER_COUNT \
	(HEAP_LP_BUFFER_SIZE / HEAP_LP_BUFFER_BLOCK_SIZE)

#define HEAP_LP_BUFFER_BASE LP_SRAM_BASE
#define HEAP_LP_BUFFER_SIZE LP_SRAM_SIZE

/* SOF Core S configuration */
#define SOF_CORE_S_SIZE \
	ALIGN((HEAP_SYSTEM_S_SIZE + HEAP_SYS_RUNTIME_S_SIZE + SOF_STACK_SIZE),\
	SRAM_BANK_SIZE)
#define SOF_CORE_S_T_SIZE ((CONFIG_CORE_COUNT - 1) * SOF_CORE_S_SIZE)

#define host_to_local(addr) (addr)
#define local_to_host(addr) (addr)

#endif /* __PLATFORM_LIB_MEMORY_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/memory.h"

#endif /* __SOF_LIB_MEMORY_H__ */
