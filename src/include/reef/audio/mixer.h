/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_AUDIO_MIXER__
#define __INCLUDE_AUDIO_MIXER__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>

/*
 * MIXER can mix multiple inputs to 1 output.
 */
struct comp_mixer {
	/* always at start of component */
	struct comp_object obj;

	/* mixer settings */
};

#endif
