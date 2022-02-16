/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __ACE_LPS_CTX_H__
#define __ACE_LPS_CTX_H__

#include <xtensa/xtruntime-frames.h>

STRUCT_BEGIN
STRUCT_FIELD(void*, 4, lps_ctx_, vector_level_2)
STRUCT_FIELD(void*, 4, lps_ctx_, vector_level_3)
STRUCT_FIELD(void*, 4, lps_ctx_, vector_level_4)
STRUCT_FIELD(void*, 4, lps_ctx_, vector_level_5)
STRUCT_FIELD(long, 4, lps_ctx_, intenable)
STRUCT_FIELD(long, 4, lps_ctx_, memmap_vecbase_reset)
STRUCT_FIELD(long, 4, lps_ctx_, threadptr)
STRUCT_FIELD(void*, 4, lps_ctx_, task_ctx)
STRUCT_END(lps_ctx)

#endif /*__ACE_LPS_CTX_H__ */
