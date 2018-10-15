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
 */

#include <sof/notifier.h>
#include <sof/sof.h>
#include <sof/list.h>
#include <sof/alloc.h>
#include <sof/cpu.h>
#include <sof/idc.h>
#include <platform/idc.h>

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
