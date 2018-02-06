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

#include <reef/notifier.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>

/* General purpose notifiers */

struct notify {
	spinlock_t lock;
	struct list_item list;	/* list of notifiers */
};

static struct notify _notify;

void notifier_register(struct notifier *notifier)
{
	spin_lock(&_notify.lock);
	list_item_prepend(&notifier->list, &_notify.list);
	spin_unlock(&_notify.lock);
}

void notifier_unregister(struct notifier *notifier)
{
	spin_lock(&_notify.lock);
	list_item_del(&notifier->list);
	spin_unlock(&_notify.lock);
}

void notifier_event(int id, int message, void *event_data)
{
	struct list_item *wlist;
	struct notifier *n;

	spin_lock(&_notify.lock);

	if (list_is_empty(&_notify.list))
		goto out;

	/* iterate through notifiers and send event to interested clients */
	list_for_item(wlist, &_notify.list) {

		n = container_of(wlist, struct notifier, list);
		if (n->id == id)
			n->cb(message, n->cb_data, event_data);
	}

out:
	spin_unlock(&_notify.lock);
}

void init_system_notify(void)
{
	list_init(&_notify.list);
	spinlock_init(&_notify.lock);
}
