// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

/*
 * Simple wait for event completion and signaling with timeouts.
 */

#include <rtos/clk.h>
#include <sof/lib/io.h>
#include <sof/lib/uuid.h>
#include <rtos/wait.h>
#include <sof/platform.h>
#include <sof/schedule/schedule.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

LOG_MODULE_REGISTER(wait, CONFIG_SOF_LOG_LEVEL);

/* 1028070e-04e8-46ab-8d81-10a0116ce738 */
DECLARE_SOF_UUID("wait", wait_uuid, 0x1028070e, 0x04e8, 0x46ab,
		 0x8d, 0x81, 0x10, 0xa0, 0x11, 0x6c, 0xe7, 0x38);

DECLARE_TR_CTX(wait_tr, SOF_UUID(wait_uuid), LOG_LEVEL_INFO);

#define DEFAULT_TRY_TIMES 8

int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us)
{
	uint64_t tick = k_us_to_cyc_ceil64(us);
	uint32_t tries = DEFAULT_TRY_TIMES;
	uint64_t delta = tick / tries;

	if (!delta) {
		/*
		 * If we want to wait for less than DEFAULT_TRY_TIMES ticks then
		 * delta has to be set to 1 and number of tries to that of number
		 * of ticks.
		 */
		delta = 1;
		tries = tick;
	}

	while ((io_reg_read(reg) & mask) != val) {
		if (!tries--) {
			tr_err(&wait_tr, "poll timeout reg %u mask %u val %u us %u",
			       reg, mask, val, (uint32_t)us);
			return -EIO;
		}
		wait_delay(delta);
	}
	return 0;
}

void wait_delay(uint64_t number_of_clks)
{
	uint64_t timeout = sof_cycle_get_64() + number_of_clks;

	while (sof_cycle_get_64() < timeout)
		idelay(PLATFORM_DEFAULT_DELAY);
}

void wait_delay_ms(uint64_t ms)
{
	wait_delay(k_ms_to_cyc_ceil64(ms));
}

void wait_delay_us(uint64_t us)
{
	wait_delay(k_us_to_cyc_ceil64(us));
}
