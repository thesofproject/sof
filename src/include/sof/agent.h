/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_SOF_AGENT__
#define __INCLUDE_SOF_AGENT__

#include <stdint.h>
#include <stddef.h>
#include <sof/schedule.h>

struct sof;

/* simple agent */
struct sa {
	uint64_t last_idle;	/* time of last idle */
	uint64_t ticks;
	struct task work;
};

void sa_enter_idle(struct sof *sof);
void sa_init(struct sof *sof);

#endif
