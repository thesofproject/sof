/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

#ifdef __PLATFORM_POWER_DOWN_H__

#ifndef __CAVS_POWER_DOWN_H__
#define __CAVS_POWER_DOWN_H__

#include <stdbool.h>
/**
 * Power down procedure.
 * Locks its code in L1 cache and shuts down memories.
 * @param  disable_lpsram        flag if LPSRAM is to be disabled (whole)
 * @param  hpsram_pwrgating_mask pointer to memory segments power gating mask
 * (each bit corresponds to one ebb)
 * @return                       nothing returned - this function never quits
 */
void power_down(bool disable_lpsram, uint32_t *hpsram_pwrgating_mask);

#endif /* __CAVS_POWER_DOWN_H__ */

#else

#error "This file shouldn't be included from outside of platform/power_down.h"

#endif /* __PLATFORM_POWER_DOWN_H__ */
