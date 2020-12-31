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

#include <cavs/version.h>
#include <sof/bit.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#include <sof/lib/wait.h>
#include <sof/math/numbers.h>

#include <stdbool.h>
#include <stdint.h>

#define MEMORY_POWER_DOWN_DELAY 256

#if CAVS_VERSION >= CAVS_VERSION_1_8

/**
 * \brief Retrieves register mask for given segment.
 * \param[in] start_bank Start bank id.
 * \param[in] end_bank End bank id.
 * \param[in] segment Segment id.
 * \return Register mask.
 */
static inline uint32_t cavs_pm_memory_hp_sram_mask_get(uint32_t start_bank,
						       uint32_t end_bank,
						       int segment)
{
	uint32_t first_in_segment;
	uint32_t last_in_segment;

	first_in_segment = segment * EBB_SEGMENT_SIZE;
	last_in_segment = ((segment + 1) * EBB_SEGMENT_SIZE) - 1;

	/* not in this segment */
	if (start_bank > last_in_segment || end_bank < first_in_segment)
		return 0;

	if (start_bank < first_in_segment)
		start_bank = first_in_segment;

	if (end_bank > last_in_segment)
		end_bank = last_in_segment;

	return MASK(end_bank - first_in_segment, start_bank - first_in_segment);
}

/**
 * \brief Sets register mask for given segment.
 * \param[in] mask Register mask to be set.
 * \param[in] segment Segment id.
 * \param[in] enabled True if banks should be enabled, false otherwise.
 */
static inline void cavs_pm_memory_hp_sram_mask_set(uint32_t mask, int segment,
						   bool enabled)
{
	uint32_t expected = enabled ? 0 : mask;

	io_reg_update_bits(SHIM_HSPGCTL(segment), mask, enabled ? 0 : mask);
	io_reg_update_bits(SHIM_HSRMCTL(segment), mask, enabled ? 0 : mask);

	idelay(MEMORY_POWER_DOWN_DELAY);

	while ((io_reg_read(SHIM_HSPGISTS(segment)) & mask) != expected)
		idelay(MEMORY_POWER_DOWN_DELAY);
}

static inline void cavs_pm_memory_hp_sram_banks_power_gate(
	uint32_t start_bank_id, uint32_t ending_bank_id, bool enabled)
{
	uint32_t mask;
	int i;

	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_HPSRAM_LDO_ON);

	idelay(MEMORY_POWER_DOWN_DELAY);

	for (i = 0; i < PLATFORM_HPSRAM_SEGMENTS; ++i) {
		mask = cavs_pm_memory_hp_sram_mask_get(start_bank_id,
						       ending_bank_id, i);
		cavs_pm_memory_hp_sram_mask_set(mask, i, enabled);
	}

	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_HPSRAM_LDO_BYPASS);
}

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

#endif /* CAVS_VERSION >= CAVS_VERSION_1_8 */

#if CONFIG_LP_SRAM

/**
 * \brief Sets LP SRAM enabled gating hw bit mask for memory banks.
 * \param[in] start_bank_id Id of first bank to be managed (inclusive) 0 based.
 * \param[in] ending_bank_id Id of last bank to be managed (inclusive) 0 based.
 * \param[in] enabled Deciding banks desired state (true powered false gated).
 */
static inline void cavs_pm_memory_lp_sram_banks_power_gate(
	uint32_t start_bank_id, uint32_t ending_bank_id, bool enabled)
{
	uint32_t mask = MASK(ending_bank_id, start_bank_id);
	uint32_t expected = enabled ? 0 : mask;

	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_LPSRAM_LDO_ON);

	idelay(MEMORY_POWER_DOWN_DELAY);

	io_reg_update_bits(LSPGCTL, mask, enabled ? 0 : mask);

	idelay(MEMORY_POWER_DOWN_DELAY);

	while ((io_reg_read(LSPGISTS) & mask) != expected)
		idelay(MEMORY_POWER_DOWN_DELAY);

	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_HPSRAM_LDO_BYPASS);
}

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
