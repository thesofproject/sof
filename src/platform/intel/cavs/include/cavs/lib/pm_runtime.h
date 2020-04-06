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

struct pm_runtime_data;

/** \brief cAVS specific runtime power management data. */
struct cavs_pm_runtime_data {
	int dsp_d0_sref; /**< simple ref counter, accessed by core 0 only */
	int host_dma_l1_sref; /**< ref counter for Host DMA accesses */
};

#endif

#else

#error "Do not include outside of platform/lib/pm_runtime.h"

#endif
