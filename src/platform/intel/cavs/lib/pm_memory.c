/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@intel.com>
 */

#include <cavs/version.h>
#include <sof/bit.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/pm_memory.h>
#include <sof/lib/shim.h>
#include <sof/lib/wait.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <stdint.h>

#define trace_mem_pm(__e, ...) \
	trace_event(TRACE_CLASS_MEM, __e, ##__VA_ARGS__)
#if CAVS_VERSION >= CAVS_VERSION_1_8

#define EBB_SEGMENT_SIZE_ZERO_BASE (EBB_SEGMENT_SIZE - 1)
#define MEMORY_POWER_DOWN_DELAY 256
/**
 * \brief Read masks from hw registers and prepare new masks from data
 * \param[in] ebb_data All masks data required for calculations.
 */
static void set_bank_masks(struct ebb_data *ebb)
{
	/* Bit is set for gated banks in the HSPGISTS register so
	 * we negate it to have consistent 1 = powered 0 = gated
	 * convention since HSPGISTS registers have it other way
	 */
	ebb->current_mask0 = ~io_reg_read(HSPGISTS0);
	ebb->current_mask1 = ~io_reg_read(HSPGISTS1);

	/* Calculate mask of bits (banks) to set to powered */
	ebb->change_mask0 = SET_BITS(ebb->ending_bank_id,
		ebb->start_bank_id, MASK(ebb->ending_bank_id, 0));
	ebb->change_mask1 = SET_BITS(ebb->ending_bank_id_high,
		ebb->start_bank_id_high,
		MASK(ebb->ending_bank_id_high, 0));
}

/**
 * \brief Calculates new mask to write to hw register
 * \param[in] ebb_data All masks data required for calculations.
 * \param[in] enabled Boolean deciding banks desired state (1 powered 0 gated).
 */
static void calculate_new_masks(struct ebb_data *ebb, uint32_t enabled)
{
	/* check if either start or end bank is in different segment
	 * and treat it on case by case basis first only banks
	 * the segments above EBB_SEGMENT_SIZE are managed
	 * (separate hw register has to be used for ebb
	 * numbered over 32 different ebb segment)
	 */
	if (ebb->start_bank_id > EBB_SEGMENT_SIZE_ZERO_BASE &&
	    ebb->ending_bank_id > EBB_SEGMENT_SIZE_ZERO_BASE) {
		/* Set the bits for all powered banks operations
		 * depend whether we turn on or off the memory
		 */
		ebb->new_mask0 =
			ebb->current_mask0;
		ebb->new_mask1 = enabled ?
			ebb->current_mask1
			| ebb->change_mask1 :
			ebb->current_mask1
			& ~ebb->change_mask1;

	/* second check if managing spans above and below EBB_SEGMENT_SIZE
	 * meaning we have to handle both ebb for both segments
	 */
	} else if (ebb->start_bank_id > EBB_SEGMENT_SIZE_ZERO_BASE
		   && ebb->ending_bank_id
		   < EBB_SEGMENT_SIZE_ZERO_BASE) {
		ebb->new_mask0 = enabled ?
			ebb->current_mask0
			| ebb->change_mask0 :
			ebb->current_mask0
			& ~ebb->change_mask0;
		ebb->new_mask1 = enabled ?
			ebb->current_mask1
			| ebb->change_mask1 :
			ebb->current_mask1
			& ~ebb->change_mask1;

	/* if only first segment needs changes */
	} else {
		ebb->new_mask0 = enabled ?
			ebb->current_mask0
			| ebb->change_mask0 :
			ebb->current_mask0
			& ~ebb->change_mask0;
		ebb->new_mask1 =
			ebb->current_mask1;
	}
}

/**
 * \brief Write calculated masks to HW registers
 * \param[in] ebb_data All masks data with masks to write.
 */
static void write_new_masks_and_check_status(struct ebb_data *ebb)
{
	uint32_t status;

	/* HSPGCTL, HSRMCTL use reverse logic - 0 means EBB is enabled gated */
	io_reg_write(HSPGCTL0, ~ebb->new_mask0);
	io_reg_write(HSRMCTL0, ~ebb->new_mask0);
	io_reg_write(HSPGCTL1, ~ebb->new_mask1);
	io_reg_write(HSRMCTL1, ~ebb->new_mask1);

	/* Query the enabled status of first part of HP memory
	 * to check whether it has been powered up. A few
	 * cycles are needed for it to be powered up
	 */
	status = io_reg_read(HSPGISTS0);
	while (status != ~ebb->new_mask0) {
		idelay(MEMORY_POWER_DOWN_DELAY);
		status = io_reg_read(HSPGISTS0);
	}

	/* Query the enabled status of second part of HP memory */
	status = io_reg_read(HSPGISTS1);
	while (status != ~ebb->new_mask1) {
		idelay(MEMORY_POWER_DOWN_DELAY);
		status = io_reg_read(HSPGISTS1);
	}
	/* add some delay before touch enabled register */
	idelay(MEMORY_POWER_DOWN_DELAY);
}

/**
 * \brief Set memory enabled gating hw bit mask for memory banks
 * \param[in] start_bank_id Id of first bank to be managed (inclusive) 0 based.
 * \param[in] ending_bank_id Id of last bank to be managed (inclusive) 0 based.
 * \param[in] enabled Boolean deciding banks desired state (1 powered 0 gated).
 */
static void set_banks_gating(uint32_t start_bank_id, uint32_t ending_bank_id,
			     uint32_t enabled)
{
	struct ebb_data ebb = {0};

	ebb.start_bank_id = start_bank_id;
	ebb.ending_bank_id = ending_bank_id;
	ebb.start_bank_id_high = start_bank_id - EBB_SEGMENT_SIZE;
	ebb.ending_bank_id_high = ending_bank_id - EBB_SEGMENT_SIZE;

	/* Take note that if there are more banks than EBB_SEGMENT_SIZE
	 * then those over have to be controlled by second mask
	 * hence start_bank_id and start_bank_id_high
	 * for calculating the banks in second segment (currently two
	 * segments is all HW supports)
	 */
	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_HPSRAM_LDO_ON);

	set_bank_masks(&ebb);

	calculate_new_masks(&ebb, enabled);

	write_new_masks_and_check_status(&ebb);

	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_HPSRAM_LDO_BYPASS);
}

