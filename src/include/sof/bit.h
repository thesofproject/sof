/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_BIT_H__
#define __SOF_BIT_H__

/* Zephyr defines this - remove local copy once Zephyr integration complete */
#ifdef BIT
#undef BIT
#endif

#if ASSEMBLY
#define BIT(b)			(1 << (b))
#else
#define BIT(b)			(1UL << (b))
#endif

#define MASK(b_hi, b_lo)	\
	(((1ULL << ((b_hi) - (b_lo) + 1ULL)) - 1ULL) << (b_lo))
#define SET_BIT(b, x)		(((x) & 1) << (b))
#define SET_BITS(b_hi, b_lo, x)	\
	(((x) & ((1ULL << ((b_hi) - (b_lo) + 1ULL)) - 1ULL)) << (b_lo))

#endif /* __SOF_BIT_H__ */
