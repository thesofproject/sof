// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Simple wait for event completion and signaling with timeouts.
 */

#include <sof/lib/clk.h>
#include <sof/lib/io.h>
#include <sof/lib/uuid.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

/* 1028070e-04e8-46ab-8d81-10a0116ce738 */
DECLARE_SOF_UUID("wait", wait_uuid, 0x1028070e, 0x04e8, 0x46ab,
		 0x8d, 0x81, 0x10, 0xa0, 0x11, 0x6c, 0xe7, 0x38);

DECLARE_TR_CTX(wait_tr, SOF_UUID(wait_uuid), LOG_LEVEL_INFO);

#define DEFAULT_TRY_TIMES 8

int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us)
{
	uint64_t tick = clock_ms_to_ticks(PLATFORM_DEFAULT_CLOCK, 1) *
						us / 1000;
	uint32_t tries = DEFAULT_TRY_TIMES;
	uint64_t delta = tick / tries;

	if (!delta) {
		delta = us;
		tries = 1;
	}

	while ((io_reg_read(reg) & mask) != val) {
		if (!tries--) {
			tr_err(&wait_tr, "ewt");
			return -EIO;
		}
		wait_delay(delta);
	}
	return 0;
}

void wait_delay(uint64_t number_of_clks)
{
	struct timer *timer = timer_get();
	uint64_t current = platform_timer_get(timer);

	while ((platform_timer_get(timer) - current) < number_of_clks)
		idelay(PLATFORM_DEFAULT_DELAY);
}
