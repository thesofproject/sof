/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file cavs/lib/pm_memory.h
 * \brief Memory power management header file for cAVS platforms
 * \author Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __PLATFORM_LIB_PM_MEMORY_H__

#ifndef __CAVS_LIB_PM_MEMORY_H__
#define __CAVS_LIB_PM_MEMORY_H__

#include <stdint.h>

/** \brief Memory banks pm masks data. */
struct ebb_data {
	uint32_t current_mask0;
	uint32_t current_mask1;
	uint32_t new_mask0;
	uint32_t new_mask1;
	uint32_t change_mask0;
	uint32_t change_mask1;
	uint32_t start_bank_id;
	uint32_t ending_bank_id;
	uint32_t start_bank_id_high;
	uint32_t ending_bank_id_high;
};

#endif /* __CAVS_LIB_PM_MEMORY_H__ */

#else

#error "Do not include outside of platform/lib/pm_memory.h"

#endif /* __PLATFORM_LIB_PM_MEMORY_H__ */
