/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_LIST__
#define __INCLUDE_LIST__

/* Really simple list manipulation */

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

/* delete item from the list */
static inline void list_item_del(struct list_item *item)
{
	item->next->prev = item->prev;
	item->prev->next = item->next;
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

#endif
