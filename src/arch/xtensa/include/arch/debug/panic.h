/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifdef __SOF_DEBUG_PANIC_H__

#ifndef __ARCH_DEBUG_PANIC_H__
#define __ARCH_DEBUG_PANIC_H__

#include <rtos/cache.h>
#include <ipc/trace.h>
#include <ipc/xtensa.h>
#include <xtensa/config/core-isa.h>
#include <stdint.h>

/* xtensa core specific oops size */
#define ARCH_OOPS_SIZE (sizeof(struct sof_ipc_dsp_oops_xtensa) \
			+ (XCHAL_NUM_AREGS * sizeof(uint32_t)))

void arch_dump_regs_a(void *dump_buf);

static inline void fill_core_dump(struct sof_ipc_dsp_oops_xtensa *oops,
				  uintptr_t stack_ptr, uintptr_t *epc1)
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

	if (epc1)
		oops->epc1 = *epc1;

	/* With crosstool-ng gcc on some platforms this corrupts most of
	 * the other panic information, including the precious line
	 * number. See https://github.com/thesofproject/sof/issues/1346
	 * Commenting this out loses the registers but avoids the
	 * corruption of the rest.
	 */
	arch_dump_regs_a((void *)&oops->exccause);
}

static inline void arch_dump_regs(void *dump_buf, uintptr_t stack_ptr,
				  uintptr_t *epc1)
{
	fill_core_dump(dump_buf, stack_ptr, epc1);

	dcache_writeback_region((__sparse_force void __sparse_cache *)dump_buf, ARCH_OOPS_SIZE);
}

#endif /* __ARCH_DEBUG_PANIC_H__ */

#else

#error "This file shouldn't be included from outside of sof/debug/panic.h"

#endif /* __SOF_DEBUG_PANIC_H__ */
