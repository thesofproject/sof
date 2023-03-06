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

#include <../include/list.h>

#define __coherent __attribute__((packed, aligned(64)))

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
struct k_spinlock {
};

typedef uint32_t k_spinlock_key_t;

struct coherent {
	union {
		struct {
			struct k_spinlock lock;	/* locking mechanism */
			k_spinlock_key_t key;	/* lock flags */
		};
//#ifdef __ZEPHYR__
//		struct k_mutex mutex;
//#endif
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
#define CHECK_ISR()		__ASSERT(!k_is_in_isr(), "Attempt to sleep in ISR!")
#define CHECK_SLEEP(_c)		__ASSERT((_c)->sleep_allowed, \
				"This context hasn't been initialized for sleeping!")
#define CHECK_ATOMIC(_c)	__ASSERT(!(_c)->sleep_allowed, \
				"This context has been initialized for sleeping!")
#else
#define CHECK_ISR()		assert(!k_is_in_isr())
#define CHECK_SLEEP(_c)		assert((_c)->sleep_allowed)
#define CHECK_ATOMIC(_c)	assert(!(_c)->sleep_allowed)
#endif

#if CONFIG_INCOHERENT
/* When coherent_acquire() is called, we are sure not to have cache for this memory */
__must_check static inline struct coherent __sparse_cache *coherent_acquire(struct coherent *c,
								     const size_t size)
{
	struct coherent __sparse_cache *cc = uncache_to_cache(c);

	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);
	CHECK_ATOMIC(c);

	/* access the shared coherent object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		c->key = k_spin_lock(&c->lock);

		/*
		 * FIXME: This is wrong. dcache_invalidate_region() only makes
		 * sense if we assume, that dirty cache lines might exist for
		 * this object. But in that case those lines could be written
		 * back here thus overwriting either user data, or the coherent
		 * header or both. When coherent_acquire() is called it must be
		 * guaranteed that the object isn't in cache. Before it is
		 * acquired no cached access to it is allowed. This has to be
		 * fixed here and on multiple further occasions below.
		 */

		/* invalidate local copy */
		dcache_invalidate_region(cc, size);
	}

	/* client can now use cached object safely */
	return cc;
}

static inline void coherent_release(struct coherent __sparse_cache *c,
				    const size_t size)
{
	struct coherent *uc = cache_to_uncache(c);

	/* assert if someone passes a coherent address in here. */
	ADDR_IS_INCOHERENT(c);
	CHECK_ATOMIC(c);

	/* access the local copy of object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(c, size);

		/* unlock on uncache alias */
		k_spin_unlock(&uc->lock, uc->key);
	}
}

static inline void *__coherent_init(size_t offset, const size_t size)
{
	void *object = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, size);
	struct coherent *c;

	if (!object)
		return NULL;

	c = (struct coherent *)((uint8_t *)object + offset);

	/* TODO static assert if we are not cache aligned */
	k_spinlock_init(&c->lock);
	c->sleep_allowed = false;
	c->shared = false;
	c->core = cpu_get_id();
	list_init(&c->list);
	/* inv local data to coherent object */
	dcache_invalidate_region(uncache_to_cache(c), size);

	return object;
}

#define coherent_init(type, member) __coherent_init(offsetof(type, member), \
						    sizeof(type))

/* set the object to shared mode with coherency managed by SW */
static inline void __coherent_shared(struct coherent *c, const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);
	CHECK_ATOMIC(c);

	c->key = k_spin_lock(&c->lock);
	c->shared = true;
	dcache_invalidate_region(uncache_to_cache(c), size);
	k_spin_unlock(&c->lock, c->key);
}

#define coherent_shared(object, member) __coherent_shared(&(object)->member, \
							  sizeof(*object))

#ifdef __ZEPHYR__

__must_check static inline struct coherent __sparse_cache *coherent_acquire_thread(
	struct coherent *c, const size_t size)
{
	struct coherent __sparse_cache *cc = uncache_to_cache(c);

	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);
	CHECK_SLEEP(c);
	CHECK_ISR();

	/* access the shared coherent object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		k_mutex_lock(&c->mutex, K_FOREVER);

		/* invalidate local copy */
		dcache_invalidate_region(cc, size);
	}

	/* client can now use cached object safely */
	return cc;
}

static inline void coherent_release_thread(struct coherent __sparse_cache *c,
					   const size_t size)
{
	struct coherent *uc = cache_to_uncache(c);

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
		k_mutex_unlock(&uc->mutex);
	}
}

