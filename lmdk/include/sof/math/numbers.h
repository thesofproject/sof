/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/*
 * LMDK-local copy of <sof/math/numbers.h>.
 *
 * Loadable modules must not pull in the whole SOF src/include tree. This file
 * mirrors the parts of the SOF header that the sink/source module API relies on
 * (mainly the ROUND_UP/ROUND_DOWN/MIN/MAX helpers) so that the unchanged
 * "#include <sof/math/numbers.h>" resolves inside the LMDK build without
 * exposing the rest of the SOF headers.
 */

#ifndef __SOF_MATH_NUMBERS_H__
#define __SOF_MATH_NUMBERS_H__

#include <stdint.h>

/* Unsafe and portable macros for consistency with Zephyr.
 * See SEI CERT-C PRE31-C
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#define ROUND_DOWN(size, alignment) ({					\
	__typeof__(size) __size = (size);				\
	__typeof__(alignment) __alignment = (alignment);		\
	__size - (__size % __alignment);				\
})

#define ROUND_UP(size, alignment) ({					\
	__typeof__(size) __size = (size);				\
	__typeof__(alignment) __alignment = (alignment);		\
	((__size + __alignment - 1) / __alignment) * __alignment;	\
})

#define ABS(a) ({			\
	__typeof__(a) __a = (a);	\
	__a < 0 ? -__a : __a;		\
})
#define SGN(a) ({			\
	__typeof__(a) __a = (a);	\
	__a < 0 ? -1 :			\
	__a > 0 ? 1 : 0;		\
})

#endif /* __SOF_MATH_NUMBERS_H__ */
