/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifdef __SOF_DRIVERS_PMC_H__

#ifndef __PLATFORM_DRIVERS_PMC_H__
#define __PLATFORM_DRIVERS_PMC_H__

#include <stdint.h>

int platform_ipc_pmc_init(void);
int ipc_pmc_send_msg(uint32_t message);
int pmc_process_msg_queue(void);

#endif /* __PLATFORM_DRIVERS_PMC_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/pmc.h"

#endif /* __SOF_DRIVERS_PMC_H__ */
