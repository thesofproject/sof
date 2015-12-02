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

struct notifier {
	int id;
	struct list_head list;
	void *cb_data;
	void (*cb)(int message, void *cb);
};

void notifier_register(struct notifier *notifier);
void notifier_unregister(struct notifier *notifier);

void notifier_event(int id, int message);

#endif
