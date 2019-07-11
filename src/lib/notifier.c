// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <sof/notifier.h>
#include <sof/sof.h>
#include <sof/list.h>
#include <sof/alloc.h>
#include <sof/cpu.h>
#include <sof/idc.h>

static struct notify_data _notify_data;

void notifier_register(struct notifier *notifier)
{
	struct notify *notify = *arch_notify_get();

	spin_lock(&notify->lock);
	list_item_prepend(&notifier->list, &notify->list);
	spin_unlock(&notify->lock);
}

void notifier_unregister(struct notifier *notifier)
{
	struct notify *notify = *arch_notify_get();

	spin_lock(&notify->lock);
	list_item_del(&notifier->list);
	spin_unlock(&notify->lock);
}

void notifier_notify(void)
{
	struct notify *notify = *arch_notify_get();
	struct list_item *wlist;
	struct notifier *n;

	if (!list_is_empty(&notify->list)) {
		dcache_invalidate_region(&_notify_data, sizeof(_notify_data));
		dcache_invalidate_region(_notify_data.data,
					 _notify_data.data_size);

		/* iterate through notifiers and send event to
		 * interested clients
		 */
		list_for_item(wlist, &notify->list) {
			n = container_of(wlist, struct notifier, list);
			if (n->id == _notify_data.id)
				n->cb(_notify_data.message, n->cb_data,
				      _notify_data.data);
		}
	}
}

void notifier_event(struct notify_data *notify_data)
{
	struct notify *notify = *arch_notify_get();
	struct idc_msg notify_msg = { IDC_MSG_NOTIFY, IDC_MSG_NOTIFY_EXT };
	int i = 0;

	spin_lock(&notify->lock);

	_notify_data = *notify_data;
	dcache_writeback_region(_notify_data.data, _notify_data.data_size);
	dcache_writeback_region(&_notify_data, sizeof(_notify_data));

	/* notify selected targets */
	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		if (_notify_data.target_core_mask & (1 << i)) {
			if (i == cpu_get_id()) {
				notifier_notify();
			} else if (cpu_is_core_enabled(i)) {
				notify_msg.core = i;
				idc_send_msg(&notify_msg, IDC_BLOCKING);
			}
		}
	}

	spin_unlock(&notify->lock);
}

void init_system_notify(struct sof *sof)
{
	struct notify **notify = arch_notify_get();
	*notify = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**notify));

	list_init(&(*notify)->list);
	spinlock_init(&(*notify)->lock);
}

void free_system_notify(void)
{
	struct notify *notify = *arch_notify_get();

	spin_lock(&notify->lock);
	list_item_del(&notify->list);
	spin_unlock(&notify->lock);
}
