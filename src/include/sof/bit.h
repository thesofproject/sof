/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_BIT_H__
#define __SOF_BIT_H__

#ifdef __ZEPHYR__
#include <zephyr/sys/util_macro.h>
#else

#if ASSEMBLY
#define BIT(b)			(1 << (b))
#else
#define BIT(b)			(1UL << (b))
#endif

#define SET_BITS(b_hi, b_lo, x)	\
	(((x) & ((1ULL << ((b_hi) - (b_lo) + 1ULL)) - 1ULL)) << (b_lo))
#define GET_BIT(b, x) \
	(((x) & (1ULL << (b))) >> (b))

#endif	/* __ZEPHYR__ */

#define GET_BITS(b_hi, b_lo, x) \
	(((x) & MASK(b_hi, b_lo)) >> (b_lo))

#define MASK(b_hi, b_lo)	\
	(((1ULL << ((b_hi) - (b_lo) + 1ULL)) - 1ULL) << (b_lo))
#define SET_BIT(b, x)		(((x) & 1) << (b))

#endif /* __SOF_BIT_H__ */
