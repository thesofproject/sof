/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#define REEF_SYS_TIMER 	0

#define PLATFORM_MAX_CHANNELS	4
#define PLATFORM_MAX_STREAMS	5

struct ipc;

/*
 * APIs declared here are defined for every platform and IPC mechanism.
 */

int platform_boot_complete(uint32_t boot_message);

int platform_ipc_init(struct ipc *context);

int platform_init(int argc, char *argv[]);

#endif
