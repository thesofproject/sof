/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 * Authors:	Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 */

#ifndef __INCLUDE_LIST__
#define __INCLUDE_LIST__

/* Really simple list manipulation - syntax same as Linux and ALSA lists */

struct list_head;

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

static inline void list_init(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void list_add(struct list_head *new, struct list_head *prev)
{
	struct list_head *next = prev->next;

	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_del(struct list_head *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

#define list_entry_offset(pos, type, offset) \
	((type *)((char *)(pos) - (offset)))

#define list_entry(pos, type, member) \
	list_entry_offset(pos, type, offsetof(type, member))

#define list_for_each(pos, list) \
	for (pos = (list)->next; pos != (list); pos = pos->next)

#define list_for_each_safe(pos, n, list) \
	for (pos = (list)->next, n = pos->next; pos != (list); \
		pos = n, n = pos->next)

#define list_empty(list) \
       ((list)->next == list)

#endif
