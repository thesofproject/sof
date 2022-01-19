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
 * This structure needs to be embedded at the start of any container to ensure
 * container object cache alignment and to minimise non cache access when
 * acquiring ownership.
 *
 * This structure should not be accessed outside of these APIs.
 * The shared flag is only set at coherent init and thereafter it's RO.
 */
struct coherent {
	spinlock_t lock;	/* locking mechanism */
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

/* debug usage in IRQ contexts - check non IRQ being used in IRQ context TODO */
#ifdef COHERENT_CHECK_IN_IRQ
#define CHECK_COHERENT_IRQ(_c) assert(1)
#else
#define CHECK_COHERENT_IRQ(_c)
#endif

/*
 * Incoherent devices require manual cache invalidation and writeback as
 * well as locking to manage shared access.
 */

#if CONFIG_INCOHERENT
__must_check static inline struct coherent *coherent_acquire(struct coherent *c,
							     const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);

	/* this flavour should not be used in IRQ context */
	CHECK_COHERENT_IRQ(c);

	/* access the shared coherent object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		k_spin_lock(&c->lock);

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

	/* this flavour should not be used in IRQ context */
	CHECK_COHERENT_IRQ(c);

	/* access the local copy of object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(c, size);

		/* unlock on uncache alias */
		k_spin_unlock(&(cache_to_uncache(c))->lock);
	}

	return cache_to_uncache(c);
}

__must_check static inline struct coherent *coherent_acquire_irq(struct coherent *c,
								 const size_t size)
{
	/* assert if someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);

	/* access the shared coherent object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		k_spin_lock_irq(&c->lock, c->flags);

		/* invalidate local copy */
		dcache_invalidate_region(uncache_to_cache(c), size);
	}

	/* client can now use cached object safely */
	return uncache_to_cache(c);
}

static inline struct coherent *coherent_release_irq(struct coherent *c, const size_t size)
{
	/* assert if someone passes a coherent address in here. */
	ADDR_IS_INCOHERENT(c);

	/* access the local copy of object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(c, size);

		/* unlock on uncache alias */
		k_spin_unlock_irq(&(cache_to_uncache(c))->lock,
				  (cache_to_uncache(c))->flags);
	}

	return cache_to_uncache(c);
}

#define coherent_init(object, member)			\
	do {	\
		/* assert if someone passes a cache/local address in here. */	\
		ADDR_IS_COHERENT(object);					\
		/* TODO static assert if we are not cache aligned */		\
		spinlock_init(&object->member.lock);				\
		object->member.shared = false;				\
		object->member.core = cpu_get_id();				\
		list_init(&object->member.list);				\
		/* wtb and inv local data to coherent object */			\
		dcache_writeback_invalidate_region(uncache_to_cache(object), sizeof(*object)); \
	} while (0)

#define coherent_free(object, member)			\
	do { \
		/* assert if someone passes a cache address in here. */	\
		ADDR_IS_COHERENT(object);					\
		/* wtb and inv local data to coherent object */			\
		dcache_writeback_invalidate_region(uncache_to_cache(object), sizeof(*object)); \
	} while (0)

/* set the object to shared mode with coherency managed by SW */
#define coherent_shared(object, member)					\
	do { \
		/* assert if someone passes a cache/local address in here. */	\
		ADDR_IS_COHERENT(object);					\
		k_spin_lock(&(object)->member.lock);				\
		(object)->member.shared = true;					\
		dcache_writeback_invalidate_region(object, sizeof(*object));	\
		k_spin_unlock(&(object)->member.lock); \
	} while (0)

/* set the object to shared mode with coherency managed by SW */
#define coherent_shared_irq(object, member)				\
	do { \
		/* assert if someone passes a cache/local address in here. */	\
		ADDR_IS_COHERENT(object);					\
		k_spin_lock_irq(&(object)->member.lock, &(object)->member.flags);	\
		(object)->member.shared = true;					\
		dcache_writeback_invalidate_region(object, sizeof(*object));	\
		k_spin_unlock_irq(&(object)->member.lock, &(object)->member.flags); \
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

__must_check static inline struct coherent *coherent_acquire_irq(struct coherent *c,
								 const size_t size)
{
	if (c->shared) {
		c->key = k_spin_lock(&c->lock);

		/* invalidate local copy */
		dcache_invalidate_region(uncache_to_cache(c), size);
	}

	return c;
}

static inline struct coherent *coherent_release_irq(struct coherent *c, const size_t size)
{
	if (c->shared) {
		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(uncache_to_cache(c), size);

		k_spin_unlock(&c->lock, c->key);
	}

	return c;
}

#define coherent_init(object, member)					\
	do { \
		/* TODO static assert if we are not cache aligned */		\
		spinlock_init(&object->member.lock);				\
		object->member.shared = 0;					\
		object->member.core = cpu_get_id();				\
		list_init(&object->member.list);				\
	} while (0)

#define coherent_free(object, member)

/* no function on cache coherent architectures */
#define coherent_shared(object, member) ((object)->member.shared = true)
#define coherent_shared_irq(object, member) ((object)->member.shared = true)

#endif /* CONFIG_CAVS && CONFIG_CORE_COUNT > 1 */

#define is_coherent_shared(object, member) ((object)->member.shared)
#endif
