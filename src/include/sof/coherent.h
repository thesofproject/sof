/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_COHERENT_H__
#define __SOF_COHERENT_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sof/spinlock.h>
#include <sof/list.h>
#include <sof/lib/memory.h>
#include <sof/lib/cpu.h>

#define __coherent __attribute__((packed, aligned(DCACHE_LINE_SIZE)))

/*
 * The coherent API allows optimized access to memory by multiple cores, using
 * cache, taking care about coherence. The intended use is to let cores acquire
 * ownership of such shared objects, use them, and then release them, possibly
 * to be re-acquired by other cores. Such shared objects must only be accessed
 * via this API. It's designed to be primarily used with dynamically allocated
 * objects because of their well-defined life span. It can also be used with
 * objects from .data or .bss sections but greater care must be takenwith them
 * to strictly follow the API flow.
 *
 * The API assumes, that in the beginning no core has cache lines associated
 * with the memory area, used with it. That is true for dynamically allocated
 * memory, because when such memory is freed, its cache is invalidated - as long
 * as that memory was never accessed by other cores, except by using this API.
 * The first call must be coherent_init(), which initializes the header. If the
 * object will be used by multiple cores, next coherent_shared() must be called.
 * After that to use that memory, coherent_acquire() must be called, which
 * acquires ownership of the object and returns a cached address of the memory.
 * After that the user can perform cached access to the memory. To release the
 * memory,  coherent_release() must be called. The only time when the memory is
 * accessed using cache is between those two calls, so only when releasing the
 * memory we have to write back and invalidate caches to make sure, that next
 * time we acquire this memory, our uncached header access will not be
 * overwritten! When memory is not needed any more, typically before freeing the
 * memory, coherent_free() should be called.
 *
 * This structure needs to be embedded at the start of any container to ensure
 * container object cache alignment and to minimise non cache access when
 * acquiring ownership.
 *
 * This structure must not be accessed outside of these APIs.
 * The shared flag is only set at coherent init and thereafter it's RO.
 */
struct coherent {
#if 1
	struct cache_debug debug;
#endif
	struct k_spinlock lock;	/* locking mechanism */
	k_spinlock_key_t key;	/* lock flags */
	uint16_t shared;	/* shared on other non coherent cores */
	uint16_t core;		/* owner core if not shared */
	struct list_item list;	/* coherent list iteration */
} __coherent;

/* debug address aliases */
#ifdef COHERENT_CHECK_ALIAS
#define ADDR_IS_INCOHERENT(_c) assert(!is_uncached(_c))
#define ADDR_IS_COHERENT(_c) assert(is_uncached(_c))
#else
#define ADDR_IS_INCOHERENT(_c)
#define ADDR_IS_COHERENT(_c)
#endif

/* debug sharing amongst cores */
#ifdef COHERENT_CHECK_NONSHARED_CORES
#define CHECK_COHERENT_CORE(_c) assert((_c)->core == cpu_get_id())
#else
#define CHECK_COHERENT_CORE(_c)
#endif

#if CONFIG_INCOHERENT
/* When coherent_acquire() is called, we are sure not to have cache for this memory */
__must_check static inline struct coherent *coherent_acquire(struct coherent *c,
							     const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);

	/* access the shared coherent object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		c->key = k_spin_lock(&c->lock);

		/* invalidate local copy */
		dcache_invalidate_region(uncache_to_cache(c), size);
	}

	/* client can now use cached object safely */
	return uncache_to_cache(c);
}

static inline struct coherent *coherent_release(struct coherent *c, const size_t size)
{
	/* assert if someone passes a coherent address in here. */
	ADDR_IS_INCOHERENT(c);

	/* access the local copy of object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(c, size);

		/* unlock on uncache alias */
		k_spin_unlock(&cache_to_uncache(c)->lock, cache_to_uncache(c)->key);
	}

	return cache_to_uncache(c);
}

#define coherent_init(object, member)						\
	do {									\
		/* assert if someone passes a cache/local address in here. */	\
		ADDR_IS_COHERENT(object);					\
		/* TODO static assert if we are not cache aligned */		\
		k_spinlock_init(&object->member.lock);				\
		object->member.shared = false;					\
		object->member.core = cpu_get_id();				\
		list_init(&object->member.list);				\
		/* inv local data to coherent object */				\
		dcache_invalidate_region(uncache_to_cache(object), sizeof(*object)); \
	} while (0)

#define coherent_free(object, member)						\
	do {									\
		/* assert if someone passes a cache address in here. */		\
		ADDR_IS_COHERENT(object);					\
		/* wtb and inv local data to coherent object */			\
		dcache_writeback_invalidate_region(uncache_to_cache(object), sizeof(*object)); \
	} while (0)

/* set the object to shared mode with coherency managed by SW */
#define coherent_shared(object, member)						\
	do {									\
		/* assert if someone passes a cache/local address in here. */	\
		ADDR_IS_COHERENT(object);					\
		(object)->member.key = k_spin_lock(&(object)->member.lock);	\
		(object)->member.shared = true;					\
		dcache_invalidate_region(object, sizeof(*object));		\
		k_spin_unlock(&(object)->member.lock, (object)->member.key);	\
	} while (0)
#else

/*
 * Coherent devices only require locking to manage shared access.
 */
__must_check static inline struct coherent *coherent_acquire(struct coherent *c, const size_t size)
{
	if (c->shared) {
		c->key = k_spin_lock(&c->lock);

		/* invalidate local copy */
		dcache_invalidate_region(uncache_to_cache(c), size);
	}

	return c;
}

static inline struct coherent *coherent_release(struct coherent *c, const size_t size)
{
	if (c->shared) {
		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(uncache_to_cache(c), size);

		k_spin_unlock(&c->lock, c->key);
	}

	return c;
}

#define coherent_init(object, member)						\
	do {									\
		/* TODO static assert if we are not cache aligned */		\
		k_spinlock_init(&object->member.lock);				\
		object->member.shared = 0;					\
		object->member.core = cpu_get_id();				\
		list_init(&object->member.list);				\
	} while (0)

#define coherent_free(object, member)

/* no function on cache coherent architectures */
#define coherent_shared(object, member) ((object)->member.shared = true)

#endif /* CONFIG_CAVS && CONFIG_CORE_COUNT > 1 */

#define is_coherent_shared(object, member) ((object)->member.shared)
#endif
