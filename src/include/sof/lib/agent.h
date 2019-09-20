/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_LIB_AGENT_H__
#define __SOF_LIB_AGENT_H__

#include <sof/schedule/task.h>
#include <stdbool.h>
#include <stdint.h>

struct sof;

/* simple agent */
struct sa {
	uint64_t last_check;	/* time of last activity checking */
	uint64_t panic_timeout;	/* threshold of panic */
	uint64_t warn_timeout;	/* threshold of warning */
	struct task work;
	bool is_active;
};

void sa_init(struct sof *sof, uint64_t timeout);
void sa_disable(void);
void sa_enable(void);

#endif /* __SOF_LIB_AGENT_H__ */
