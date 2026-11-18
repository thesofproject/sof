/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __ARCH_LIB_WAIT_H__
#define __ARCH_LIB_WAIT_H__

static inline void arch_wait_for_interrupt(int level) {}

static inline void idelay(int n) {}

#endif /* __ARCH_LIB_WAIT_H__ */
