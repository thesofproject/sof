/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#include <sof/perfcount.h>
#include <sof/timer.h>
#include <sof/math/numbers.h>

struct perfcount_data {
	struct timer *perfcount_timer;
	struct list_item free_contexts;
	uint32_t next_slot;
};

static struct perfcount_data *pd;

static void perfcount_timer_handler(void *arg) {}

static void sys_perfcount_init(void)
{
	pd = (struct perfcount_data *)rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM,
					      sizeof(*pd));

	pd->perfcount_timer = &platform_internal_timers[0].timer;
	arch_timer_register(pd->perfcount_timer, perfcount_timer_handler, NULL);

	/* setting timer to 1 tick, for 32-bit rollover handling */
	arch_timer_set(pd->perfcount_timer, 1);

	list_init(&pd->free_contexts);
}

DECLARE_MODULE(sys_perfcount_init);

static inline void *perfcount_get_slot(void)
{
	void *slot = (void *)mailbox_get_perfcount_base();

	slot += pd->next_slot * sizeof(struct perfcount_measure);

	if ((uint32_t)slot + sizeof(struct perfcount_measure) >=
	    mailbox_get_perfcount_base() + mailbox_get_perfcount_size()) {
		slot = NULL;
	}

	return slot;
}

struct perfcount_context *perfcount_init(uint32_t task_id)
{
	struct perfcount_context *context = NULL;

	if (list_is_empty(&pd->free_contexts)) {
		context = (struct perfcount_context *)rzalloc(RZONE_RUNTIME,
							      SOF_MEM_CAPS_RAM,
							      sizeof(*context));

		context->mailbox_slot = perfcount_get_slot();
	} else {
		context = list_first_item(&pd->free_contexts,
					  struct perfcount_context, reuse_list);

		bzero(context, sizeof(*context));
		list_item_del(&context->reuse_list);
	}

	context->measure.task_id = task_id;

	return context;
}

void perfcount_free(struct perfcount_context **context)
{
	list_item_prepend(&(*context)->reuse_list, &pd->free_contexts);
	context = NULL;
}

static inline void perfcount_write_stats(struct perfcount_context *context)
{
	if (context->mailbox_slot)
		assert(!memcpy_s(context->mailbox_slot,
				 sizeof(context->measure),
				 &context->measure,
				 sizeof(context->measure)));
}

static inline void perfcount_update_cur_stat(struct perfcount_context *context,
					     struct perfcount_sample *sample)
{
	struct perfcount_sample *stat =
		&context->measure.stats[PERFCOUNT_STAT_CUR];

	stat->time_delta = sample->time_delta;
	stat->ccount_delta = sample->ccount_delta;
}

static inline void perfcount_update_max_stat(struct perfcount_context *context,
					     struct perfcount_sample *sample)
{
	struct perfcount_sample *stat =
		&context->measure.stats[PERFCOUNT_STAT_MAX];

	stat->time_delta = MAX(sample->time_delta, stat->time_delta);
	stat->ccount_delta = MAX(sample->ccount_delta, stat->ccount_delta);
}

static inline void perfcount_update_avg_stat(struct perfcount_context *context)
{
	struct perfcount_sample *stat =
		&context->measure.stats[PERFCOUNT_STAT_AVG];
	int avg_step;

	stat->ccount_delta = 0;
	stat->time_delta = 0;

	for (avg_step = 0; avg_step < PERFCOUNT_NUM_STEPS; ++avg_step) {
		stat->ccount_delta += context->samples[avg_step].ccount_delta;
		stat->time_delta += context->samples[avg_step].time_delta;
	}

	stat->ccount_delta >>= PERFCOUNT_AVG_SHIFT;
	stat->time_delta >>= PERFCOUNT_AVG_SHIFT;
}

static inline void perfcount_update_stats(struct perfcount_context *context,
					  struct perfcount_sample *sample)
{
	perfcount_update_cur_stat(context, sample);
	perfcount_update_avg_stat(context);
	perfcount_update_max_stat(context, sample);
}

void perfcount_begin(struct perfcount_context *context)
{
	context->begin_time = platform_timer_get(platform_timer);
	context->begin_ccount = arch_timer_get_system(pd->perfcount_timer);
}

void perfcount_end(struct perfcount_context *context)
{
	struct perfcount_sample *sample = &context->samples[context->cur_step];
	uint64_t cur_time = platform_timer_get(platform_timer);
	uint64_t cur_ccount = arch_timer_get_system(pd->perfcount_timer);

	if (++context->cur_step >= PERFCOUNT_NUM_STEPS)
		context->cur_step = 0;

	sample->time_delta = cur_time - context->begin_time;
	sample->ccount_delta = cur_ccount - context->begin_ccount;

	perfcount_update_stats(context, sample);
}

uint64_t perfcount_get_microseconds(struct perfcount_context *context,
				    int stat_type)
{
	uint32_t freq_hz = clock_get_freq(PLATFORM_DEFAULT_CLOCK);

	return context->measure.stats[stat_type].time_delta /
		(freq_hz / 1000000UL);
}

uint64_t perfcount_get_ccount(struct perfcount_context *context, int stat_type)
{
	return context->measure.stats[stat_type].ccount_delta;
}
