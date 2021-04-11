/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_LIB_AGENT_H__
#define __SOF_LIB_AGENT_H__

#include <sof/atomic.h>
#include <sof/lib/memory.h>
#include <sof/lib/perf_cnt.h>
#include <sof/schedule/task.h>
#include <sof/sof.h>

#include <stdbool.h>
#include <stdint.h>

struct sof;

/* simple agent */
struct sa {
	uint64_t last_check;	/* time of last activity checking */
	uint64_t panic_timeout;	/* threshold of panic */
	uint64_t warn_timeout;	/* threshold of warning */
#if CONFIG_PERFORMANCE_COUNTERS
	struct perf_cnt_data pcd;
#endif
	struct task work;
	atomic_t panic_cnt;	/**< ref counter for panic_on_delay property */
	bool panic_on_delay;	/**< emits panic on delay if true */
};

#if CONFIG_HAVE_AGENT

/**
 * Enables or disables panic on agent delay.
 * @param enabled True for panic enabling, false otherwise.
 */
static inline void sa_set_panic_on_delay(bool enabled)
{
	struct sa *sa = sof_get()->sa;

	if (enabled)
		atomic_add(&sa->panic_cnt, 1);
	else
		atomic_sub(&sa->panic_cnt, 1);

	/* enable panic only if no refs */
	sa->panic_on_delay = !atomic_read(&sa->panic_cnt);

}

void sa_init(struct sof *sof, uint64_t timeout);

#else

static inline void sa_init(struct sof *sof, uint64_t timeout) { }
static inline void sa_set_panic_on_delay(bool enabled) { }

#endif

#endif /* __SOF_LIB_AGENT_H__ */
