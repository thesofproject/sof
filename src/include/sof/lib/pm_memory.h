/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@linux.intel.com>
 */

/**
 * \file include/sof/lib/pm_memory.h
 * \brief Runtime power management header file
 * \author Jakub Dabek <jakub.dabek@linux.intel.com>
 */

#ifndef __SOF_LIB_PM_MEMORY_H__
#define __SOF_LIB_PM_MEMORY_H__

#include <sof/spinlock.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
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

/**
 * \brief Set power gating of memory banks in the address range
 *
 * Power gates address range only on full banks if given address form mid block
 * it will try to narrow down power gate to nearest full banks
 *
 * \param[in] ptr Ptr to address from which to start gating.
 * \param[in] size Size of memory to manage.
 * \param[in] enabled Boolean deciding banks desired state (1 powered 0 gated).
 */
void set_power_gate_for_memory_address_range(void *ptr,
					     uint32_t size, uint32_t enabled);

#endif /* __SOF_LIB_PM_MEMORY_H__ */
