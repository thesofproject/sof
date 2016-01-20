/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

/* Platform timer config */
#define REEF_SYS_TIMER 	0

/* Platform stream capabilities */
#define PLATFORM_MAX_CHANNELS	4
#define PLATFORM_MAX_STREAMS	5

/* Platform Host DMA buffer config - these should align with DMA engine */
#define PLAT_HOST_PERSIZE	256	/* must be multiple of DMA burst size */
#define PLAT_HOST_PERIODS	3 	/* give enough latency for DMA refill */

/* Platform Dev DMA buffer config - these should align with DMA engine */
#define PLAT_DEV_PERSIZE	256	/* must be multiple of DMA+DEV burst size */
#define PLAT_DEV_PERIODS	3 	/* give enough latency for DMA refill */


/*
 * APIs declared here are defined for every platform and IPC mechanism.
 */

int platform_boot_complete(uint32_t boot_message);

int platform_ipc_init(void);

int platform_init(void);

#endif
