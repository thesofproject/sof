/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

#ifdef __SOF_SOF_H__

#ifndef __ARCH_SOF_H__
#define __ARCH_SOF_H__

#include <sof/cache.h>
#include <sof/mailbox.h>
#include <ipc/xtensa.h>
#include <xtensa/config/core-isa.h>
#include <stdint.h>

/* architecture specific stack frames to dump */
#define ARCH_STACK_DUMP_FRAMES		32

/* xtensa core specific oops size */
#define ARCH_OOPS_SIZE (sizeof(struct sof_ipc_dsp_oops_xtensa) \
			+ (XCHAL_NUM_AREGS * sizeof(uint32_t)))

/* entry point to main firmware */
void _ResetVector(void);

void boot_master_core(void);

void arch_dump_regs_a(void *dump_buf, uint32_t ps);

static inline void *arch_get_stack_ptr(void)
{
	void *ptr;

	/* stack pointer is in a1 */
	__asm__ __volatile__ ("mov %0, a1"
		: "=a" (ptr)
		:
		: "memory");
	return ptr;
}

static inline void fill_core_dump(struct sof_ipc_dsp_oops_xtensa *oops,
				  uint32_t ps, uintptr_t stack_ptr,
				  uintptr_t *epc1)
{
	oops->arch_hdr.arch = ARCHITECTURE_ID;
	oops->arch_hdr.totalsize = ARCH_OOPS_SIZE;
#if XCHAL_HW_CONFIGID_RELIABLE
	oops->plat_hdr.configidhi = XCHAL_HW_CONFIGID0;
	oops->plat_hdr.configidlo = XCHAL_HW_CONFIGID1;
#else
	oops->plat_hdr.configidhi = 0;
	oops->plat_hdr.configidlo = 0;
#endif
	oops->plat_hdr.numaregs = XCHAL_NUM_AREGS;
	oops->plat_hdr.stackoffset = oops->arch_hdr.totalsize
				     + sizeof(struct sof_ipc_panic_info);
	oops->plat_hdr.stackptr = stack_ptr;

	oops->epc1 = *epc1;

	arch_dump_regs_a((void *)&oops->exccause, ps);
}

static inline void arch_dump_regs(uint32_t ps, uintptr_t stack_ptr,
				  uintptr_t *epc1)
{
	void *buf = (void *)mailbox_get_exception_base();

	fill_core_dump(buf, ps, stack_ptr, epc1);

	dcache_writeback_region(buf, ARCH_OOPS_SIZE);
}

#endif /* __ARCH_SOF_H__ */

#else

#error "This file shouldn't be included from outside of sof/sof.h"

#endif /* __SOF_SOF_H__ */
