// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>

#include <ace/version.h>
#include <ace/lib/pm_memory.h>
#include <sof/bit.h>
#include <sof/lib/cache.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/lib/shim.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <ipc/trace.h>
#include <rimage/sof/user/manifest.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MANIFEST_BASE	IMR_BOOT_LDR_MANIFEST_BASE
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

/* MTL bootloader is at position 0, actual modules start at position 1 */
#define MAN_SKIP_ENTRIES 1

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

#if PLATFORM_MEM_INIT_AT_BOOT
static void hp_sram_power_memory_ace (void)
{
	volatile struct HPSRAM_REGS *hpsram_regs =
		(volatile struct HPSRAM_REGS * const)(CONFIG_ADSP_L2HSBxPM_ADDRESS(0));

	int i;
	for (i = 0; i < CONFIG_HP_MEMORY_BANKS; i++) {
		hpsram_regs[i].power_gating_control.part.l2lmpge = 0;
	}

	for (i = 0; i < CONFIG_HP_MEMORY_BANKS; i++) {
		while (hpsram_regs[i].power_gating_status.part.l2lmpgis) {
			// TODO: verify timeout
		}
	}
}

#if 0 // TODO - to be enabled for ACE
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

static int32_t hp_sram_power_off_unused_banks(uint32_t memory_size)
{
	/* keep enabled only memory banks used by FW */
	return hp_sram_power_memory_cavs(memory_size, false);
}
#endif // 0

static int32_t hp_sram_init(void)
{
	hp_sram_power_memory_ace();
	return 0;
}
#endif // PLATFORM_MEM_INIT_AT_BOOT

/* boot primary core */
void boot_primary_core(void)
{
#if PLATFORM_MEM_INIT_AT_BOOT
	int32_t result;
#endif /* PLATFORM_MEM_INIT_AT_BOOT */

	trace_point(TRACE_BOOT_LDR_ENTRY);

#if PLATFORM_MEM_INIT_AT_BOOT
	/* init the HPSRAM */
	trace_point(TRACE_BOOT_LDR_HPSRAM);
	result = hp_sram_init();
	if (result < 0) {
		platform_panic(SOF_IPC_PANIC_MEM);
		return;
	}
#endif /* PLATFORM_MEM_INIT_AT_BOOT */

#if CONFIG_LP_SRAM
	/* init the LPSRAM */
	trace_point(TRACE_BOOT_LDR_LPSRAM);

	cavs_pm_memory_lp_sram_banks_power_gate(0,
						PLATFORM_LPSRAM_EBB_COUNT - 1,
						true);
#endif /* CONFIG_LP_SRAM */

#if CONFIG_L1_DRAM
	/* Power ON L1 DRAM memory */
	trace_point(TRACE_BOOT_LDR_L1DRAM);
	cavs_pm_memory_l1_dram_banks_power_gate(CONFIG_L1_DRAM_MEMORY_BANKS - 1,
						0, true);
#endif /* CONFIG_L1_DRAM */

	/* parse manifest and copy modules */
	trace_point(TRACE_BOOT_LDR_MANIFEST);
	parse_manifest();

#if PLATFORM_MEM_INIT_AT_BOOT
#if 0 // TODO - to be enabled for MTL
	hp_sram_power_off_unused_banks(get_fw_size_in_use());
#endif // 0
#endif // PLATFORM_MEM_INIT_AT_BOOT

	/* now call SOF entry */
	trace_point(TRACE_BOOT_LDR_JUMP);
	_ResetVector();
}
