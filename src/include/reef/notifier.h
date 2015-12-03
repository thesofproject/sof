/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_NOTIFIER__
#define __INCLUDE_NOTIFIER__

#include <reef/list.h>

/* notifier general IDs */
#define NOTIFIER_ID_CPU_FREQ	0

struct notifier {
	int id;
	struct list_head list;
	void *cb_data;
	void (*cb)(int message, void *cb_data, void *event_data);
};

void notifier_register(struct notifier *notifier);
void notifier_unregister(struct notifier *notifier);

void notifier_event(int id, int message, void *event_data);

#endif
