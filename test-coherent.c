/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

//#define __coherent __attribute__((packed, aligned(64)))

#define __coherent
#define __must_check

#define container_of(ptr, type, member) \
	({const typeof(((type *)0)->member)*__memberptr = (ptr); \
	(type *)((char *)__memberptr - offsetof(type, member)); })

struct list_item;

struct list_item {
	struct list_item *next;
	struct list_item *prev;
};

/* initialise list before any use - list will point to itself */
static inline void list_init(struct list_item *list)
{
	list->next = list;
	list->prev = list;
}

static inline void list_item_append(struct list_item *item,
	struct list_item *list)
{
	struct list_item *tail = list->prev;

	tail->next = item;
	item->next = list;
	item->prev = tail;
	list->prev = item;
}

#define list_is_empty(item) \
	((item)->next == item)

#define cpu_get_id() 0

#define spin_lock(l) (*l) = 1
#define spin_unlock(l) (*l) = 0
#define spinlock_init(l) (*l) = 0

#define spin_lock_irq(l, f)
#define spin_unlock_irq(l, f)

#define dcache_invalidate_region(c, size)
#define dcache_writeback_invalidate_region(c, size)

#define uncache_to_cache(c) c
#define cache_to_uncache(c) c

/*
 * This structue needs to be embedded at the start of any container to ensure
 * container object cache alignment and to minimise non cache access when
 * acquiring ownership.
 *
 * This structure should not be accessed outside of these APIs.
 * The shared flag is only set at coherent init and thereafter it's RO.
 */
struct coherent {
	int lock;	/* locking mechanism */
	uint32_t flags;		/* lock flags */
	uint16_t shared;	/* shared on other non coherent cores */
	uint16_t core;		/* owner core if not shared */
	struct list_item list;	/* coherent list iteration */
}__coherent;

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

#if CONFIG_CAVS && CONFIG_CORE_COUNT > 1
__must_check static inline struct coherent *coherent_acquire(
	struct coherent *c, const size_t size)
{
	/* assert is someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);

	/* this flavour should not be used in IRQ context */
	CHECK_COHERENT_IRQ(c);

	/* access the shared coherent object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		spin_lock(&c->lock);

		/* invalidate local copy */
		dcache_invalidate_region(uncache_to_cache(c), size);
	}

	/* client can now use cached object safely */
	return uncache_to_cache(c);
}

static inline struct coherent *coherent_release(struct coherent *c,
	const size_t size)
{
	/* assert is someone passes a coherent address in here. */
	ADDR_IS_INCOHERENT(c);

	/* this flavour should not be used in IRQ context */
	CHECK_COHERENT_IRQ(c);

	/* access the local copy of object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(c, size);

		/* unlock on uncache alias */
		spin_unlock(&(cache_to_uncache(c))->lock);
	}

	return cache_to_uncache(c);
}

__must_check static inline struct coherent *coherent_acquire_irq(
	struct coherent *c, const size_t size)
{
	/* assert is someone passes a cache/local address in here. */
	ADDR_IS_COHERENT(c);

	/* access the shared coherent object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		spin_lock_irq(&c->lock, c->flags);

		/* invalidate local copy */
		dcache_invalidate_region(uncache_to_cache(c), size);
	}

	/* client can now use cached object safely */
	return uncache_to_cache(c);
}

static inline struct coherent *coherent_release_irq(struct coherent *c,
	const size_t size)
{
	/* assert is someone passes a coherent address in here. */
	ADDR_IS_INCOHERENT(c);

	/* access the local copy of object */
	if (c->shared) {
		CHECK_COHERENT_CORE(c);

		/* wtb and inv local data to coherent object */
		dcache_writeback_invalidate_region(c, size);

		/* unlock on uncache alias */
		spin_unlock_irq(&(cache_to_uncache(c))->lock,
				(cache_to_uncache(c))->flags);
	}

	return cache_to_uncache(c);
}

#define coherent_init(object, member, is_shared)			\
	/* assert is someone passes a cache/local address in here. */	\
	ADDR_IS_COHERENT(object);					\
	/* TODO static assert if we are not cache aligned */		\
	spinlock_init(&object->member.lock);				\
	object->member.shared = is_shared;				\
	object->member.core = cpu_get_id();				\
	list_init(&object->member.list);				\
	/* wtb and inv local data to coherent object */			\
	dcache_writeback_invalidate_region(uncache_to_cache(object), sizeof(*object));

#else

/*
 * Coherent devices only require locking to manage shared access.
 */
__must_check static inline struct coherent *coherent_acquire(
	struct coherent *c, const size_t size)
{
	spin_lock(&c->lock);
	return c;
}

static inline struct coherent *coherent_release(struct coherent *c,
	const size_t size)
{
	spin_unlock(&c->lock);
	return c;
}

__must_check static inline struct coherent *coherent_acquire_irq(
	struct coherent *c, const size_t size)
{
	spin_lock_irq(&c->lock, &c->flags);
	return c;
}

static inline struct coherent *coherent_release_irq(struct coherent *c,
	const size_t size)
{
	spin_unlock_irq(&c->lock, c->flags);
	return c;
}

#define coherent_init(object, member, is_shared)			\
	/* TODO static assert if we are not cache aligned */		\
	spinlock_init(&object->member.lock);				\
	object->member.shared = is_shared;				\
	object->member.core = cpu_get_id();				\
	list_init(&object->member.list);

