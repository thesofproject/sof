/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * \file arch/xtensa/include/arch/init.h
 * \brief Arch init header file
 * \authors Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_INIT_H__

#ifndef __ARCH_INIT_H__
#define __ARCH_INIT_H__

#include <sof/debug/panic.h>
#include <ipc/trace.h>

#include <xtensa/corebits.h>
#include <xtensa/xtruntime.h>
#include <stddef.h>
#include <stdint.h>

struct sof;

/**
 * \brief Called from assembler context with no return or parameters.
 */
static inline void __memmap_init(void) { }

#if CONFIG_MULTICORE

int secondary_core_init(struct sof *sof);

#else

static inline int secondary_core_init(struct sof *sof) { return 0; }

#endif /* CONFIG_MULTICORE */

#endif /* __ARCH_INIT_H__ */

#else

#error "This file shouldn't be included from outside of sof/init.h"

#endif /* __SOF_INIT_H__ */
