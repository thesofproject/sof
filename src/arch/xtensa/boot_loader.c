// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <cavs/version.h>
#include <sof/bit.h>
#include <sof/lib/cache.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <user/manifest.h>
#include <config.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_SUECREEK
#define MANIFEST_BASE	BOOT_LDR_MANIFEST_BASE
#else
#define MANIFEST_BASE	IMR_BOOT_LDR_MANIFEST_BASE
#endif

#if CONFIG_BOOT_LOADER
#define MANIFEST_SEGMENT_COUNT 3

/* generic string compare cloned into the bootloader to
 * compact code and make it more readable
 */
int strcmp(const char *s1, const char *s2)
{
	while (*s1 != 0 && *s2 != 0) {
		if (*s1 < *s2)
			return -1;
		if (*s1 > *s2)
			return 1;
		s1++;
		s2++;
	}

	/* did both string end */
	if (*s1 != 0)
		return 1;
	if (*s2 != 0)
		return -1;

	/* match */
	return 0;
}

/* memcopy used by boot loader */
static inline void bmemcpy(void *dest, void *src, size_t bytes)
{
	uint32_t *d = dest;
	uint32_t *s = src;
	int i;

	for (i = 0; i < (bytes >> 2); i++)
		d[i] = s[i];

	dcache_writeback_region(dest, bytes);
}

/* bzero used by bootloader */
static inline void bbzero(void *dest, size_t bytes)
{
	uint32_t *d = dest;
	int i;

	for (i = 0; i < (bytes >> 2); i++)
		d[i] = 0;

	dcache_writeback_region(dest, bytes);
}

static void parse_module(struct sof_man_fw_header *hdr,
	struct sof_man_module *mod)
{
	int i;
	uint32_t bias;

	/* each module has 3 segments */
	for (i = 0; i < MANIFEST_SEGMENT_COUNT; i++) {

		trace_point(TRACE_BOOT_LDR_PARSE_SEGMENT + i);
		switch (mod->segment[i].flags.r.type) {
		case SOF_MAN_SEGMENT_TEXT:
		case SOF_MAN_SEGMENT_DATA:
			bias = (mod->segment[i].file_offset -
				SOF_MAN_ELF_TEXT_OFFSET);

			/* copy from IMR to SRAM */
			bmemcpy((void *)mod->segment[i].v_base_addr,
				(void *)((int)hdr + bias),
				mod->segment[i].flags.r.length *
				HOST_PAGE_SIZE);
			break;
		case SOF_MAN_SEGMENT_BSS:
			/* copy from IMR to SRAM */
			bbzero((void *)mod->segment[i].v_base_addr,
			       mod->segment[i].flags.r.length *
			       HOST_PAGE_SIZE);
			break;
		default:
			/* ignore */
			break;
		}
	}
}

/* On Sue Creek the boot loader is attached separately, no need to skip it */
#if CONFIG_SUECREEK
#define MAN_SKIP_ENTRIES 0
#else
#define MAN_SKIP_ENTRIES 1
#endif

static uint32_t get_fw_size_in_use(void)
{
	struct sof_man_fw_desc *desc =
		(struct sof_man_fw_desc *)MANIFEST_BASE;
	struct sof_man_fw_header *hdr = &desc->header;
	struct sof_man_module *mod;
	uint32_t fw_size_in_use = 0xffffffff;
	int i;

	/* Calculate fw size passed in BASEFW module in MANIFEST */
	for (i = MAN_SKIP_ENTRIES; i < hdr->num_module_entries; i++) {
		trace_point(TRACE_BOOT_LDR_PARSE_MODULE + i);
		mod = (struct sof_man_module *)((char *)desc +
						SOF_MAN_MODULE_OFFSET(i));
		if (strcmp((char *)mod->name, "BASEFW"))
			continue;
		for (i = 0; i < MANIFEST_SEGMENT_COUNT; i++) {
			if (mod->segment[i].flags.r.type
				== SOF_MAN_SEGMENT_BSS) {
				fw_size_in_use = mod->segment[i].v_base_addr
				- HP_SRAM_BASE
				+ (mod->segment[i].flags.r.length
				* HOST_PAGE_SIZE);
			}
		}
	}

	return fw_size_in_use;
}

/* parse FW manifest and copy modules */
static void parse_manifest(void)
{
	struct sof_man_fw_desc *desc =
		(struct sof_man_fw_desc *)MANIFEST_BASE;
	struct sof_man_fw_header *hdr = &desc->header;
	struct sof_man_module *mod;
	int i;

	/* copy module to SRAM  - skip bootloader module */
	for (i = MAN_SKIP_ENTRIES; i < hdr->num_module_entries; i++) {

		trace_point(TRACE_BOOT_LDR_PARSE_MODULE + i);
		mod = (struct sof_man_module *)((char *)desc +
						SOF_MAN_MODULE_OFFSET(i));
		parse_module(hdr, mod);
	}
}
#endif

#if CAVS_VERSION >= CAVS_VERSION_1_8
/* function powers up a number of memory banks provided as an argument and
 * gates remaining memory banks
 */
