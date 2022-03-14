/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __ACE_MEM_WINDOW_H__
#define __ACE_MEM_WINDOW_H__

#include <sof/bit.h>
#include <stdint.h>

/** \brief Zero memory window during initialization */
#define MEM_WND_INIT_CLEAR		BIT(0)

void platform_memory_windows_init(uint32_t flags);

#endif /*__ACE_MEM_WINDOW_H__ */
