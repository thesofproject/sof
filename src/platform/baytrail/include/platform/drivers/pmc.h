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

/* messages */
#define PMC_DDR_LINK_UP		0xc0	/* LPE req path to DRAM is up */
#define PMC_DDR_LINK_DOWN	0xc1	/* LPE req path to DRAM is down */
#define PMC_SET_LPECLK		0xc2	/* LPE req clock change to FR_LAT_REQ */

#if CONFIG_BAYTRAIL

/* LPE req SSP clock to 19.2MHz w/ PLL*/
#define PMC_SET_SSP_19M2	0xc5

/* LPE req SSP clock to 25MHz w/ XTAL */
#define PMC_SET_SSP_25M		0xc6

#elif CONFIG_CHERRYTRAIL

/* LPE req SSP clock to 25MHz w/ PLL */
#define PMC_SET_SSP_25M		0xc5

/* LPE req SSP clock to 19.2MHz w/ XTAL */
#define PMC_SET_SSP_19M2	0xc6
#endif

int platform_ipc_pmc_init(void);
int ipc_pmc_send_msg(uint32_t message);
int pmc_process_msg_queue(void);

#endif /* __PLATFORM_DRIVERS_PMC_H__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/pmc.h"

#endif /* __SOF_DRIVERS_PMC_H__ */
