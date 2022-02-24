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
	union {
		struct {
			struct k_spinlock lock;	/* locking mechanism */
			k_spinlock_key_t key;	/* lock flags */
		};
#ifdef __ZEPHYR__
		struct k_mutex mutex;
#endif
	};
	uint8_t sleep_allowed;	/* the object will never be acquired or released
				 * in atomic context */
	uint8_t shared;		/* shared on other non coherent cores */
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

#ifdef __ZEPHYR__
#define CHECK_ISR()		__ASSERT(!arch_is_in_isr(), "Attempt to sleep in ISR!")
#define CHECK_SLEEP(_c)		__ASSERT((_c)->sleep_allowed, \
				"This context hasn't been initialized for sleeping!")
#define CHECK_ATOMIC(_c)	__ASSERT(!(_c)->sleep_allowed, \
				"This context has been initialized for sleeping!")
#else
#define CHECK_ISR()		assert(!arch_is_in_isr())
#define CHECK_SLEEP(_c)		assert((_c)->sleep_allowed)
#define CHECK_ATOMIC(_c)	assert(!(_c)->sleep_allowed)
#endif

#if CONFIG_INCOHERENT
/* When coherent_acquire() is called, we are sure not to have cache for this memory */
__must_check static inline struct coherent *coherent_acquire(struct coherent *c,
							     const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);
	CHECK_ATOMIC(c);

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
	CHECK_ATOMIC(c);

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

static inline void __coherent_init(struct coherent *c, const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);

	/* TODO static assert if we are not cache aligned */
	k_spinlock_init(&c->lock);
	c->sleep_allowed = false;
	c->shared = false;
	c->core = cpu_get_id();
	list_init(&c->list);
	/* inv local data to coherent object */
	dcache_invalidate_region(uncache_to_cache(c), size);
}

#define coherent_init(object, member) __coherent_init(&(object)->member, \
						      sizeof(*object))

/* set the object to shared mode with coherency managed by SW */
static inline void __coherent_shared(struct coherent *c, const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);
	CHECK_ATOMIC(c);

	c->key = k_spin_lock(&c->lock);
	c->shared = true;
	dcache_invalidate_region(c, size);
	k_spin_unlock(&c->lock, c->key);
}

#define coherent_shared(object, member) __coherent_shared(&(object)->member, \
							  sizeof(*object))

#ifdef __ZEPHYR__

__must_check static inline struct coherent *coherent_acquire_thread(struct coherent *c,
								    const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);
	CHECK_SLEEP(c);
	CHECK_ISR();

	/* access the shared coherent object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		k_mutex_lock(&c->mutex, K_FOREVER);

		/* invalidate local copy */
		dcache_invalidate_region(uncache_to_cache(c), size);
	}

	/* client can now use cached object safely */
	return uncache_to_cache(c);
}

static inline struct coherent *coherent_release_thread(struct coherent *c, const size_t size)
{
	/* assert if someone passes a coherent address in here. */
	ADDR_IS_INCOHERENT(c);
	CHECK_SLEEP(c);
	CHECK_ISR();

	/* access the local copy of object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(c, size);

		/* unlock on uncache alias */
		k_mutex_unlock(&cache_to_uncache(c)->mutex);
	}

	return cache_to_uncache(c);
}

static inline void __coherent_init_thread(struct coherent *c, const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);

	/* TODO static assert if we are not cache aligned */
	k_mutex_init(&c->mutex);
	c->sleep_allowed = true;
	c->shared = false;
	c->core = cpu_get_id();
	list_init(&c->list);
	/* inv local data to coherent object */
	dcache_invalidate_region(uncache_to_cache(c), size);
}

#define coherent_init_thread(object, member) __coherent_init_thread(&(object)->member, \
								    sizeof(*object))

static inline void __coherent_shared_thread(struct coherent *c, const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);
	CHECK_SLEEP(c);
	CHECK_ISR();

	k_mutex_lock(&c->mutex, K_FOREVER);
	c->shared = true;
	dcache_invalidate_region(c, size);
	k_mutex_unlock(&c->mutex);
}

#define coherent_shared_thread(object, member) __coherent_shared_thread(&(object)->member, \
									sizeof(*object))

#endif /* __ZEPHYR__ */

#define coherent_free(object, member)						\
	do {									\
		/* assert if someone passes a cache address in here. */		\
		ADDR_IS_COHERENT(object);					\
		/* wtb and inv local data to coherent object */			\
		dcache_writeback_invalidate_region(uncache_to_cache(object), sizeof(*object)); \
	} while (0)

#else /* CONFIG_INCOHERENT */

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

static inline void __coherent_init(struct coherent *c, const size_t size)
{
	/* TODO static assert if we are not cache aligned */
	k_spinlock_init(&c->lock);
	c->shared = 0;
	c->core = cpu_get_id();
	list_init(&c->list);
}

#define coherent_init(object, member) __coherent_init(&(object)->member, \
						      sizeof(*object))

#define coherent_free(object, member) do {} while (0)

/* no function on cache coherent architectures */
#define coherent_shared(object, member) (object)->member.shared = true

#ifdef __ZEPHYR__
__must_check static inline struct coherent *coherent_acquire_thread(struct coherent *c,
								    const size_t size)
{
	if (c->shared) {
		k_mutex_lock(&c->mutex, K_FOREVER);

		/* invalidate local copy */
		dcache_invalidate_region(uncache_to_cache(c), size);
	}

	return c;
}

static inline struct coherent *coherent_release_thread(struct coherent *c, const size_t size)
{
	if (c->shared) {
		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(uncache_to_cache(c), size);

		k_mutex_unlock(&c->mutex);
	}

	return c;
}

static inline void __coherent_init_thread(struct coherent *c, const size_t size)
{
	/* TODO static assert if we are not cache aligned */
	k_mutex_init(&c->mutex);
	c->shared = 0;
	c->core = cpu_get_id();
	list_init(&c->list);
}

#define coherent_init_thread(object, member) __coherent_init_thread(&(object)->member, \
								    sizeof(*object))
#endif /* __ZEPHYR__ */

#endif /* CONFIG_INCOHERENT */

#ifndef __ZEPHYR__
#define coherent_acquire_thread coherent_acquire
#define coherent_release_thread coherent_release
#define coherent_init_thread coherent_init
#define coherent_shared_thread coherent_shared
#endif

#define coherent_free_thread coherent_free

#define is_coherent_shared(object, member) ((object)->member.shared)
#endif
