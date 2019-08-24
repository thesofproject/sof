/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_ATOMIC_H_
#define __INCLUDE_ATOMIC_H_

#include <atomic.h>

#define atomic_read	atomic_get
#define atomic_init	atomic_set

#endif
