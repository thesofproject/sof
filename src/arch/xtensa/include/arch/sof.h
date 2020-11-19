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

/* entry point to main firmware */
void _ResetVector(void);

void boot_primary_core(void);

#endif /* __ARCH_SOF_H__ */

#else

#error "This file shouldn't be included from outside of sof/sof.h"

#endif /* __SOF_SOF_H__ */
