/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_LIST_H__
#define __SOF_LIST_H__

/* Really simple list manipulation */

struct list_item;

// HACK
#define LIST_ITEM_SIZE	64

struct list_item {
	struct list_item *next;
	struct list_item *prev;
	unsigned int core[4];
	char unused[LIST_ITEM_SIZE - (sizeof(struct list_item *) * 2) - sizeof(unsigned int) * 4];
} __attribute__ ((aligned (LIST_ITEM_SIZE)));

// HACK - only defined in some places when using this file for zephyr !
#ifdef BUILD_ASSERT
BUILD_ASSERT(sizeof(struct list_item)  == LIST_ITEM_SIZE, "LIST_ITEM_SIZE too small");
#endif

// make sure we dont enable this for sof-logger build
#ifdef __ZEPHYR__
#define DEBUG_CACHE_LIST
#endif

#ifdef DEBUG_CACHE_LIST

void printk(const char *fmt, ...);

static inline int __arch_cpu_get_id(void)
{
	int prid;

	__asm__("rsr.prid %0" : "=a"(prid));

	return prid;
}


static inline void ___arch_xtensa_is_ptr_cached(void *ptr, const char *func,
		const char *use, int line)
{
	if (((unsigned long)ptr >> 29) == 5) {
		printk("%s:%d: using cached ptr for %s\n", func, line, use);
	}

}
#endif

/* a static list head initialiser */
#define LIST_INIT(head) {&head, &head}

#ifdef DEBUG_CACHE_LIST

static inline void __list_init(struct list_item *list)
{
	list->next = list;
	list->prev = list;
	list->core[0] = 0;
	list->core[1] = 0;
	list->core[2] = 0;
	list->core[3] = 0;
}

#define list_init(dlist) \
	do { \
	___arch_xtensa_is_ptr_cached((dlist), __func__, "list_init", __LINE__); \
	__list_init((dlist)); \
	} while (0)

#else
/* initialise list before any use - list will point to itself */
static inline void list_init(struct list_item *list)
{
	list->next = list;
	list->prev = list;
}
#endif

#ifdef DEBUG_CACHE_LIST

static inline void __list_item_prepend(struct list_item *item,
	struct list_item *list)
{
	struct list_item *next = list->next;

	next->prev = item;
	item->next = next;
	item->prev = list;
	list->next = item;
}

#define list_item_prepend(ditem, dlist) \
	do { \
	___arch_xtensa_is_ptr_cached((dlist), __func__, "list_prepend list", __LINE__); \
	___arch_xtensa_is_ptr_cached((ditem), __func__, "list_prepend item", __LINE__); \
	__list_item_prepend((ditem), (dlist)); \
	} while (0)

#else

/* add new item to the start or head of the list */
static inline void list_item_prepend(struct list_item *item,
	struct list_item *list)
{
	struct list_item *next = list->next;

	next->prev = item;
	item->next = next;
	item->prev = list;
	list->next = item;
}
#endif


#ifdef DEBUG_CACHE_LIST

static inline void __list_item_append(struct list_item *item,
	struct list_item *list)
{
	struct list_item *tail = list->prev;

	tail->next = item;
	item->next = list;
	item->prev = tail;
	list->prev = item;
}

#define list_item_append(ditem, dlist) \
	do { \
	___arch_xtensa_is_ptr_cached((ditem), __func__, "list_append item", __LINE__); \
	___arch_xtensa_is_ptr_cached((dlist), __func__, "list_append list", __LINE__); \
	__list_item_append((ditem), (dlist)); \
	} while (0)

#else
/* add new item to the end or tail of the list */
static inline void list_item_append(struct list_item *item,
	struct list_item *list)
{
	struct list_item *tail = list->prev;

	tail->next = item;
	item->next = list;
	item->prev = tail;
	list->prev = item;
}
#endif


#ifdef DEBUG_CACHE_LIST

static inline void __list_item_del(struct list_item *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
	list_init(item);
}

#define list_item_del(ditem) \
	do { \
	___arch_xtensa_is_ptr_cached((ditem), __func__, "list_item_del", __LINE__); \
	__list_item_del((ditem)); \
	} while (0)

#else
/* delete item from the list leaves deleted list item
 *in undefined state list_is_empty will return true
 */
static inline void list_item_del(struct list_item *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
	list_init(item);
}
#endif

/* is list item the last item in list ? */
static inline int list_item_is_last(struct list_item *item,
				struct list_item *list)
{
	return item->next == list;
}

/* is list empty ? */
#define list_is_empty(item) \
	((item)->next == item)

#ifdef DEBUG_CACHE_LIST
#define __list_object(item, type, offset, ffunc, lline) \
	({ int cpu = __arch_cpu_get_id(); \
	(item)->core[cpu]++; \
	if (cpu != 0 && (item)->core[0]) {printk("%s:%d: shared with core 0 and %d\n", ffunc, lline, cpu);} \
	if (cpu != 1 && (item)->core[1]) {printk("%s:%d: shared with core 1 and %d\n", ffunc, lline, cpu);} \
	if (cpu != 2 && (item)->core[2]) {printk("%s:%d: shared with core 2 and %d\n", ffunc, lline, cpu);} \
	if (cpu != 3 && (item)->core[3]) {printk("%s:%d: shared with core 3 and %d\n", ffunc, lline, cpu);} \
	((type *)((char *)(item) - (offset))); })
#else
#define __list_object(item, type, offset) \
	((type *)((char *)(item) - (offset)))
#endif

/* get the container object of the list item */
#define list_item(item, type, member) \
	__list_object(item, type, offsetof(type, member), __func__, __LINE__)

/* get the container object of the first item in the list */
#define list_first_item(list, type, member) \
	__list_object((list)->next, type, offsetof(type, member), __func__, __LINE__)

/* get the next container object in the list */
#define list_next_item(object, member) \
	list_item((object)->member.next, __typeof__(*(object)), member)

/* list iterator */
#define list_for_item(item, list) \
	for (item = (list)->next; item != (list); item = item->next)

/* list iterator */
#define list_for_item_prev(item, list) \
	for (item = (list)->prev; item != (list); item = item->prev)

/* list iterator - safe to delete items */
#define list_for_item_safe(item, tmp, list) \
	for (item = (list)->next, tmp = item->next;\
		item != (list); \
		item = tmp, tmp = item->next)

/**
 * Re-links the list when head address changed (list moved).
 * @param new_list New address of the head.
 * @param old_list Old address of the head.
 */
static inline void list_relink(struct list_item *new_list,
			       struct list_item *old_list)
{
	struct list_item *li;

	if (new_list->next == old_list) {
		list_init(new_list);
	} else {
		list_for_item(li, new_list)
			if (li->next == old_list)
				li->next = new_list; /* for stops here */
		list_for_item_prev(li, new_list)
			if (li->prev == old_list)
				li->prev = new_list; /* for stops here */
	}
}

#endif /* __SOF_LIST_H__ */
