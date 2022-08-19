/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_LIB_NOTIFIER_H__
#define __SOF_LIB_NOTIFIER_H__

#include <rtos/bit.h>
#include <sof/list.h>
#include <rtos/spinlock.h>
#include <sof/sof.h>
#include <stdint.h>

/* notifier target core masks */
#define NOTIFIER_TARGET_CORE_MASK(x)	(1 << (x))
#define NOTIFIER_TARGET_CORE_LOCAL	NOTIFIER_TARGET_CORE_MASK(cpu_get_id())
#define NOTIFIER_TARGET_CORE_ALL_MASK	0xFFFFFFFF

/** \brief Notifier flags. */
#define NOTIFIER_FLAG_AGGREGATE		BIT(0)

enum notify_id {
	NOTIFIER_ID_CPU_FREQ = 0,		/* struct clock_notify_data * */
	NOTIFIER_ID_SSP_FREQ,			/* struct clock_notify_data * */
	NOTIFIER_ID_KPB_CLIENT_EVT,		/* struct kpb_event_data * */
	NOTIFIER_ID_DMA_DOMAIN_CHANGE,		/* struct dma_chan_data * */
	NOTIFIER_ID_BUFFER_PRODUCE,		/* struct buffer_cb_transact* */
	NOTIFIER_ID_BUFFER_CONSUME,		/* struct buffer_cb_transact* */
	NOTIFIER_ID_BUFFER_FREE,		/* struct buffer_cb_free* */
	NOTIFIER_ID_DMA_COPY,			/* struct dma_cb_data* */
	NOTIFIER_ID_LL_POST_RUN,		/* NULL */
	NOTIFIER_ID_DMA_IRQ,			/* struct dma_chan_data * */
	NOTIFIER_ID_DAI_TRIGGER,		/* struct dai_group * */
	NOTIFIER_ID_COUNT
};

struct notify {
	struct list_item list[NOTIFIER_ID_COUNT]; /* list of callback handles */
	struct k_spinlock lock;	/* list lock */
};

struct notify_data {
	const void *caller;
	enum notify_id type;
	uint32_t data_size;
	void *data;
};

#ifdef CLK_SSP
#define NOTIFIER_CLK_CHANGE_ID(clk) \
	((clk) == CLK_SSP ? NOTIFIER_ID_SSP_FREQ : NOTIFIER_ID_CPU_FREQ)
#else
#define NOTIFIER_CLK_CHANGE_ID(clk) NOTIFIER_ID_CPU_FREQ
#endif

struct notify **arch_notify_get(void);

/** Register a callback to be run when event 'type' happens.
 *
 * The identifier for un-registration is the tuple (receiver_data,
 * caller_id_filter, event_type), the callback argument is not part of
 * it.
 *
 * caller_data argument from notifier_event()
 *
 * @param receiver_data private data passed to the callback.
 * @param caller_id_filter optional, can be used to be notified only by
 * some specific notifier_event() calls when not NULL.
 * @param event_type list of callbacks to be added to
 * @param callback callback function
 * @param flags see NOTIFIER_FLAG_* above
 */
int notifier_register(void *receiver_data, void *caller_id_filter, enum notify_id event_type,
		      void (*callback)(void *receiver_data, enum notify_id event_type,
				       void *caller_data),
		      uint32_t flags);

/** Unregister all callbacks matching that arguments tuple. NULL acts
 * as a wildcard.
 */
void notifier_unregister(void *receiver_data_filter, void *caller_id_filter, enum notify_id type);

/** Unregister callbacks matching the arguments for every notify_id.
 *  A NULL parameter acts as a wildcard.
 */
void notifier_unregister_all(void *receiver_data_filter, void *caller_id_filter);

void notifier_notify_remote(void);

/* data_size is required to manage cache coherency for notifications
 * across cores.
 */
void notifier_event(const void *caller_id, enum notify_id event_type, uint32_t core_mask,
		    void *caller_data, uint32_t data_size);

void init_system_notify(struct sof *sof);

void free_system_notify(void);

static inline struct notify_data *notify_data_get(void)
{
	return sof_get()->notify_data;
}

#endif /* __SOF_LIB_NOTIFIER_H__ */
