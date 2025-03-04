/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __SOF_LIB_MEMORY_H__
#define __SOF_LIB_MEMORY_H__

#include <platform/lib/memory.h>

#ifndef __cold
#define __cold
#endif

#ifndef __cold_rodata
#define __cold_rodata
#endif

#define assert_can_be_cold() do {} while (0)

#endif /* __SOF_LIB_MEMORY_H__ */
