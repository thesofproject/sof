/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#ifdef __SOF_DRIVERS_ALH__

#ifndef __PLATFORM_DRIVERS_ALH__
#define __PLATFORM_DRIVERS_ALH__

#include <stdint.h>

/**
 * \brief ALH Handshakes for audio I/O
 * Stream ID -> DMA Handshake map
 * -1 identifies invalid handshakes/streams
 */
static const uint8_t alh_handshake_map[64] = {
	-1,	/* 0  - INVALID */
	-1,	/* 1  - INVALID */
	-1,	/* 2  - INVALID */
	-1,	/* 3  - INVALID */
	-1,	/* 4  - INVALID */
	-1,	/* 5  - INVALID */
	-1,	/* 6  - INVALID */
	22,	/* 7  - BIDIRECTIONAL */
	23,	/* 8  - BIDIRECTIONAL */
	24,	/* 9  - BIDIRECTIONAL */
	25,	/* 10 - BIDIRECTIONAL */
	-1,	/* 5  - INVALID */
	-1,	/* 6  - INVALID */
	-1,	/* 13 - INVALID */
	-1,	/* 14 - INVALID */
	-1,	/* 15 - INVALID */
	-1,	/* 16 - INVALID */
	-1,	/* 17 - INVALID */
	-1,	/* 18 - INVALID */
	-1,	/* 19 - INVALID */
	-1,	/* 20 - INVALID */
	-1,	/* 21 - INVALID */
	-1,	/* 22 - INVALID */
	32,	/* 23 - BIDIRECTIONAL */
	33,	/* 24 - BIDIRECTIONAL */
	34,	/* 25 - BIDIRECTIONAL */
	35,	/* 26 - BIDIRECTIONAL */
	-1,	/* 5  - INVALID */
	-1,	/* 6  - INVALID */
	-1,	/* 29 - INVALID */
	-1,	/* 30 - INVALID */
	-1,	/* 31 - INVALID */
	-1,	/* 32 - INVALID */
	-1,	/* 33 - INVALID */
	-1,	/* 34 - INVALID */
	-1,	/* 35 - INVALID */
	-1,	/* 36 - INVALID */
	-1,	/* 37 - INVALID */
	-1,	/* 38 - INVALID */
	42,	/* 39 - BIDIRECTIONAL */
	43,	/* 40 - BIDIRECTIONAL */
	44,	/* 41 - BIDIRECTIONAL */
	45,	/* 42 - BIDIRECTIONAL */
	-1,	/* 5  - INVALID */
	-1,	/* 6  - INVALID */
	-1,	/* 45 - INVALID */
	-1,	/* 46 - INVALID */
	-1,	/* 47 - INVALID */
	-1,	/* 48 - INVALID */
	-1,	/* 49 - INVALID */
	-1,	/* 50 - INVALID */
	-1,	/* 51 - INVALID */
	-1,	/* 52 - INVALID */
	-1,	/* 53 - INVALID */
	-1,	/* 54 - INVALID */
	52,	/* 55 - BIDIRECTIONAL */
	53,	/* 56 - BIDIRECTIONAL */
	54,	/* 57 - BIDIRECTIONAL */
	55,	/* 58 - BIDIRECTIONAL */
	-1,	/* 5  - INVALID */
	-1,	/* 6  - INVALID */
	-1,	/* 61 - INVALID */
	-1,	/* 62 - INVALID */
	-1,	/* 63 - INVALID */
};

#define ALH_GPDMA_BURST_LENGTH	4

#endif /* __PLATFORM_DRIVERS_ALH__ */

#else

#error "This file shouldn't be included from outside of sof/drivers/alh.h"

#endif
