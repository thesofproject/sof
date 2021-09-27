/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file cavs/lib/pm_runtime.h
 * \brief Runtime power management header file for cAVS platforms
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __PLATFORM_LIB_PM_RUNTIME_H__

#ifndef __CAVS_LIB_PM_RUNTIME_H__
#define __CAVS_LIB_PM_RUNTIME_H__

/**
 * \brief extra pwr flag to power up a core with a specific reason
 * it can be powered down only with the same reason (flag)
 */
#define PWRD_MASK	MASK(31, 30)
#define PWRD_BY_HPRO	BIT(31)		/**< requested by HPRO */
#define PWRD_BY_TPLG	BIT(30)		/**< typical power up */

struct pm_runtime_data;

/** \brief cAVS specific runtime power management data. */
struct cavs_pm_runtime_data {
	bool dsp_d0; /**< dsp target D0(true) or D0ix(false) */
	int host_dma_l1_sref; /**< ref counter for Host DMA accesses */
	uint32_t sleep_core_mask; /**< represents cores in waiti state */
	uint32_t prepare_d0ix_core_mask; /**< indicates whether core needs */
					   /**< to prepare to d0ix power down */
					   /**<	before next waiti */
	int dsp_client_bitmap[CONFIG_CORE_COUNT]; /**< simple pwr override */
};

/**
 * \brief CAVS DSP residency counters
 * R0 - HPRO clock, highest power consumption state
 * R1 - LPRO clock, low power consumption state
 * R2 - LPS, lowest power consumption state
 * with extra priority to R2 (LPS) which cannot be interrupted by R0/R1 changes
 */

#endif

#else

#error "Do not include outside of platform/lib/pm_runtime.h"

#endif
