/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_AUDIO_ENDPOINT__
#define __INCLUDE_AUDIO_ENDPOINT__

#include <stdint.h>
#include <stddef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/audio/component.h>

/*
 * Endpoint component.
 *
 * An endpoint component can either be on the front end or back end of the DSP.
 * FE endpoints are typically DMA buffers used for data Tx/Rx with the host,
 * whilst BE endpoints are typically DMA buffers used for data Tx/Rx with DSP
 * DAIs.
 */ 
struct comp_endpoint {
	/* always at start of component */
	struct comp_object obj;

	/* list */
	struct list_head list;	/* in pipeline list of endpoints */
};
