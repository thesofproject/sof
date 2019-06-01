/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_LIB_PLATFORM_HOST_PMC_H__
#define __INCLUDE_LIB_PLATFORM_HOST_PMC_H__

#include <stdint.h>


int platform_ipc_pmc_init(void);
int ipc_pmc_send_msg(uint32_t message);
int pmc_process_msg_queue(void);

#endif
