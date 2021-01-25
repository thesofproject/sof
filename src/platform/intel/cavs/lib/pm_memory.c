/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@intel.com>
 */

#include <cavs/lib/pm_memory.h>
#include <cavs/version.h>
#include <sof/common.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#include <stdbool.h>
#include <stdint.h>

/* 14f25ab6-3a4b-4e5d-b343-2a142d4e4d92 */
DECLARE_SOF_UUID("pm-memory", pm_mem_uuid, 0x14f25ab6, 0x3a4b, 0x4e5d,
		 0xb3, 0x43, 0x2a, 0x14, 0x2d, 0x4e, 0x4d, 0x92);

DECLARE_TR_CTX(pm_mem_tr, SOF_UUID(pm_mem_uuid), LOG_LEVEL_INFO);

#if (CAVS_VERSION >= CAVS_VERSION_1_8) || CONFIG_LP_SRAM
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
		start = (void *)ALIGN_UP_COMPILE((uintptr_t)start, SRAM_BANK_SIZE);

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
#endif

#if CAVS_VERSION >= CAVS_VERSION_1_8

void cavs_pm_memory_hp_sram_power_gate(void *ptr, uint32_t size, bool enabled)
{
	uint32_t start_bank = 0;
	uint32_t end_bank = 0;

	memory_banks_get(ptr, (char *)ptr + size, HP_SRAM_BASE, &start_bank,
			 &end_bank);

	cavs_pm_memory_hp_sram_banks_power_gate(start_bank, end_bank, enabled);
}

#endif /* CAVS_VERSION >= CAVS_VERSION_1_8 */

#if CONFIG_LP_SRAM

void cavs_pm_memory_lp_sram_power_gate(void *ptr, uint32_t size, bool enabled)
{
	uint32_t start_bank = 0;
	uint32_t end_bank = 0;

	memory_banks_get(ptr, (char *)ptr + size, LP_SRAM_BASE, &start_bank,
			 &end_bank);

	cavs_pm_memory_lp_sram_banks_power_gate(start_bank, end_bank, enabled);
}

#endif /* CONFIG_LP_SRAM */
