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

struct list_item {
	struct list_item *next;
	struct list_item *prev;
};

/* a static list head initialiser */
#define LIST_INIT(head) {&head, &head}

/* initialise list before any use - list will point to itself */
static inline void list_init(struct list_item *list)
{
	list->next = list;
	list->prev = list;
}

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

/* delete item from the list leaves deleted list item
 *in undefined state list_is_empty will return true
 */
static inline void list_item_del(struct list_item *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
	list_init(item);
}

/* is list item the last item in list ? */
static inline int list_item_is_last(struct list_item *item,
				struct list_item *list)
{
	return item->next == list;
}

/* is list empty ? */
#define list_is_empty(item) \
	((item)->next == item)

#define __list_object(item, type, offset) \
	((type *)((char *)(item) - (offset)))

/* get the container object of the list item */
#define list_item(item, type, member) \
	__list_object(item, type, offsetof(type, member))

/* get the container object of the first item in the list */
#define list_first_item(list, type, member) \
	__list_object((list)->next, type, offsetof(type, member))

/* get the next container object in the list */
#define list_next_item(object, member) \
	list_item((object)->member.next, typeof(*(object)), member)

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