#endif

/*
 * List iteration is more complex with coherent objects as locks must be
 * obtained and objects invalidated and written back.
 */
/* check that next item is not head */
#define _coherent_next_not_head(_pos, _head)				\
	(_pos->next != _head)

/* acquire (hold lock and invalidate) next object in list */
#define _coherent_next_object(_item, _size)				\
	coherent_acquire(container_of((_item)->next, struct coherent, list), _size)

/* release (release lock and writeback) prev object in list */
#define _coherent_prev_object(_item, _size)				\
	coherent_release(container_of((_item)->prev, struct coherent, list), _size)

/* acquire (hold lock and invalidate) next object in list */
#define _coherent_next_object_irq(_item, _size)				\
	coherent_acquire_irq(container_of((_item)->next, struct coherent, list), _size)

/* release (release lock and writeback) prev object in list */
#define _coherent_prev_object_irq(_item, _size)				\
	coherent_release_irq(container_of((_item)->prev, struct coherent, list), _size)

/* get next list item (coherent) pointer */
#define _coherent_next_list_item(_item, _size)				\
	&(_coherent_next_object(_item, _size))->list

/* get next list item (coherent) pointer */
#define _coherent_next_list_item_irq(_item, _size)				\
	&(_coherent_next_object_irq(_item, _size))->list

/* acquire the next coherent object in list or list head if empty */
#define _coherent_iterate_init(_head, _pos, _size)			\
	_pos = list_is_empty(_head) ? 					\
		uncache_to_cache(_head) : _coherent_next_list_item(_head, _size)

/* acquire the next coherent object in list or list head if empty */
#define _coherent_iterate_init_irq(_head, _pos, _size)			\
	_pos = list_is_empty(_head) ? 					\
		uncache_to_cache(_head) : _coherent_next_list_item_irq(_pos, _size)

/* iterate if true */
#define _coherent_iterate_condition(_head, _pos)			\
	uncache_to_cache(_head) != _pos

/* acquire next coherent object, release previous object and set next position */
#define _coherent_iterate_next(_head, _pos, _size)			\
	(_pos = _coherent_next_not_head(_pos, _head) ?			\
		_coherent_next_list_item(_pos, _size) : uncache_to_cache(_head)), \
		_coherent_prev_object(_pos, _size)

/* acquire next coherent object, release previous object and set next position */
#define _coherent_iterate_next_irq(_head, _pos, _size)			\
	(_pos = _coherent_next_not_head(_pos, _head) ?			\
		_coherent_next_list_item_irq(_pos, _size) : uncache_to_cache(_head)), \
		_coherent_prev_object_irq(_pos, _size)

/* cleanup the objects and release */
#define list_coherent_stop(_pos, _size)				\
	coherent_release(container_of(_pos, struct coherent, list), _size)

/* cleanup the objects and release */
#define list_coherent_stop_irq(_pos, _size)				\
	coherent_release_irq(container_of(_pos, struct coherent, list), _size)

/*
 * Coherent list iterator.
 *
 * A little more complexity than usual as we need to take care of managing
 * both locking and cache coherency of each item in the list.
 *
 * Initialisation
 * --------------
 * 1) If list is empty then pos = head and goto 3.
 * 2) Acquire (head->next), pos = head->next.
 *
 * Iteration Condition
 * -------------------
 * 3) Stop iteration if pos == head (uncached addresses)
 *
 * Iterate Next
 * ------------
 * 4) Check if pos->next == head then 6) else 5)
 * 5) Acquire (pos->next), pos = pos->next, release (pos->prev), goto 3.
 * 6) pos = head, release (pos->prev) (now fails 3), goto 3.
 *
 * NOTE: users must call list_coherent_stop() before directly calling break or
 * return to ensure locking and cache coherecy is preserved.
 */
#define list_for_coherent_item(_pos, _head, _size)			\
	for (_coherent_iterate_init(_head, _pos, _size); 		\
	     _coherent_iterate_condition(_head, _pos);			\
		_coherent_iterate_next(_head, _pos, _size))

#define list_for_coherent_item_irq(_head, _pos, _size)			\
	for (_coherent_iterate_init(_head, _pos, _size); 		\
	     _coherent_iterate_condition(_head, _pos);			\
		_coherent_iterate_next(_head, _pos, _size))


struct test_object {
	struct coherent c;
	int a;
};

#define COUNT 10

int main(int argc, char *argv[])
{
	struct test_object *o[COUNT];
	struct list_item head;
	struct test_object *t;
	struct list_item *clist = NULL;
	int i;
	
	list_init(&head);
	
	for (i = 0; i < COUNT; i++) {
		o[i] = calloc(COUNT, sizeof(struct test_object));
		coherent_init(o[i], c, 1);
		list_item_append(&o[i]->c.list, &head);
		o[i]->a = i;
	}
	
	list_for_coherent_item(clist, &head, sizeof(struct test_object)) {
		t = container_of(clist, struct test_object, c.list);
		printf("t = %d\n", t->a);
		if (t->a == 7) {
			list_coherent_stop(clist, sizeof(struct test_object));
			break;
		}
	}
	
	for (i = 0; i < COUNT; i++) {
		printf("t = %d lock %d\n", o[i]->a, o[i]->c.lock);
		free(o[i]);
	}
	
	return 0;
}

