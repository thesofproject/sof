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
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/list.h>
#include <sof/sof.h>
#include <ipc/topology.h>

#define trace_notifier(__e, ...) \
	trace_event(TRACE_CLASS_NOTIFIER, __e, ##__VA_ARGS__)
#define tracev_notifier(__e, ...) \
	tracev_event(TRACE_CLASS_NOTIFIER, __e, ##__VA_ARGS__)
#define trace_notifier_error(__e, ...) \
	trace_error(TRACE_CLASS_NOTIFIER, __e, ##__VA_ARGS__)

static SHARED_DATA struct notify_data notify_data[PLATFORM_CORE_COUNT];

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

	handle = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			 sizeof(*handle));

	if (!handle) {
		trace_notifier_error("notifier_register() error: callback "
				     "handle allocation failed.");
		return -ENOMEM;
	}

	handle->receiver = receiver;
	handle->caller = caller;
	handle->cb = cb;

	list_item_prepend(&handle->list, &notify->list[type]);

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
	list_for_item_safe(wlist, tlist, &notify->list[type]) {
		handle = container_of(wlist, struct callback_handle, list);
		if ((!receiver || handle->receiver == receiver) &&
		    (!caller || handle->caller == caller)) {
			list_item_del(&handle->list);
			rfree(handle);
		}
	}
}

void notifier_unregister_all(void *receiver, void *caller)
{
	int i;

	for (i = NOTIFIER_ID_CPU_FREQ; i < NOTIFIER_ID_COUNT; i++)
		notifier_unregister(receiver, caller, i);
}

static void notifier_notify(const void *caller, enum notify_id type, void *data)
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
	struct notify_data *notify_data = notify_data_get() + cpu_get_id();

	if (!list_is_empty(&notify->list[notify_data->type])) {
		dcache_invalidate_region(notify_data->data,
					 notify_data->data_size);
		notifier_notify(notify_data->caller, notify_data->type,
				notify_data->data);
	}

	platform_shared_commit(notify_data, sizeof(*notify_data));
}

void notifier_event(const void *caller, enum notify_id type, uint32_t core_mask,
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
				notify_data = notify_data_get() + i;
				notify_data->caller = caller;
				notify_data->type = type;

				/* NOTE: for transcore events, payload has to
				 * be allocated on heap, not on stack
				 */
				notify_data->data = data;
				notify_data->data_size = data_size;

				dcache_writeback_region(notify_data->data,
							data_size);

				platform_shared_commit(notify_data,
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
	*notify = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
			  sizeof(**notify));

	for (i = NOTIFIER_ID_CPU_FREQ; i < NOTIFIER_ID_COUNT; i++)
		list_init(&(*notify)->list[i]);

	if (cpu_get_id() == PLATFORM_MASTER_CORE_ID)
		sof->notify_data = platform_shared_get(notify_data,
						       sizeof(notify_data));
}

void free_system_notify(void)
{
}
