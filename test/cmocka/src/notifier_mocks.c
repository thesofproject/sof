// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>

#include <stdint.h>
#include <stdlib.h>
#include <rtos/alloc.h>
#include <sof/lib/notifier.h>
#include <sof/list.h>
#include <sof/common.h>

struct callback_handle {
	void *receiver;
	const void *caller;
	void (*cb)(void *arg, enum notify_id, void *data);
	struct list_item list;
};

static struct notify *_notify;

struct notify **arch_notify_get(void)
{
	int i;

	if (!_notify) {
		_notify = malloc(sizeof(*_notify));

		for (i = 0; i < NOTIFIER_ID_COUNT; i++)
			list_init(&_notify->list[i]);
	}

	return &_notify;
}

void notifier_event(const void *caller, enum notify_id type, uint32_t core_mask,
		    void *data, uint32_t data_size)
{
	struct notify *notify = *arch_notify_get();
	struct list_item *wlist;
	struct list_item *tlist;
	struct callback_handle *handle;

	list_for_item_safe(wlist, tlist, &notify->list[type]) {
		handle = container_of(wlist, struct callback_handle, list);
		if (!caller || !handle->caller || handle->caller == caller)
			handle->cb(handle->receiver, type, data);
	}
}

int notifier_register(void *receiver, void *caller, enum notify_id type,
	void (*cb)(void *arg, enum notify_id type, void *data), uint32_t flags)
{
	struct notify *notify = *arch_notify_get();
	struct callback_handle *handle;

	if (type >= NOTIFIER_ID_COUNT)
		return -EINVAL;

	handle = rzalloc(0, 0, 0, sizeof(struct callback_handle));

	if (!handle)
		return -ENOMEM;

	list_init(&handle->list);
	handle->receiver = receiver;
	handle->caller = caller;
	handle->cb = cb;

	list_item_append(&handle->list, &notify->list[type]);

	return 0;
}

void notifier_unregister(void *receiver, void *caller, enum notify_id type)
{
	struct notify *notify = *arch_notify_get();
	struct list_item *wlist;
	struct list_item *tlist;
	struct callback_handle *handle;

	if (type >= NOTIFIER_ID_COUNT)
		return;

	list_for_item_safe(wlist, tlist, &notify->list[type]) {
		handle = container_of(wlist, struct callback_handle, list);
		if ((!receiver || handle->receiver == receiver) &&
		    (!caller || handle->caller == caller)) {
			list_item_del(&handle->list);
			free(handle);
		}
	}
}

void notifier_unregister_all(void *receiver, void *caller)
{
	int i;

	for (i = 0; i < NOTIFIER_ID_COUNT; i++)
		notifier_unregister(receiver, caller, i);
}
