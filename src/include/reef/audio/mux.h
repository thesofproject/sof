/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_AUDIO_MUX__
#define __INCLUDE_AUDIO_MUX__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>

/*
 * MUX can switch 1 input to N outputs.
 */
struct comp_mux {
	/* always at start of component */
	struct comp_object obj;

	/* mixer settings */
};

#endif
