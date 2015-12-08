/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_AUDIO_SWITCH__
#define __INCLUDE_AUDIO_SWITCH__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>

/*
 * Switch can disconnect path between source and sink.
 */
struct comp_switch {
	/* always at start of component */
	struct comp_object obj;

	/* mixer settings */
};

#endif
