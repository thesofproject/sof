/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@intel.com>
 */

#include <cavs/lib/pm_memory.h>
#include <cavs/version.h>
#include <sof/bit.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#include <sof/lib/uuid.h>
#include <sof/lib/wait.h>
#include <sof/math/numbers.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <config.h>
#include <stdbool.h>
#include <stdint.h>

/* 14f25ab6-3a4b-4e5d-b343-2a142d4e4d92 */
DECLARE_SOF_UUID("pm-memory", pm_mem_uuid, 0x14f25ab6, 0x3a4b, 0x4e5d,
		 0xb3, 0x43, 0x2a, 0x14, 0x2d, 0x4e, 0x4d, 0x92);

DECLARE_TR_CTX(pm_mem_tr, SOF_UUID(pm_mem_uuid), LOG_LEVEL_INFO);

#if CAVS_VERSION >= CAVS_VERSION_1_8

#define MEMORY_POWER_DOWN_DELAY 256

/**
 * \brief Retrieves memory banks based on start and end pointer.
 * \param[in,out] start Start address of memory range.
 * \param[in,out] end End address of memory range.
 * \param[in] base Base address of memory range.
 * \param[in,out] start_bank Start bank id calculated by this function.
 * \param[in,out] end_bank End bank id calculated by this function.
 */
static void memory_banks_get(void *start, void *end, uint32_t base,
			     uint32_t *start_bank, uint32_t *end_bank)
{
	/* if ptr is not aligned to bank size change it to
	 * closest possible memory address at the start of bank
	 * or end for end address
	 */
	if ((uintptr_t)start % SRAM_BANK_SIZE)
		start = (void *)ALIGN((uintptr_t)start, SRAM_BANK_SIZE);

	if ((uintptr_t)end % SRAM_BANK_SIZE)
		end = (void *)ALIGN_DOWN((uintptr_t)end, SRAM_BANK_SIZE);

	/* return if no full bank could be found for enabled gate control */
	if ((char *)end - (char *)start < SRAM_BANK_SIZE) {
		tr_info(&pm_mem_tr, "cavs_pm_memory_banks_get(): cannot find full bank to perform gating operation");
		return;
	}

	*start_bank = ((uintptr_t)start - base) / SRAM_BANK_SIZE;
	/* Ending bank id has to be lowered by one because it is
	 * calculated from memory end ptr
	 */
	*end_bank = ((uintptr_t)end - base) / SRAM_BANK_SIZE - 1;
}

/**
 * \brief Retrieves register mask for given segment.
 * \param[in] start_bank Start bank id.
 * \param[in] end_bank End bank id.
 * \param[in] segment Segment id.
 * \return Register mask.
 */
static uint32_t cavs_pm_memory_hp_sram_mask_get(uint32_t start_bank,
						uint32_t end_bank, int segment)
{
	uint32_t first_in_segment;
	uint32_t last_in_segment;

	first_in_segment = segment * EBB_SEGMENT_SIZE;
	last_in_segment = ((segment + 1) * EBB_SEGMENT_SIZE) - 1;

	/* not in this segment */
	if (start_bank > last_in_segment || end_bank < first_in_segment)
		return 0;

	if (start_bank >= first_in_segment)
		return MASK(MIN(end_bank, last_in_segment) - first_in_segment,
			    start_bank - first_in_segment);

	return MASK(end_bank - first_in_segment, 0);
}

/**
 * \brief Sets register mask for given segment.
 * \param[in] mask Register mask to be set.
 * \param[in] segment Segment id.
 * \param[in] enabled True if banks should be enabled, false otherwise.
 */
static void cavs_pm_memory_hp_sram_mask_set(uint32_t mask, int segment,
					    bool enabled)
{
	uint32_t expected = enabled ? 0 : mask;

	io_reg_update_bits(SHIM_HSPGCTL(segment), mask, enabled ? 0 : mask);
	io_reg_update_bits(SHIM_HSRMCTL(segment), mask, enabled ? 0 : mask);

	idelay(MEMORY_POWER_DOWN_DELAY);

	while ((io_reg_read(SHIM_HSPGISTS(segment)) & mask) != expected)
		idelay(MEMORY_POWER_DOWN_DELAY);
}

void cavs_pm_memory_hp_sram_banks_power_gate(uint32_t start_bank_id,
					     uint32_t ending_bank_id,
					     bool enabled)
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

void cavs_pm_memory_hp_sram_power_gate(void *ptr, uint32_t size, bool enabled)
{
	uint32_t start_bank = 0;
	uint32_t end_bank = 0;

	memory_banks_get(ptr, (char *)ptr + size, HP_SRAM_BASE, &start_bank,
			 &end_bank);

	cavs_pm_memory_hp_sram_banks_power_gate(start_bank, end_bank, enabled);
}

#if CONFIG_LP_SRAM

void cavs_pm_memory_lp_sram_banks_power_gate(uint32_t start_bank_id,
					     uint32_t ending_bank_id,
					     bool enabled)
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

void cavs_pm_memory_lp_sram_power_gate(void *ptr, uint32_t size, bool enabled)
{
	uint32_t start_bank = 0;
	uint32_t end_bank = 0;

	memory_banks_get(ptr, (char *)ptr + size, LP_SRAM_BASE, &start_bank,
			 &end_bank);

	cavs_pm_memory_lp_sram_banks_power_gate(start_bank, end_bank, enabled);
}

#endif /* CONFIG_LP_SRAM */

#endif /* CAVS_VERSION >= CAVS_VERSION_1_8 */
