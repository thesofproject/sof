// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Shriram Shastry <malladi.sastry@linux.intel.com>
#include <stdint.h>


/*define macros*/
#define Q_CONVERT_FLOAT(f, qy) \
	((int32_t)(((const double)f) * ((int64_t)1 << (const int)qy) + 0.5))
#define Q_MULTSR_32X32(px, py, qx, qy, qp) \
	((((px) * (py) >> ((qx) + (qy) - (qp) - 1)) + 1) >> 1)
#define q_mult(a, b, qa, qb, qy) ((int32_t)Q_MULTSR_32X32((int64_t)(a), b, qa, qb, qy))
#define Q_SHIFT_RND(x, src_q, dst_q) \
	((((x) >> ((src_q) - (dst_q) - 1)) + 1) >> 1)
/* Alternative version since compiler does not allow (x >> -1) */
#define Q_SHIFT_LEFT(x, src_q, dst_q) ((x) << ((dst_q) - (src_q)))
#define TEST_VECTOR 639
extern void input_data_fixpt(const int32_t x[TEST_VECTOR], int32_t y[TEST_VECTOR]);
extern void data_initialization_fixpt(int32_t x[TEST_VECTOR]);
extern int32_t log10_linear_log[TEST_VECTOR];
