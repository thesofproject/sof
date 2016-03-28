/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __PLATFORM_PMC_H__
#define __PLATFORM_PMC_H__

#include <stdint.h>

/* messages */
#define PMC_DDR_LINK_UP		0xc0	/* LPE req path to DRAM is up */
#define PMC_DDR_LINK_DOWN	0xc1	/* LPE req path to DRAM is down */
#define PMC_SET_LPECLK		0xc2	/* LPE req clock change to FR_LAT_REQ */
#define PMC_SET_SSP_19M2	0xc5	/* LPE req SSP clock to 19.2MHz */
#define PMC_SET_SSP_25M		0xc6	/* LPE req SSP clock to 25MHz  */

int platform_ipc_pmc_init(void);
int ipc_pmc_send_msg(uint32_t message);
int pmc_process_msg_queue(void);

#endif
