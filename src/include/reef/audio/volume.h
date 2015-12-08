/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_AUDIO_VOLUME__
#define __INCLUDE_AUDIO_VOLUME__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>

/*
 * PGA - programmable gain amplifier.
 */
struct comp_volume {
	/* always at start of component */
	struct comp_object obj;

	/* volume settings */
	uint32_t gain;
};

#endif