static inline void *__coherent_init_thread(size_t offset, const size_t size)
{
	void *object = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, size);
	struct coherent *c;

	if (!object)
		return NULL;

	c = (struct coherent *)((uint8_t *)object + offset);

	/* TODO static assert if we are not cache aligned */
	k_mutex_init(&c->mutex);
	c->sleep_allowed = true;
	c->shared = false;
	c->core = cpu_get_id();
	list_init(&c->list);
	/* inv local data to coherent object */
	dcache_invalidate_region(uncache_to_cache(c), size);

	return object;
}

#define coherent_init_thread(type, member) __coherent_init_thread(offsetof(type, member), \
								  sizeof(type))

static inline void __coherent_shared_thread(struct coherent *c, const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);
	CHECK_SLEEP(c);
	CHECK_ISR();

	k_mutex_lock(&c->mutex, K_FOREVER);
	c->shared = true;
	dcache_invalidate_region(uncache_to_cache(c), size);
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
		dcache_writeback_invalidate_region(uncache_to_cache(object),	\
						   sizeof(*object));		\
		rfree(object);							\
	} while (0)

#else /* CONFIG_INCOHERENT */

/*
 * Coherent devices only require locking to manage shared access.
 */
static inline struct coherent *coherent_acquire(struct coherent *c,
								     const size_t size)
{
	if (c->shared) {
		struct coherent *cc = uncache_to_cache(c);

		c->key = k_spin_lock(&c->lock);

		/* invalidate local copy */
		dcache_invalidate_region(cc, size);
	}

	return (struct coherent *)c;
}

static inline void coherent_release(struct coherent *c,
				    const size_t size)
{
	if (c->shared) {
		struct coherent *uc = cache_to_uncache(c);

		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(c, size);

		k_spin_unlock(&uc->lock, uc->key);
	}
}

static inline void *__coherent_init(size_t offset, const size_t size)
{
//	void *object = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, size);
//	struct coherent *c;
//
//	if (!object)
//		return NULL;
//
//	c = (struct coherent *)((uint8_t *)object + offset);
//
//	/* TODO static assert if we are not cache aligned */
//	k_spinlock_init(&c->lock);
//	c->shared = 0;
//	c->core = cpu_get_id();
//	list_init(&c->list);
//
//	return object;
	return 0;
}

#define coherent_init(type, member) __coherent_init(offsetof(type, member), \
						    sizeof(type))

#define coherent_free(object, member) rfree(object)

static inline void __coherent_shared(struct coherent *c, const size_t size)
{
	c->key = k_spin_lock(&c->lock);
	c->shared = true;
	k_spin_unlock(&c->lock, c->key);
}

#define coherent_shared(object, member) __coherent_shared(&(object)->member, \
							  sizeof(*object))

#ifdef __ZEPHYR__
static inline struct coherent *coherent_acquire_thread(
	struct coherent *c, const size_t size)
{
//	if (c->shared) {
//		struct coherent *cc = uncache_to_cache(c);
//
//		k_mutex_lock(&c->mutex, K_FOREVER);
//
//		/* invalidate local copy */
//		dcache_invalidate_region(cc, size);
//	}
//
//	return (struct coherent *)c;
return 0;
}

static inline void coherent_release_thread(struct coherent *c,
					   const size_t size)
{
//	if (c->shared) {
//		struct coherent *uc = cache_to_uncache(c);
//
//		/* wtb and inv local data to coherent object */
//		dcache_writeback_invalidate_region(c, size);
//
//		k_mutex_unlock(&uc->mutex);
//	}
}

static inline void *__coherent_init_thread(size_t offset, const size_t size)
{
	/*
	 * Allocate an object with an uncached alias but align size on a cache-
	 * line boundary to avoid sharing a cache line with the adjacent
	 * allocation
	 */
//	void *object = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
//			       ALIGN_UP(size, PLATFORM_DCACHE_ALIGN));
//	struct coherent *c;
//
//	if (!object)
//		return NULL;
//
//	c = (struct coherent *)((uint8_t *)object + offset);
//
//	/* TODO static assert if we are not cache aligned */
//	k_mutex_init(&c->mutex);
//	c->shared = 0;
//	c->core = cpu_get_id();
//	list_init(&c->list);
//
//	return object;
return 0;
}

#define coherent_init_thread(type, member) __coherent_init_thread(offsetof(type, member), \
								  sizeof(type))

static inline void __coherent_shared_thread(struct coherent *c, const size_t size)
{
//	k_mutex_lock(&c->mutex, K_FOREVER);
//	c->shared = true;
//	k_mutex_unlock(&c->mutex);
}

#define coherent_shared_thread(object, member) __coherent_shared_thread(&(object)->member, \
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
