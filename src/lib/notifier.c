// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/idc.h>
#include <rtos/interrupt.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/sof.h>
#include <ipc/topology.h>
#include <stdint.h>

LOG_MODULE_REGISTER(notifier, CONFIG_SOF_LOG_LEVEL);

/* 1fb15a7a-83cd-4c2e-8b32-4da1b2adeeaf */
DECLARE_SOF_UUID("notifier", notifier_uuid, 0x1fb15a7a, 0x83cd, 0x4c2e,
		 0x8b, 0x32, 0x4d, 0xa1, 0xb2, 0xad, 0xee, 0xaf);

DECLARE_TR_CTX(nt_tr, SOF_UUID(notifier_uuid), LOG_LEVEL_INFO);

static SHARED_DATA struct notify_data notify_data_shared[CONFIG_CORE_COUNT];

struct callback_handle {
	void *receiver;
	void *caller;
	void (*cb)(void *arg, enum notify_id, void *data);
	struct list_item list;
	uint32_t num_registrations;
};

int notifier_register(void *receiver, void *caller, enum notify_id type,
		      void (*cb)(void *arg, enum notify_id type, void *data),
		      uint32_t flags)
{
	struct notify *notify = *arch_notify_get();
	struct callback_handle *handle;
	k_spinlock_key_t key;
	int ret = 0;

	assert(type >= NOTIFIER_ID_CPU_FREQ && type < NOTIFIER_ID_COUNT);

	key = k_spin_lock(&notify->lock);

	/* Find already registered event of this type */
	if (flags & NOTIFIER_FLAG_AGGREGATE &&
	    !list_is_empty(&notify->list[type])) {
		handle = container_of((&notify->list[type])->next,
				      struct callback_handle, list);
		handle->num_registrations++;

		goto out;
	}

	handle = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			 sizeof(*handle));

	if (!handle) {
		tr_err(&nt_tr, "notifier_register(): callback handle allocation failed.");
		ret = -ENOMEM;
		goto out;
	}

	handle->receiver = receiver;
	handle->caller = caller;
	handle->cb = cb;
	handle->num_registrations = 1;

	list_item_prepend(&handle->list, &notify->list[type]);

out:
	k_spin_unlock(&notify->lock, key);
	return ret;
}

void notifier_unregister(void *receiver, void *caller, enum notify_id type)
{
	struct notify *notify = *arch_notify_get();
	struct list_item *wlist;
	struct list_item *tlist;
	struct callback_handle *handle;
	k_spinlock_key_t key;

	assert(type >= NOTIFIER_ID_CPU_FREQ && type < NOTIFIER_ID_COUNT);

	key = k_spin_lock(&notify->lock);

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
			if (!--handle->num_registrations) {
				list_item_del(&handle->list);
				rfree(handle);
			}
		}
	}

	k_spin_unlock(&notify->lock, key);
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
		dcache_invalidate_region((__sparse_force void __sparse_cache *)notify_data->data,
					 notify_data->data_size);
		notifier_notify(notify_data->caller, notify_data->type,
				notify_data->data);
	}

}

void notifier_event(const void *caller, enum notify_id type, uint32_t core_mask,
		    void *data, uint32_t data_size)
{
	struct notify_data *notify_data;
	struct idc_msg notify_msg = { IDC_MSG_NOTIFY, IDC_MSG_NOTIFY_EXT };
	int i;

	/* notify selected targets */
	for (i = 0; i < CONFIG_CORE_COUNT; i++) {
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

				dcache_writeback_region((__sparse_force void __sparse_cache *)
							notify_data->data, data_size);

				idc_send_msg(&notify_msg, IDC_NON_BLOCKING);
			}
		}
	}
}

void init_system_notify(struct sof *sof)
{
	struct notify **notify = arch_notify_get();
	int i;
	*notify = rzalloc(SOF_MEM_ZONE_SYS, SOF_MEM_FLAG_COHERENT, SOF_MEM_CAPS_RAM,
			  sizeof(**notify));

	k_spinlock_init(&(*notify)->lock);
	for (i = NOTIFIER_ID_CPU_FREQ; i < NOTIFIER_ID_COUNT; i++)
		list_init(&(*notify)->list[i]);

	if (cpu_get_id() == PLATFORM_PRIMARY_CORE_ID)
		sof->notify_data = platform_shared_get(notify_data_shared,
						       sizeof(notify_data_shared));
}

void free_system_notify(void)
{
}
