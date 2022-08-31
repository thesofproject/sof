/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ZEPHYR_RTOS_WAIT_H__
#define __ZEPHYR_RTOS_WAIT_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

/* TODO: use equivalent Zephyr */
static inline void idelay(int n)
{
	while (n--)
		asm volatile("nop");
}

/* DSP default delay in cycles - all platforms use this today */
#define PLATFORM_DEFAULT_DELAY	12

static inline void wait_delay(uint64_t number_of_clks)
{
	uint64_t timeout = k_cycle_get_64() + number_of_clks;

	while (k_cycle_get_64() < timeout)
		idelay(PLATFORM_DEFAULT_DELAY);
}

static inline void wait_delay_ms(uint64_t ms)
{
	wait_delay(k_ms_to_cyc_ceil64(ms));
}

static inline void wait_delay_us(uint64_t us)
{
	wait_delay(k_us_to_cyc_ceil64(us));
}

int poll_for_register_delay(uint32_t reg, uint32_t mask,
			    uint32_t val, uint64_t us);

/* TODO: this is tmp used by some IMX IPC drivers that have not transitioned
 * to Zephyr. To be removed after drivers migrated to Zephyr. It's xtensa only too.
 */
static inline void wait_for_interrupt(int level)
{
	asm volatile("waiti 0");
}

#endif /* __ZEPHYR_RTOS_WAIT_H__ */
