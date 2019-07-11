/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_WAIT_H__

#ifndef __ARCH_WAIT_H__
#define __ARCH_WAIT_H__

static inline void arch_wait_for_interrupt(int level) {}

static inline void idelay(int n) {}

#endif /* __ARCH_WAIT_H__ */

#else

#error "This file shouldn't be included from outside of sof/wait.h"

#endif /* __SOF_WAIT_H__ */
