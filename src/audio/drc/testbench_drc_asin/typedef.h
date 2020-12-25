// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
/* Convert a float number to fractional Qnx.ny format. Note that there is no
 * check for nx+ny number of bits to fit the word length of int. The parameter
 * qy must be 31 or less.
 */

#include <stdint.h>

#define TEST_VECTOR 21
#define Q_CONVERT_FLOAT(f, qy) \
	((int32_t)(((const double)f) * ((int64_t)1 << (const int)qy) + 0.5))
 /* Fractional multiplication with shift and round
  * Note that the parameters px and py must be cast to (int64_t) if other type.
  */
#define Q_MULTSR_32X32(px, py, qx, qy, qp) \
	((((px) * (py) >> ((qx) + (qy) - (qp) - 1)) + 1) >> 1)
#define q_mult(a, b, qa, qb, qy) ((int32_t)Q_MULTSR_32X32((int64_t)(a), b, qa, qb, qy))
#define q_multq(a, b, q) ((int32_t)Q_MULTSR_32X32((int64_t)(a), b, q, q, q))
#define Q_SHIFT_RND(x, src_q, dst_q) \
	((((x) >> ((src_q) - (dst_q) - 1)) + 1) >> 1)
#define ABS(a) ({		\
	typeof(a) __a = (a);	\
	__a < 0 ? -__a : __a;	\
})
#if !defined(ABS)
#define ABS(A)	({ __typeof__(A) __a = (A); __a < 0 ? -__a : __a; })
#endif
