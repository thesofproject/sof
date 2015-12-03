/*
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#include <reef/notifier.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>

/* General purpose notifiers */

struct notify {
	spinlock_t lock;
	struct list_head list;	/* list of notifiers */
};

static struct notify _notify;

void notifier_register(struct notifier *notifier)
{
	spin_lock(&_notify.lock);
	list_add(&notifier->list, &_notify.list);
	spin_unlock(&_notify.lock);
}

void notifier_unregister(struct notifier *notifier)
{
	spin_lock(&_notify.lock);
	list_del(&notifier->list);
	spin_unlock(&_notify.lock);
}

void notifier_event(int id, int message, void *event_data)
{
	struct list_head *wlist;
	struct notifier *n;

	spin_lock(&_notify.lock);

	/* iterate through notifiers and send event to interested clients */
	list_for_each(wlist, &_notify.list) {

		n = container_of(wlist, struct notifier, list);
		if (n->id == id)
			n->cb(message, n->cb_data, event_data);
	}

	spin_unlock(&_notify.lock);
}

void init_system_notify(void)
{
	list_init(&_notify.list);
	spinlock_init(&_notify.lock);
}