void set_power_gate_for_memory_address_range(void *ptr,
					     uint32_t size, uint32_t enabled)
{
	uint32_t start_bank_id;
	uint32_t ending_bank_id;
	void *end_ptr = (char *)ptr + size;

	/* if ptr is not aligned to bank size change it to
	 * closest possible memory address at the start of bank
	 * or end for end address
	 */
	if ((uintptr_t)ptr % SRAM_BANK_SIZE)
		ptr = (void *)ALIGN((uintptr_t)ptr, SRAM_BANK_SIZE);

	if ((uintptr_t)end_ptr % SRAM_BANK_SIZE)
		end_ptr = (void *)ALIGN_DOWN((uintptr_t)ptr, SRAM_BANK_SIZE);

	/* return if no full bank could be found for enabled gate control */
	if ((char *)end_ptr - (char *)ptr < SRAM_BANK_SIZE) {
		trace_mem_pm("Could not find full bank "
			"to perform gating operation");
		return;
	}

	start_bank_id = ((uintptr_t)ptr - HP_SRAM_BASE) / SRAM_BANK_SIZE;
	/* Ending bank id has to be lowered by one because it is
	 * calculated from memory end ptr
	 */
	ending_bank_id = (((uintptr_t)end_ptr - HP_SRAM_BASE)
		/ SRAM_BANK_SIZE - 1);

	set_banks_gating(start_bank_id, ending_bank_id, enabled);
}

#else
void set_power_gate_for_memory_address_range(void *ptr,
					     uint32_t size, uint32_t enabled)
{
}
#endif
