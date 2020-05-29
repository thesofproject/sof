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

#ifndef __CAVS_LIB_PM_MEMORY_H__
#define __CAVS_LIB_PM_MEMORY_H__

#include <config.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * \brief Sets HP SRAM enabled gating hw bit mask for memory banks.
 * \param[in] start_bank_id Id of first bank to be managed (inclusive) 0 based.
 * \param[in] ending_bank_id Id of last bank to be managed (inclusive) 0 based.
 * \param[in] enabled Deciding banks desired state (true powered false gated).
 */
void cavs_pm_memory_hp_sram_banks_power_gate(uint32_t start_bank_id,
					     uint32_t ending_bank_id,
					     bool enabled);

/**
 * \brief Sets HP SRAM power gating.
 *
 * Power gates address range only on full banks. If given address form mid block
 * it will try to narrow down power gate to nearest full banks.
 *
 * \param[in] ptr Ptr to address from which to start gating.
 * \param[in] size Size of memory to manage.
 * \param[in] enabled Deciding banks desired state (true powered false gated).
 */
void cavs_pm_memory_hp_sram_power_gate(void *ptr, uint32_t size, bool enabled);

#if CONFIG_LP_SRAM

/**
 * \brief Sets LP SRAM enabled gating hw bit mask for memory banks.
 * \param[in] start_bank_id Id of first bank to be managed (inclusive) 0 based.
 * \param[in] ending_bank_id Id of last bank to be managed (inclusive) 0 based.
 * \param[in] enabled Deciding banks desired state (true powered false gated).
 */
void cavs_pm_memory_lp_sram_banks_power_gate(uint32_t start_bank_id,
					     uint32_t ending_bank_id,
					     bool enabled);

/**
 * \brief Sets LP SRAM power gating.
 *
 * Power gates address range only on full banks. If given address form mid block
 * it will try to narrow down power gate to nearest full banks.
 *
 * \param[in] ptr Ptr to address from which to start gating.
 * \param[in] size Size of memory to manage.
 * \param[in] enabled Deciding banks desired state (true powered false gated).
 */
void cavs_pm_memory_lp_sram_power_gate(void *ptr, uint32_t size, bool enabled);

#endif /* CONFIG_LP_SRAM */

#endif /* __CAVS_LIB_PM_MEMORY_H__ */
