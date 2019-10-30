// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/idc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/notifier.h>
#include <sof/list.h>
#include <sof/spinlock.h>
#include <ipc/topology.h>

struct notify_data {
	void *caller;
	enum notify_id type;
	uint32_t data_size;
	void *data;
} __aligned(PLATFORM_DCACHE_ALIGN);

static struct notify_data _notify_data[PLATFORM_CORE_COUNT]
	__aligned(PLATFORM_DCACHE_ALIGN);

struct callback_handle {
	void *receiver;
	void *caller;
	void (*cb)(void *arg, enum notify_id, void *data);
	struct list_item list;
};

int notifier_register(void *receiver, void *caller, enum notify_id type,
		      void (*cb)(void *arg, enum notify_id type, void *data))
{
	struct notify *notify = *arch_notify_get();
	struct callback_handle *handle;

	assert(type >= NOTIFIER_ID_CPU_FREQ && type < NOTIFIER_ID_COUNT);

	handle = rzalloc(RZONE_SYS_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*handle));

	if (!handle)
		return -ENOMEM;

	handle->receiver = receiver;
	handle->caller = caller;
	handle->cb = cb;

	spin_lock(notify->lock);
	list_item_prepend(&handle->list, &notify->list[type]);
	spin_unlock(notify->lock);

	return 0;
}

void notifier_unregister(void *receiver, void *caller, enum notify_id type)
{
	struct notify *notify = *arch_notify_get();
	struct list_item *wlist;
	struct list_item *tlist;
	struct callback_handle *handle;

	assert(type >= NOTIFIER_ID_CPU_FREQ && type < NOTIFIER_ID_COUNT);

	/*
	 * Unregister all matching callbacks
	 * If receiver is NULL, unregister all callbacks with matching callers
	 * If caller is NULL, unregister all callbacks with matching receivers
	 *
	 * Event producer might force unregister all receivers by passing
	 * receiver NULL
	 * Event consumer might unregister from all callers by passing caller
	 * NULL
	 */
	spin_lock(notify->lock);
	list_for_item_safe(wlist, tlist, &notify->list[type]) {
		handle = container_of(wlist, struct callback_handle, list);
		if ((!receiver || handle->receiver == receiver) &&
		    (!caller || handle->caller == caller)) {
			list_item_del(&handle->list);
			rfree(handle);
		}
	}
	spin_unlock(notify->lock);
}

void notifier_unregister_all(void *receiver, void *caller)
{
	int i;

	for (i = NOTIFIER_ID_CPU_FREQ; i < NOTIFIER_ID_COUNT; i++)
		notifier_unregister(receiver, caller, i);
}

static void notifier_notify(void *caller, enum notify_id type, void *data)
{
	struct notify *notify = *arch_notify_get();
	struct list_item *wlist;
	struct list_item *tlist;
	struct callback_handle *handle;

	/* iterate through notifiers and send event to
	 * interested clients
	 */
	list_for_item_safe(wlist, tlist, &notify->list[type]) {
		handle = container_of(wlist, struct callback_handle, list);
		if (!caller || !handle->caller || handle->caller == caller)
			handle->cb(handle->receiver, type, data);
	}
}

void notifier_notify_remote(void)
{
	struct notify *notify = *arch_notify_get();
	struct notify_data *notify_data = &_notify_data[cpu_get_id()];

	dcache_invalidate_region(notify_data, sizeof(*notify_data));
	if (!list_is_empty(&notify->list[notify_data->type])) {
		dcache_invalidate_region(notify_data->data,
					 notify_data->data_size);
		notifier_notify(notify_data->caller, notify_data->type,
				notify_data->data);
	}
}

void notifier_event(void *caller, enum notify_id type, uint32_t core_mask,
		    void *data, uint32_t data_size)
{
	struct notify_data *notify_data;
	struct idc_msg notify_msg = { IDC_MSG_NOTIFY, IDC_MSG_NOTIFY_EXT };
	int i;

	/* notify selected targets */
	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (core_mask & NOTIFIER_TARGET_CORE_MASK(i)) {
			if (i == cpu_get_id()) {
				notifier_notify(caller, type, data);
			} else if (cpu_is_core_enabled(i)) {
				notify_msg.core = i;
				notify_data = &_notify_data[i];
				notify_data->caller = caller;
				notify_data->type = type;

				/* NOTE: for transcore events, payload has to
				 * be allocated on heap, not on stack
				 */
				notify_data->data = data;
				notify_data->data_size = data_size;

				dcache_writeback_region(notify_data->data,
							data_size);
				dcache_writeback_region(notify_data,
							sizeof(*notify_data));

				idc_send_msg(&notify_msg, IDC_NON_BLOCKING);
			}
		}
	}
}

void init_system_notify(struct sof *sof)
{
	struct notify **notify = arch_notify_get();
	int i;
	*notify = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**notify));

	for (i = NOTIFIER_ID_CPU_FREQ; i < NOTIFIER_ID_COUNT; i++)
		list_init(&(*notify)->list[i]);
	spinlock_init(&(*notify)->lock);
}

void free_system_notify(void)
{
}