static int32_t hp_sram_pm_banks(uint32_t banks)
{
	int delay_count = 256;
	uint32_t status;
	uint32_t ebb_mask0, ebb_mask1, ebb_avail_mask0, ebb_avail_mask1;
	uint32_t total_banks_count = PLATFORM_HPSRAM_EBB_COUNT;

	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_HPSRAM_LDO_ON);

	/* add some delay before touch power register */
	idelay(delay_count);

	/* bit masks reflect total number of available EBB (banks) in each
	 * segment; current implementation supports 2 segments 0,1
	 */
	if (total_banks_count > EBB_SEGMENT_SIZE) {
		ebb_avail_mask0 = (uint32_t)MASK(EBB_SEGMENT_SIZE - 1, 0);
		ebb_avail_mask1 = (uint32_t)MASK(total_banks_count -
		EBB_SEGMENT_SIZE - 1, 0);
	} else{
		ebb_avail_mask0 = (uint32_t)MASK(total_banks_count - 1,
		0);
		ebb_avail_mask1 = 0;
	}

	/* bit masks of banks that have to be powered up in each segment */
	if (banks > EBB_SEGMENT_SIZE) {
		ebb_mask0 = (uint32_t)MASK(EBB_SEGMENT_SIZE - 1, 0);
		ebb_mask1 = (uint32_t)MASK(banks - EBB_SEGMENT_SIZE - 1,
		0);
	} else{
		/* assumption that ebb_in_use is > 0 */
		ebb_mask0 = (uint32_t)MASK(banks - 1, 0);
		ebb_mask1 = 0;
	}

	/* HSPGCTL, HSRMCTL use reverse logic - 0 means EBB is power gated */
	io_reg_write(HSPGCTL0, (~ebb_mask0) & ebb_avail_mask0);
	io_reg_write(HSRMCTL0, (~ebb_mask0) & ebb_avail_mask0);
	io_reg_write(HSPGCTL1, (~ebb_mask1) & ebb_avail_mask1);
	io_reg_write(HSRMCTL1, (~ebb_mask1) & ebb_avail_mask1);

	/* query the power status of first part of HP memory */
	/* to check whether it has been powered up. A few    */
	/* cycles are needed for it to be powered up         */
	status = io_reg_read(HSPGISTS0);
	while (status != ((~ebb_mask0) & ebb_avail_mask0)) {
		idelay(delay_count);
		status = io_reg_read(HSPGISTS0);
	}
	/* query the power status of second part of HP memory */
	/* and do as above code                               */

	status = io_reg_read(HSPGISTS1);
	while (status != ((~ebb_mask1) & ebb_avail_mask1)) {
		idelay(delay_count);
		status = io_reg_read(HSPGISTS1);
	}
	/* add some delay before touch power register */
	idelay(delay_count);

	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_HPSRAM_LDO_BYPASS);

	return 0;
}

static uint32_t hp_sram_power_on_memory(uint32_t memory_size)
{
	uint32_t ebb_in_use;

	/* calculate total number of used SRAM banks (EBB)
	 * to power up only necessary banks
	 */
	ebb_in_use = (!(memory_size % SRAM_BANK_SIZE)) ?
	(memory_size / SRAM_BANK_SIZE) :
	(memory_size / SRAM_BANK_SIZE) + 1;

	return hp_sram_pm_banks(ebb_in_use);
}

static int32_t hp_sram_power_off_unused_banks(uint32_t memory_size)
{
	/* keep enabled only memory banks used by FW */
	return hp_sram_power_on_memory(memory_size);
}

static int32_t hp_sram_init(void)
{
	return hp_sram_power_on_memory(HP_SRAM_SIZE);
}

#else

static int32_t hp_sram_power_off_unused_banks(uint32_t memory_size)
{
	return 0;
}

static uint32_t hp_sram_init(void)
{
	return 0;
}

#endif

#if CONFIG_LP_SRAM
static int32_t lp_sram_init(void)
{
	uint32_t status;
	uint32_t lspgctl_value;
	uint32_t timeout_counter, delay_count = 256;

	timeout_counter = delay_count;

	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_LPSRAM_LDO_ON);

	/* add some delay before writing power registers */
	idelay(delay_count);

	lspgctl_value = io_reg_read(LSPGISTS);
	io_reg_write(LSPGCTL, lspgctl_value & ~LPSRAM_MASK(0));

	/* add some delay before checking the status */
	idelay(delay_count);

	/* query the power status of first part of LP memory */
	/* to check whether it has been powered up. A few    */
	/* cycles are needed for it to be powered up         */
	status = io_reg_read(LSPGISTS);
	while (status) {
		if (!timeout_counter--) {
			platform_panic(SOF_IPC_PANIC_MEM);
			break;
		}
		idelay(delay_count);
		status = io_reg_read(LSPGISTS);
	}

	shim_write(SHIM_LDOCTL, SHIM_LDOCTL_LPSRAM_LDO_BYPASS);

	return status;
}
#endif

/* boot master core */
void boot_master_core(void)
{
	int32_t result;

	trace_point(TRACE_BOOT_LDR_ENTRY);

	/* init the HPSRAM */
	trace_point(TRACE_BOOT_LDR_HPSRAM);
	result = hp_sram_init();
	if (result < 0) {
		platform_panic(SOF_IPC_PANIC_MEM);
		return;
	}

#if CONFIG_LP_SRAM
	/* init the LPSRAM */
	trace_point(TRACE_BOOT_LDR_LPSRAM);

	result = lp_sram_init();
	if (result < 0) {
		platform_panic(SOF_IPC_PANIC_MEM);
		return;
	}
#endif

#if CONFIG_BOOT_LOADER
	/* parse manifest and copy modules */
	trace_point(TRACE_BOOT_LDR_MANIFEST);
	parse_manifest();

	hp_sram_power_off_unused_banks(get_fw_size_in_use());
#endif
	/* now call SOF entry */
	trace_point(TRACE_BOOT_LDR_JUMP);
	_ResetVector();
}
