/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_LIB_NOTIFIER_H__
#define __SOF_LIB_NOTIFIER_H__

#include <sof/list.h>
#include <sof/spinlock.h>
#include <stdint.h>

struct sof;

/* notifier target core masks */
#define NOTIFIER_TARGET_CORE_MASK(x)	(1 << x)
#define NOTIFIER_TARGET_CORE_ALL_MASK	0xFFFFFFFF

enum notify_id {
	NOTIFIER_ID_CPU_FREQ = 0,
	NOTIFIER_ID_SSP_FREQ,
	NOTIFIER_ID_KPB_CLIENT_EVT,
	NOTIFIER_ID_DMA_DOMAIN_CHANGE,
};

struct notify {
	spinlock_t *lock;	/* notifier lock */
	struct list_item list;	/* list of notifiers */
};

struct notify_data {
	enum notify_id id;
	uint32_t message;
	uint32_t target_core_mask;
	uint32_t data_size;
	void *data;
};

struct notifier {
	enum notify_id id;
	struct list_item list;
	void *cb_data;
	void (*cb)(int message, void *cb_data, void *event_data);
};

#define NOTIFIER_CLK_CHANGE_ID(clk) \
	((clk) == CLK_SSP ? NOTIFIER_ID_SSP_FREQ : NOTIFIER_ID_CPU_FREQ)

struct notify **arch_notify_get(void);

void notifier_register(struct notifier *notifier);
void notifier_unregister(struct notifier *notifier);

void notifier_notify(void);
void notifier_event(struct notify_data *notify_data);

void init_system_notify(struct sof *sof);

void free_system_notify(void);

#endif /* __SOF_LIB_NOTIFIER_H__ */
